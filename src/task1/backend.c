#include <errno.h>	
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"

int main()
{
    int server_fd, client_fd;
    struct sockaddr_un addr;
    LoginRequest request;
    char response[RESPONSE_SIZE];
	FILE *logFile;
    unlink(SOCKET_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, SOCKET_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("=================================\n");
    printf(" Authentication Server Started\n");
    printf(" Waiting for client...\n");
    printf("=================================\n");

printf("\n========== Privilege Information ==========\n");
printf("Real UID      : %d\n", getuid());
printf("Effective UID : %d\n", geteuid());

printf("\nAttempting to drop privileges...\n");

if (setresuid(getuid(), getuid(), getuid()) == -1)
{
    perror("setresuid");
}
else
{
    printf("Privileges dropped successfully.\n");
}

printf("Current Real UID      : %d\n", getuid());
printf("Current Effective UID : %d\n", geteuid());
printf("===========================================\n");    client_fd = accept(server_fd, NULL, NULL);

    if (client_fd < 0)
    {
        perror("accept");
        exit(EXIT_FAILURE);
    }

    read(client_fd, &request, sizeof(request));

    printf("\nClient Connected\n");
    printf("Username: %s\n", request.username);

    logFile = fopen("auth.log", "a");

if (strcmp(request.username, "admin") == 0 &&
    strcmp(request.password, "Password123") == 0)
{
    strcpy(response, "SUCCESS");

    if(logFile)
        fprintf(logFile,
                "Username: %s | Status: SUCCESS\n",
                request.username);
}
else
{
    strcpy(response, "FAILED");

    if(logFile)
        fprintf(logFile,
                "Username: %s | Status: FAILED\n",
                request.username);
}

if(logFile)
    fclose(logFile);
	memset(request.password, 0, sizeof(request.password));

	printf("Password buffer cleared from memory.\n");

    write(client_fd, response, sizeof(response));

    close(client_fd);
    close(server_fd);

    unlink(SOCKET_PATH);

    printf("Authentication Complete.\n");

    return 0;
}

