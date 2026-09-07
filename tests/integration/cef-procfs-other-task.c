#include <pthread.h>
#include <signal.h>
#include <unistd.h>

static void *wait_forever(void *opaque)
{
    (void)opaque;
    for (;;) {
        pause();
    }
    return NULL;
}

int main(void)
{
    pthread_t thread;

    if (pthread_create(&thread, NULL, wait_forever, NULL) != 0) {
        return 1;
    }

    raise(SIGSTOP);
    return 1;
}
