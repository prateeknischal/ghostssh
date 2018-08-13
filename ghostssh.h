#ifndef GHOSTSSH_H
#define GHOSTSSH_H

#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libssh2.h>
#include <pthread.h>

#include "tlog.h"

#define BUFF_SIZE   0xff
#define SLEEP_TIME  5

/*
ssh_host stores the context information for a session to a
particular IP address.
char *ip_addr - String representation of the IP address
int fd - The file descriptor to which the output of the
    command will be written using write() syscall
int sock - The socket descriptor on which the SSH connection
    is eshtablised
LIBSSH2_SESSION *session - Pointer to libssh2 session that
    keeps the session
LIBSSH2_CHANNEL *channel - Pointer to libssh2 channel that
    hold the pointer to the current channel to for the
    session

sockaddr_in sin - stores the information that is used to
    initialise the socket
*/
struct ssh_host {
    //Requires initialization
    char *ip_addr;
    int fd;

    //Will be initialized automatically
    int sock;

    LIBSSH2_SESSION *session;
    LIBSSH2_CHANNEL *channel;

    struct sockaddr_in sin;
};

/*
job_queue struct stores the list of all ssh_host instances,
i.e. all session and can be used as list of session to connect
to
int q_idx - current index in the queue
int q_size - total size of the queue
ssh_host *hosts - pointer to the array that stores all the contexts
*/
struct job_queue {
    int q_idx;
    int q_size;
    struct ssh_host *hosts;
};

/*
credentials struct stores the authentication information
i.e. the username and password to connect to all the servers
*/
struct credentials {
  char *username;
  char *password;
};

struct credentials *creds;

/*
pthread mutex object that is used to synchronize the job_queue
when used by multiple threads
*/
pthread_mutex_t lock;


int   ghostssh_init();
int   ghostssh_get_channel(struct ssh_host *ctx);
int   ghostssh_setup(struct ssh_host *ctx, char *host_ip);
void  ghostssh_cleanup(struct ssh_host *ctx);
void  ghostssh_get_next(struct job_queue *q, struct ssh_host **next);
int   ghostssh_thread_create(void *(*run)(void *), struct job_queue *q);


/*
Signal handler for when SIGINT is sent to the process
as it is running in an infinite loop. It will just call
for cleanup for libssh2
*/
void ghostssh_sigint_handler(int signo)
{
    if (signo == SIGINT){
        tlog(stderr, "Recieved SIGINT, stopping and beginning cleanup\n");
        libssh2_exit();
        tlog(stderr, "Cleanup done\n");
        exit(0);
    }
}

/*
ghostssh_init will initialise the libssh2 library and setup up credentials for
use during setting up connections. It also registers the SIGINT handler
*/
int ghostssh_init(const char *username , const char *password) {
    int rc;

    creds = (struct credentials*) malloc(sizeof(struct credentials));
    creds->username = strdup(username);
    creds->password = strdup(password);

    if (signal(SIGINT, ghostssh_sigint_handler) == SIG_ERR) {
        tlog(stderr, "Failed to register SIGINT Handler");
    }

    rc = libssh2_init(0);
    if (rc != 0) {
        tlog(stderr,
            "Failed to init libssh2: libssh2_init error:%d\n",
            rc);
        return rc;
    }
    return 0;
}

/*
ghostssh_get_channel will return a channel over an existing session, if
the session does not exists, it will return with error code -1
*/
int ghostssh_get_channel(struct ssh_host *ctx) {
    if (ctx->session == NULL) {
        tlog(stderr, "Session does not exists\n");
        return -1;
    }
    ctx->channel = libssh2_channel_open_session(ctx->session);
    if (ctx->channel == NULL) {
        tlog(stderr,
            "%s - Failed to open channel; libssh2_channel_open_session error\n", ctx->ip_addr);
        return -1;
    }
    tlog(stderr, "%s - Open channel successful\n", ctx->ip_addr);
    return 0;
}

/*
ghostssh_setup will create everything that is needed to eshtablish an ssh session
This includes, creating a socket connection, completing the handshake and authenticating
with the remote server.
*/
// TODO - check if the session is up or not, if not
// create another session
int ghostssh_setup(struct ssh_host *ctx, char *host_ip) {
    int rc;
    unsigned long host_addr = inet_addr(host_ip);

    if (host_ip == NULL) {
        tlog(stderr, "Host IP address is not initialized\n");
        return -1;
    }

    ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
    ctx->sin.sin_family = AF_INET;
    ctx->sin.sin_port = htons(22);
    ctx->sin.sin_addr.s_addr = host_addr;

    rc = connect(ctx->sock, (struct sockaddr*)(&ctx->sin), sizeof(struct sockaddr_in));
    if (rc != 0) {
        tlog(stderr,
            "%s - Failed to init the socket connection: connect error: %d\n",
            ctx->ip_addr, rc);
        return rc;
    }
    tlog(stderr, "%s - Socket connection init success\n", ctx->ip_addr);

    ctx->session = libssh2_session_init();

    // TODO - server verification, handshake cipher options
    rc = libssh2_session_handshake(ctx->session, ctx->sock);
    if (rc != 0) {
        tlog(stderr,
            "%s - Failure to eshtablish ssh: libssh2_session_handshake error: %d\n",
            ctx->ip_addr, rc);
        return -1;
    }
    tlog(stderr, "%s - Handshake successful\n", ctx->ip_addr);

    rc = libssh2_userauth_password(ctx->session, creds->username, creds->password);

    if (rc != 0) {
        tlog(stderr,
            "%s - Failed to authenticate; libssh2_userauth_password error: %d\n",
            ctx->ip_addr, rc);
        return rc;
    }

    tlog(stderr, "%s - Authenticaton successful\n", ctx->ip_addr);
    return 0;
}

