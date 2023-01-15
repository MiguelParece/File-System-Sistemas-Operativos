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
#include <producer-consumer.h>
#include <stdint.h>
#include <producer-consumer.h>
#include <signal.h>

#define TAMMSG 1000

void Manager(u_int8_t opcode, char *pipe_name, char *box_name)
{   
    int inumber;
    char *responseBuffer = (char *)malloc(1026);
    memset(responseBuffer, '\0', 1026);
    ssize_t n;
    char *errorMessageForCreate = (char *)malloc(1024);
    char *errorMessageForRemove = (char *)malloc(1024);
    strcpy(errorMessageForRemove, "Box doesn't exist");
    strcpy(errorMessageForCreate, "Box already exists");
    int clientFIFO = open(pipe_name, O_RDWR);
    if (clientFIFO == -1)
    {
        perror("erro ao abrir o publisher pipe");
        exit(EXIT_FAILURE);
    }
    switch (opcode)
    {
    //Create Box
    case 3:
        responseBuffer[0] = 4;
        //Check if the box exists. If it does return an error message
        inumber = tfs_open(box_name, TFS_O_APPEND);
        tfs_close(inumber);
        if (inumber != -1)
        {
            responseBuffer[1] = -1;
            memcpy(responseBuffer + 2, errorMessageForCreate, 1024);
            n = write(clientFIFO, responseBuffer, 1026);
            if (n < 0)
            {
                perror("erro ao escrever no pipe");
                exit(EXIT_FAILURE);
            }
            break;
        }

        //Create the Box
        inumber = tfs_open(box_name, TFS_O_CREAT);
        if (inumber != -1)
        {
            //Create the Box that is going to be added to the global variable,
            //servers_box, an array of boxes
            Box box_to_add;
            strcpy(box_to_add.box_name, box_name);
            box_to_add.box_size = 0;
            box_to_add.last = 0;
            box_to_add.n_publishers = 0;
            box_to_add.n_subscribers = 0;
            if (pthread_mutex_init(&box_to_add.b_mutex, NULL) == -1 || pthread_cond_init(&box_to_add.cond, NULL) == -1)
            {
                perror("trying to initialize the box");
                exit(EXIT_FAILURE);
            }
            Server_Box_Add(box_to_add);
            //Inform that the operation was sucessfull in the message
            responseBuffer[1] = 0;
            n = write(clientFIFO, responseBuffer, 1026);
            if (n < 0)
            {
                perror("erro ao escrever no pipe");
                exit(EXIT_FAILURE);
            }
        }
        tfs_close(inumber);
        break;

    //Remove Box
    case 5:
        responseBuffer[0] = 6;
        //Verify if the box that is going to be removed exists. If not, 
        // write an error message in client's pipe
        inumber = tfs_open(box_name, TFS_O_APPEND);
        if (inumber == -1)
        {
            responseBuffer[1] = -1;
            memcpy(responseBuffer + 2, errorMessageForRemove, 1024);
            n = write(clientFIFO, responseBuffer, 1026);
            if (n < 0)
            {
                perror("erro ao escrever no pipe");
                exit(EXIT_FAILURE);
            }
        }
        //Remove the box from the TFS and from the servers_box
        tfs_unlink(box_name);
        Server_Box_Remove(box_name);
        //Inform that the operation was sucessfull in the message
        responseBuffer[1] = 0;
        n = write(clientFIFO, responseBuffer, 1026);
        if (n < 0)
        {
            perror("erro ao escrever no pipe");
            exit(EXIT_FAILURE);
        }
        break;
    //List Boxes
    case 7:
        Server_Box_List(pipe_name);
    default:
        break;
    }
    free(errorMessageForCreate);
    free(errorMessageForRemove);
    free(responseBuffer);
}

