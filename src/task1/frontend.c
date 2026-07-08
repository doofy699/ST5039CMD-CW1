#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"

int main()
{
    int client_fd;
    struct sockaddr_un addr;
    LoginRequest request;
    char response[RESPONSE_SIZE];

    client_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (client_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (connect(client_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    printf("=================================\n");
    printf(" University Login System\n");
    printf("=================================\n");

    printf("Username: ");
    scanf("%49s", request.username);

    printf("Password: ");
    scanf("%49s", request.password);

    write(client_fd, &request, sizeof(request));

    read(client_fd, response, sizeof(response));

    printf("\nAuthentication Result: %s\n", response);

    close(client_fd);

    return 0;
}