/*
ghostssh_run_command takes a command and sends it over to the remote machine over
and eshtablished session. It will request for a channel over a session and send the
commands over. It will then read the output of the command. current buffer size for
simplicity purposes is ony BUFF_SIZE = 0xff.

This is some specific code as it will output the response in the following format
[TIME] | IP | type | <output> to what ever the output file descriptor is present in
the context structure

Once all the data is read from the channel it will close it gracefully by sending EOF
and then freeing up the resource.
*/
int ghostssh_run_command(struct ssh_host *ctx, const char *cmd, const char *type) {
    int rc;
    char buff[BUFF_SIZE];

    // create a new channel to run commands under the same session
    rc = ghostssh_get_channel(ctx);
    if (rc != 0) {
        tlog(stderr,
            "%s - Failed to open channel; get_channel error: %d\n",
            ctx->ip_addr, rc);
        return rc;
    }

    char *curr_time = get_time();

    rc = libssh2_channel_exec(ctx->channel, cmd);

    switch (rc) {
        case LIBSSH2_ERROR_ALLOC:
            tlog(stderr,
                "%s - LIBSSH2_ERROR_ALLOC - Internal error - \n",
                ctx->ip_addr);
            return rc;

        case LIBSSH2_ERROR_SOCKET_SEND:
            tlog(stderr,
                "%s - LIBSSH_ERROR_SOCKET_SEND - Failed to send data on socket\n",
                ctx->ip_addr);
            return rc;

        case LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED:
            tlog(stderr,
                "%s - LIBSSH2_ERROR_CHANNEL_REQUEST_DENIED - Channel request denied\n",
                ctx->ip_addr);
            return rc;

        case LIBSSH2_ERROR_EAGAIN:
            tlog(stderr,
                "%s - LIBSSH2_ERROR_EAGAIN - Failed to execute command on remote host\n",
                ctx->ip_addr);
            return rc;

        case LIBSSH2_ERROR_NONE:
            tlog(stderr, "%s - Command execution successful\n",
                ctx->ip_addr);

            rc = libssh2_channel_read(ctx->channel, buff, sizeof(buff));
            if (rc < 0) {
                tlog(stderr,
                    "%s - Failed to read from channel: %d\n",
                    ctx->ip_addr, rc);
                return rc;
            }

            pthread_mutex_lock(&log_lock);
            write(ctx->fd, curr_time, strlen(curr_time));
            write(ctx->fd, " | ", 3);
            write(ctx->fd, ctx->ip_addr, strlen(ctx->ip_addr));
            write(ctx->fd, " | ", 3);
            write(ctx->fd, type, strlen(type));
            write(ctx->fd, " | ", 3);

            while (rc > 0) {
                // Check if the write is equal to the number of bytes read from
                // channel
                if (write(ctx->fd, buff, rc) != rc) {
                    tlog(stderr,
                        "%s - Failed to write output to %d\n",
                        ctx->ip_addr, rc);
                    return -1;
                }
                rc = libssh2_channel_read(ctx->channel, buff, sizeof(buff));
            }
            pthread_mutex_unlock(&log_lock);
            break;
        default:
            tlog(stderr,
                "%s - Unknown return code from libssh2_channel_exec: %d\n",
                ctx->ip_addr, rc);
            return rc;
    }

    if (ctx->channel) {
        libssh2_channel_eof(ctx->channel);
        libssh2_channel_close(ctx->channel);
        libssh2_channel_free(ctx->channel);
    }
    return LIBSSH2_ERROR_NONE;
}

/*
ghostssh_cleanup will free up all resources taken up for the ssh
connection by a particular context
*/
void ghostssh_cleanup(struct ssh_host *ctx) {
    if (ctx->channel) {
        libssh2_channel_eof(ctx->channel);
        libssh2_channel_close(ctx->channel);
        libssh2_channel_free(ctx->channel);
    }

    if (ctx->session) {
        libssh2_session_disconnect(ctx->session, "Normal shutdown");
        libssh2_session_free(ctx->session);
    }

    if (ctx->sock) {
        close(ctx->sock);
    }
}


/*
ghostssh_get_next will return the next context in the hosts array, i.e.
the next session from the list of session and wraps around when at then
end like a circular list.

This is meant to be thread safe as it is protected by a mutex. the state of
index of the queue is maintained in the job_queue itself.
*/
void ghostssh_get_next(struct job_queue *q, struct ssh_host **next) {
    pthread_mutex_lock(&lock);
    q->q_idx = q->q_idx % q->q_size;
    *next = q->hosts + q->q_idx;
    q->q_idx = (q->q_idx + 1) %q-> q_size;
    pthread_mutex_unlock(&lock);
}

/*
ghostssh_thread_create will create threads with count equal to the number of
hosts to which ssh connection is required. It will use the provided run function
and arguments mimicing the pthread_create() call in case of arguments.

once all the threads are created it will attach them to the main process.
*/
int ghostssh_thread_create(void *(*run)(void *), struct job_queue *q) {
    int rc;
    pthread_t *thread = (pthread_t *)malloc(q->q_size * sizeof(pthread_t));

    if ((rc = pthread_mutex_init(&lock, NULL)) != 0) {
        tlog(stderr, "Failed to init mutex; error code: pthread_mutex_init: %d\n", rc);
        return rc;
    }

    for (int i = 0; i < q->q_size; i++) {
        pthread_create(&thread[i], NULL, run, (void *)q);
        tlog(stderr, "Created thread %d\n", i);
    }

    for (int i = 0; i < q->q_size; i++) {
        pthread_join(thread[i], NULL);
    }
    return 0;
}

#endif
