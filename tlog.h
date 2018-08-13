#include <stdarg.h>
#include <pthread.h>

pthread_mutex_t log_lock;
int lock_init = 0;

time_t rawtime;
struct tm *timeinfo;

char* get_time() {
    time (&rawtime);
    timeinfo = localtime(&rawtime);
    char *t = (char *) malloc(30 * sizeof(char));
    strftime(t, 30,"[%F %X]",timeinfo);
    return t;
}

void tlog(FILE *fp, const char *fmt, ...) {
    if (lock_init == 0) {
      if (pthread_mutex_init(&log_lock, NULL) != 0) {
          tlog(stderr, "Failed to init log mutex; error code: pthread_mutex_init: \n");
          return;
      }
      lock_init = 1;
    }

    va_list args;
    va_start(args, fmt);

    // print anything other than output to stderr
    pthread_mutex_lock(&log_lock);
    fprintf(stderr, "%s ", get_time());
    vfprintf(stderr, fmt, args);
    pthread_mutex_unlock(&log_lock);
    va_end(args);
}
