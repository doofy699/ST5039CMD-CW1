#include <stdio.h>

int main(void)
{
    printf("[cpu_hog] starting infinite CPU-bound loop\n");

    volatile unsigned long i = 0;
    while (1)
    {
        i++;
    }

    return 0;
}