void RegisterSubscriber(char *box_name, char *pipe_name)
{
    (void)box_name;
    (void)pipe_name;
    char buffer[1025];
    int rx = open(pipe_name, O_WRONLY);
    if (rx == -1)
    {
        exit(EXIT_FAILURE);
    }
    //Verify if the box exists. If not write in the client's fifo a message
    // with the eleventh code, which informs that the box that was supposed to be associated
    // with the client doesn't exist
    int inumber = tfs_open(box_name, TFS_O_APPEND);
    if (inumber == -1)
    {   
        buffer[0]=(char)11;
        memset(buffer+1,0,1024);
        ssize_t k= write(rx,buffer,1025);  
        if(k==-1){
            perror("sub trying to write to pipe");
            exit(EXIT_FAILURE);
        }
        close(rx);
        return;
    }
    tfs_close(inumber);
    inumber = tfs_open(box_name, 0);

    //Increment the number of subscribers in the box
    Server_Box_Change_Values(box_name, 2);

    //Get the box that is going to be worked with
    Box *BoxSub = Server_Box_Lookup(box_name);

    while (true)
    {
        memset(buffer, 0, 1025);
        buffer[0] = (char)10;
        if (pthread_mutex_lock(&BoxSub->b_mutex) != 0)
        {
            perror("error trying to lock the box");
            exit(EXIT_FAILURE);
        }
        ssize_t i;
        while ((i = tfs_read(inumber, buffer + 1, 1024)) == 0)
        {
            pthread_cond_wait(&BoxSub->cond, &BoxSub->b_mutex);
            char bufferteste[1];
            memset(bufferteste,'=',1); 
            ssize_t q = write(rx, bufferteste, 1);
            if(q<0){
                if (pthread_mutex_unlock(&BoxSub->b_mutex) != 0)
                    {
                        perror("error trying to lock the box");
                        exit(EXIT_FAILURE);
                    }
                goto fimdosub;     
            }
            // fflush(stderr);
        }

        if (pthread_mutex_unlock(&BoxSub->b_mutex) != 0)
        {
            perror("error trying to lock the box");
            exit(EXIT_FAILURE);
        }

        if (i == -1)
        {
            perror("sub trying to read from file");
            exit(EXIT_FAILURE);
        }
        ssize_t q = write(rx, buffer, sizeof(buffer));
        if (q == -1)
        {
            perror("sub trying to write to pipe");
            exit(EXIT_FAILURE);
        }
    }
    //Final Part of Sub. The Function reaches this state when the sub's session closes and a 
    //"broadcast" is casted in order to unlock the conditional variable, making the decrement 
    //of the number of subs possible
    fimdosub:
    fprintf(stderr,"CHEGOU AUI3\n");
    Server_Box_Change_Values(box_name,4);
    return;
}

void RegisterPublisher(char *box_name, char *pipe_name)
{   
    //Verify if the box exists
    int inumber = tfs_open(box_name, TFS_O_APPEND);
    if (inumber == -1)
    {
        int rx = open(pipe_name, O_RDONLY);
        if (rx == -1)
        {
            exit(EXIT_FAILURE);
        }
        close(rx);
        return;
    }
    //Get the box that its going to be worked with
    Box *box_of_pub = Server_Box_Lookup(box_name);
    //Check if the box in question has no active publishers
    if (box_of_pub->n_publishers == 1)
    {
        int rx = open(pipe_name, O_RDONLY);
        if (rx == -1)
        {
            exit(EXIT_FAILURE);
        }
        close(rx);
        return;
    }
    //Increment the number of publishers by one, since we have 
    //confirmation that the box doesn't have any publisher
    Server_Box_Change_Values(box_name, 1);
    int rx = open(pipe_name, O_RDONLY);
    if (rx == -1)
    {
        exit(EXIT_FAILURE);
    }

    ssize_t n;

    // Close the box name so that we can open it and check if it exists in the while loop
    tfs_close(inumber);
    while (true)
    {
        char buffer[1025];
        memset(buffer, 0, 1025);

        n = read(rx, buffer, 1025);
        if (n < 0)
        {
            perror("lol");
            exit(EXIT_FAILURE);
        }
        if (n == 0)
        {
            // EOF reached
            Server_Box_Change_Values(box_name, 3);
            break;
        }
        
        //Check if the box does still exists in the TFS
        //In order words, check if the box wasn't removed in the time 
        //between the beggining of the while loop and the "present"
        inumber = tfs_open(box_name, TFS_O_APPEND);
        if (inumber == -1)
        {
            tfs_close(inumber);
            break;
        }

        //Write the contents to the box, and increment its size 
        ssize_t k = tfs_write(inumber, buffer + 1, 1024);
        Server_Box_Increment_Size(box_name, (uint64_t)strlen(buffer) - 1);
        if (k < 0)
        {
            close(rx);
            tfs_close(inumber);
            perror("publisher trying to write");
            exit(EXIT_FAILURE);
        }
        tfs_close(inumber);

        // send broadcast to all subs waiting for this box
        pthread_cond_broadcast(&box_of_pub->cond);
    }
    close(rx);
}

