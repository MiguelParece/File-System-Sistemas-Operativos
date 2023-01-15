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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "library.h"
#include <stdint.h>

//Add a box to the servers_box_Manager. Similar to Server_Box_Add but its receives 
//the array as a parameter
void Server_Add_Box_Manager(Box servers_box_Manager[], Box box_to_add)
{
    for (size_t i = 0; i < 64; i++)
    {
        if (strcmp(servers_box_Manager[i].box_name, "\0") == 0)
        {
            servers_box_Manager[i] = box_to_add;
            servers_box_Manager[i].last = 1;
            if (i != 0)
                servers_box_Manager[i - 1].last = 0;
            break;
        }
    }
    return;
}

int main(int argc, char **argv)
{
    // Check if the input is reasonably correct. In other words see if the number of elements
    //  in the input are correct
    if (argc != 4 && argc != 5)
    {
        fprintf(stdout, "ERROR: Invalid Arguments\n");
        return -1;
    }
    // Creation of an array of boxes, similar to the one we use in the Server
    Box *servers_box_Manager = (Box *)malloc(sizeof(Box) * 64);
    Server_Box_Init_Manager(servers_box_Manager);

    (void)argc;

    u_int8_t buffer[289];
    unlink(argv[2]);
    // Make and open the client fifo
    int manager_fifo = mkfifo(argv[2], 0666);
    int registerFIFO = open(argv[1], O_RDWR);
    if (manager_fifo != 0)
    {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (registerFIFO == -1)
    {
        fprintf(stderr, "[ERR]: OPEN failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    //  Evaluation of what type of message are we dealing with
    int code = -1;
    if (strcmp(argv[3], "create") == 0)
    {
        code = 1;
    }
    if (strcmp(argv[3], "remove") == 0)
    {
        code = 2;
    }
    if (strcmp(argv[3], "list") == 0)
    {
        code = 3;
    }

    // Creation of the request that is gonna be sent to the Server
    switch (code)
    {
    case 1:
        buffer[0] = 3;
        memcpy(buffer + 1, argv[2], 256);
        memcpy(buffer + 257, argv[4], 32);
        break;
    case 2:
        buffer[0] = 5;
        memcpy(buffer + 1, argv[2], 256);
        memcpy(buffer + 257, argv[4], 32);
        break;
    case 3:
        buffer[0] = 7;
        memcpy(buffer + 1, argv[2], 256);
        memset(buffer + 257, '\0', 32);
        break;
    default:
        printf("Not suitable operator for manager\n");
        return -1;
    }

    // Write the message in the register fifo
    ssize_t ret = write(registerFIFO, buffer, 289);
    if (ret < 0)
    {
        exit(EXIT_FAILURE);
    }

    int rx2 = open(argv[2], O_RDWR);
    if (rx2 < 0)
    {
        perror("erro ao abrir o manager pipe");
        exit(EXIT_FAILURE);
    }

    //Get and print the response of the request sent by the server
    char *message = (char *)malloc(1026);
    char *error_message = (char *)malloc(1024);
    char *boxname = (char *)malloc(32);
    memset(message, '\0', 1026);
    memset(boxname, '\0', 32);
    memset(error_message, '\0', 1024);
    while (true)
    {
        ret = read(rx2, message, 1);
        if (ret < 0)
        {
            exit(EXIT_FAILURE);
        }
        switch (message[0])
        {
        //Response to Create
        case 4:
            ret = read(rx2, message + 1, 1025);
            if (ret < 0)
            {
                exit(EXIT_FAILURE);
            }
            if (message[1] == 0)
            {
                fprintf(stdout, "OK\n");
                free(message);
                free(error_message);
                free(boxname);
                return 0;
            }
            memcpy(error_message, message + 2, 1024);
            fprintf(stdout, "ERROR %s\n", error_message);
            free(message);
            free(error_message);
            free(boxname);
            return 0;

        //Response to Remove
        case 6:
            ret = read(rx2, message + 1, 1025);
            if (ret < 0)
            {
                exit(EXIT_FAILURE);
            }
            if (message[1] == 0)
            {
                fprintf(stdout, "OK\n");
                free(message);
                free(error_message);
                free(boxname);
                return 0;
            }
            memcpy(error_message, message + 2, 1024);
            fprintf(stdout, "ERROR %s\n", error_message);
            free(message);
            free(error_message);
            free(boxname);
            return 0;

        //Response to List
        case 8:
            ret = read(rx2, message + 1, 57);
            if (ret < 0)
            {
                exit(EXIT_FAILURE);
            }
            memcpy(boxname, message + 2, 32);
            Box box_to_add;
            strcpy(box_to_add.box_name, boxname);
            box_to_add.box_size = (uint64_t)message[50];
            box_to_add.last = 0;
            box_to_add.n_publishers = (uint64_t)message[34];
            box_to_add.n_subscribers = (uint64_t)message[42];

            Server_Add_Box_Manager(servers_box_Manager, box_to_add);
            break;

        default:
            break;
        }

        if (message[1] == 1)
        {
            break;
        }
    }

    // This free is only reached when the comand is to list the boxes, message[0]=8.
    // So we are in no danger of freeing this pointer twice.
    free(message);
    free(error_message);
    free(boxname);

    // Alphabetically Sort the array of boxes
    Server_Box_List_Manager(servers_box_Manager);
    if (strcmp(&servers_box_Manager[63].box_name[0], "\0") == 0)
    {
        fprintf(stdout, "NO BOXES FOUND\n");
        return 0;
    }

    for (unsigned int i = 0; i < 64; i++)
    {
        //Print to the output the all the valid boxes
        if (strcmp(servers_box_Manager[i].box_name, "\0") != 0)
        {
            fprintf(stdout, "%s %zu %zu %zu\n", servers_box_Manager[i].box_name, servers_box_Manager[i].box_size,
                    servers_box_Manager[i].n_publishers, servers_box_Manager[i].n_subscribers);
        }
        if (servers_box_Manager[i].last == 1)
        {
            return 0;
        }
    }
    return -1;
}