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
#include "utils/library.h"

#include <fcntl.h>
bool flag;

//fuction to handle the SIGINT
static void sig_handler(int sig)
{
    if (sig == SIGINT)
    {
        if (signal(SIGINT, sig_handler) == SIG_ERR)
        {
            exit(EXIT_FAILURE);
        }
        flag = false;
        return;
    }
}

int main(int argc, char **argv)
{
    if (argc != 4)
    {
        fprintf(stdout, "ERROR: Invalid Arguments\n");
        return -1;
    }
    (void)argc;
    (void)argv;
    //Flag that indicates that the sub should or should not try to read messages
    //from the box 
    flag = true;

    if (signal(SIGINT, sig_handler) == SIG_ERR)
    {
        exit(EXIT_FAILURE);
    }
    u_int8_t buffer[289];


    int rx = open(argv[1], O_WRONLY);
    if (rx == -1)
    {
  
        exit(EXIT_FAILURE);
    }
    //Make sure there isn't any fifo with this path name
    unlink(argv[2]);
    int subFIFO = mkfifo(argv[2], 0666);
    if (subFIFO != 0)
    {
        exit(EXIT_FAILURE);
    }
    // Create the request for a register sub
    buffer[0] = 2;
    memcpy(buffer + 1, argv[2], 256);
    memcpy(buffer + 257, argv[3], 32);
    ssize_t ret = write(rx, buffer, sizeof(buffer));
    if (ret < 0)
    {
        exit(EXIT_FAILURE);
    }
    rx = open(argv[2], O_RDONLY);
    if (rx < 0)
    {
        perror("sub trying to open pipe");
        exit(EXIT_FAILURE);
    }

    uint64_t number_of_messages_read = 0;

    while (flag)
    {
        char mensage[1025];
        memset(mensage, '\0', 1025);
        ssize_t n = read(rx, mensage, sizeof(mensage));

        //The box in which we tried to subscribe didn't exist,
        //Immediately leave the program
        if(mensage[0]==11){
            close(rx);
            return 0;
        }

        //If flag=false , it means that at some point we caught a SIGINT. So we need to finish the session
        if (n < 0 && !flag)
        {
            fprintf(stdout, "Number of messages read: %zu\n", number_of_messages_read);
            
            close(rx);
            return 0;
        }
        else if (n < 0)
        {
            if (errno = EPIPE){
                return 0;
            }
            perror("sub trying to read from pipe");
            exit(EXIT_FAILURE);
        }
        //The most common iteration of the loop. Simply write in the output one message
        // from the box and increment the number of messages read
        if (n > 0)
        {
            fprintf(stdout, "%s\n", mensage + 1);
            number_of_messages_read++;
        }

    }
    close(rx);
    return -1;
}
