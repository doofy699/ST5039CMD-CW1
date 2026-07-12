#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("[benign] starting, will exit after 1 second\n");
    sleep(1);
    printf("[benign] exiting normally\n");
    return 0;
}
