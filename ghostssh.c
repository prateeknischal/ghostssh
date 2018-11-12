#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>

#ifndef GHOSTSSH_H
#include "ghostssh.h"
#endif

//const char *cpu =   "python -c \"import os; print os.getloadavg()[2]\"";
const char *cpu =   "top -bn1|grep \"Cpu(s)\"|sed \"s/.*, *\([0-9.]*\)%* id.*/\1/\" |awk '{print (100 - $1)/100}'"
const char *mem =   "cat /proc/meminfo | head -n2 | tr -d ' ' "
                    "| python -c 'import sys; t=[x.strip(\"\\nkB \").split(\":\")[-1]"
                    "for x in sys.stdin.readlines()];print \"%.3f\"%(float(t[1])/float(t[0]))'";


void *run(void *q) {
    int rc;
    struct ssh_host *next;
    while (1) {
        next = NULL;
        ghostssh_get_next(q, &next);

        if (next == NULL) {
            tlog(stderr, "Failed to get ssh_host from queue; stopping thread\n");
            break;
        }

        // Check if the session exists or not
        if (next->session == NULL) {
            // if the session does not exists then create one
            // call setup(ssh_host, ip) on it
            if (ghostssh_setup(next, next->ip_addr) != 0) {
                tlog(stderr, "Failed to initialize connection; ip: %s\n", next->ip_addr);
                continue;
            }
        }

        // Get CPU metrics
        rc = ghostssh_run_command(next, cpu, "CPU_LOAD");
        if (rc != 0) {
            tlog(stderr, "%s - Failed to get CPU metrics; error: %d\n", next->ip_addr, rc);
        }

        // Get Memory metrics
        rc = ghostssh_run_command(next, mem, "MEM_LOAD");
        if (rc != 0) {
            tlog(stderr, "%s - Failed to get MEM metrics; error: %d\n", next->ip_addr, rc);
        }

        // sleep before next run
        sleep(SLEEP_TIME);
    }
    return NULL;
}

#if defined(GHOSTSSH_USERNAME) && defined(GHOSTSSH_PASSWORD)
int main() {
#else
int main(int argc, char **argv){
#endif
    int rc, _q_size = 0;
    // 255.255.255.255\0 max length:16
    char buff[16];
    // Support max of 255 IPs
    char *ip[0xff];
    /*
    Read the list of IPs
    */
    FILE *f = fopen("ip.txt", "r");
    while (fscanf(f, "%s", buff) != EOF) {
        ip[_q_size] = strdup(buff);
        ++_q_size;
    }

#if defined(GHOSTSSH_USERNAME) && defined(GHOSTSSH_PASSWORD)
    ghostssh_init(GHOSTSSH_USERNAME, GHOSTSSH_PASSWORD);
#else
    if (argc < 2) {
        tlog(stdout, "usage : ./ghostssh username\n");
        exit(1);
    }

    char *password = getpass("Password: ");
    ghostssh_init(argv[1], password);
#endif

    // Create the job queue
    struct job_queue *q = (struct job_queue *) malloc(sizeof(struct job_queue));
    q->q_idx = 0;
    q->q_size = _q_size;

    q->hosts = (struct ssh_host *)malloc(q->q_size * sizeof(struct ssh_host));
    for (int i = 0; i < q->q_size; i++) {
        // Copying all the ip address for setup()
        q->hosts[i].ip_addr = ip[i];
        q->hosts[i].fd = 1;
        q->hosts[i].session = NULL;
        q->hosts[i].channel = NULL;
    }

    rc = ghostssh_thread_create(run, q);
    if (rc != 0) {
      tlog(stderr, "Failed to start ghostssh\n");
      exit(rc);
    }

    return 0;
}
