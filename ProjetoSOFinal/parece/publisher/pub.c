#include "logging.h"
#include "fs/config.h"
#include "fs/operations.h"
#include "fs/state.h"
#include "sys/stat.h"
#include "stdio.h"
#include "stdlib.h"
#include "stdbool.h"
#include "pthread.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <library.h>
#include <signal.h>

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);
    (void)argc;
    (void)argv;

    if (argc != 4)
    {
        fprintf(stdout, "ERROR: Invalid Arguments\n");
        return -1;
    }
    // desativar o sigpipe
    char buffer[289];
    memset(buffer, '\0', 289);
    unlink(argv[2]);
    int pubFIFO = mkfifo(argv[2], 0666);
    if (pubFIFO != 0)
    {
        // tf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // register request
    int rx = open(argv[1], O_RDWR);
    if (rx == -1)
    {
        // fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    buffer[0] = 1;
    memcpy(buffer + 1, argv[2], 256);
    memcpy(buffer + 257, argv[3], 32);
    ssize_t ret = write(rx, buffer, sizeof(buffer));
    if (ret < 0)
    {
        exit(EXIT_FAILURE);
    }
    close(rx);

    // publisher inputs
    int rx2 = open(argv[2], O_WRONLY);
    if (rx2 < 0)
    {
        perror("erro ao abrir o publisher pipe");
        exit(EXIT_FAILURE);
    }

    //Loop for creating the message that is going to be written into the box of this
    //publisher, and send it to the Server
    while (true)
    {

        char mensage[1025];
        mensage[0] = '9';
        memset(mensage + 1, '\0', sizeof(mensage) - 1);
        char *f = fgets(mensage + 1, sizeof(mensage) - 1, stdin);
        if (f == NULL)
        {
            break;
        }
        mensage[strlen(mensage) - 1] = '\0';
        ssize_t n = write(rx2, mensage, sizeof(mensage));

        if (n < 0)
        {
            if (errno = EPIPE)
                return 0;
            perror("erro no write do Publisher");
            exit(EXIT_FAILURE);
        }
    }

    close(rx2);
    return 0;
}
