#include <stdio.h>
#include <signal.h>
#include <unistd.h>

static void handle_sigterm(int sig)
{
    (void)sig;
    printf("[ignore_sigterm] received SIGTERM, ignoring it\n");
}

int main(void)
{
    signal(SIGTERM, handle_sigterm);
    printf("[ignore_sigterm] starting, will ignore SIGTERM and loop forever\n");

    while (1)
    {
        sleep(1);
    }

    return 0;
}
