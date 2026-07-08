#ifndef COMMON_H
#define COMMON_H

#include <sys/socket.h>
#include <sys/un.h>

#define SOCKET_PATH "/tmp/auth_socket"

#define USERNAME_SIZE 50
#define PASSWORD_SIZE 50
#define RESPONSE_SIZE 100

typedef struct
{
    char username[USERNAME_SIZE];
    char password[PASSWORD_SIZE];
} LoginRequest;

#endif