//
pc_queue_t producer_consumer_queue;
//

void *process_register_request(void *arg)
{
    (void)arg;
    while (1)
    {
        /* Wait for an order to be available in the queue */
        char *order = (char *)pcq_dequeue(&producer_consumer_queue);
        /* Process the order */
        char *pipe_name = (char *)malloc(256);
        char *box_name = (char *)malloc(33);
        u_int8_t opcode = (u_int8_t)order[0];
        box_name[0] = '/';
        memcpy(pipe_name, order + 1, 256);
        memcpy(box_name + 1, order + 257, 32);
        switch (opcode)
        {
        case 1:
            RegisterPublisher(box_name, pipe_name);
            break;

        case 2:
            RegisterSubscriber(box_name, pipe_name);
            break;
        case 3:
            Manager(opcode, pipe_name, box_name);

            break;
        case 5:
            Manager(opcode, pipe_name, box_name);
            break;
        case 7:

            Manager(opcode, pipe_name, box_name);
            break;

        default:
            break;
        }

        free(box_name);
        free(pipe_name);
        /* Release the resources allocated for the order */
        free(order);
    }
    return NULL;
}
//

int main(int argc, char **argv)
{
    (void)argc;
    // ignore the sigpipe
    signal(SIGPIPE, SIG_IGN);
    // Initialize the server's boxes
    Server_Box_Init_All(servers_box);
    ssize_t n;
    fprintf(stderr, "usage: mbroker <pipename>\n");

    tfs_params params = tfs_default_params();
    tfs_init(&params);
    // create the register pipe
    unlink(argv[1]);
    int registerFIFO = mkfifo(argv[1], 0666);
    if (registerFIFO != 0)
    {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    // open register pipe
    int fd = open(argv[1], O_RDWR);
    if (fd == -1)
    {
        fprintf(stderr, "[ERR]: OPEN failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // create the queue
    size_t capacity = 100;
    pcq_create(&producer_consumer_queue, capacity);

    int numberOfThreads = 10;
    pthread_t threads[numberOfThreads];
    int thread_id[numberOfThreads];

    for (int i = 0; i < 10; i++)
    {
        thread_id[i] = i;
        pthread_create(&threads[i], NULL, process_register_request, &thread_id[i]);
    }

    while (true)
    {
        // Check if data is available for reading
        char *order = (char *)malloc(sizeof(char) * 289);
        memset(order, '\0', 289);
        n = read(fd, order, 289);
        char opcode = order[0];
        switch (opcode)
        {
            // publisher
        case 1:
        case 2:
        case 3:
        case 5:
            if (n >= 288)
            {

                pcq_enqueue(&producer_consumer_queue, order);

                break;

            case 7:
                if (n >= 256)
                {

                    pcq_enqueue(&producer_consumer_queue, order);
                }
                break;
            default:
                break;
            }
            if (n == 0)
            {
                // EOF reached
                break;
            }
            else if (n < 0)
            {
                // Read error
                fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }
    for (int i = 0; i < 10; i++)
    {
        pthread_join(threads[i], NULL);
    }
    pcq_destroy(&producer_consumer_queue);
    close(fd);
    Server_Box_Destroy(servers_box);
    return 0;
}
