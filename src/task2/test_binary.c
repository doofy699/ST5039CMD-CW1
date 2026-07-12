#include <stdio.h>
#include <unistd.h>

int main()
{
    printf("Untrusted program started...\n");

    while (1)
    {
        printf("Running...\n");
        sleep(1);
    }

    return 0;
}
