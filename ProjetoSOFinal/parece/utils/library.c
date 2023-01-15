#include "library.h"

Box *servers_box;
pthread_mutex_t servers_box_mutex;

void Server_Box_Init_All()
{
    servers_box = (Box *)malloc(sizeof(Box) * 64);
    if (pthread_mutex_init(&servers_box_mutex, NULL) == -1)
    {
        exit(EXIT_FAILURE);
    }
    for (unsigned int i = 0; i < 64; i++)
    {

        strcpy(servers_box[i].box_name, "\0");
        servers_box[i].box_size = (uint64_t)0;
        servers_box[i].n_publishers = (uint64_t)0;
        servers_box[i].n_subscribers = (uint64_t)0;
    }
}

void Server_Box_Increment_Size(char *box_name, uint64_t size)
{
    if (pthread_mutex_lock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < 64; i++)
    {
        if (strcmp(servers_box[i].box_name, box_name) == 0)
        {
            servers_box[i].box_size += size;
        }
    }
    if (pthread_mutex_unlock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
}

void Server_Box_Remove(char *box_name)
{
    if (pthread_mutex_lock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < 64; i++)
    {
        if (strcmp(servers_box[i].box_name, box_name) == 0)
        {
            servers_box[i].box_size = 0;
            servers_box[i].n_publishers = 0;
            servers_box[i].n_subscribers = 0;
            strcpy(servers_box[i].box_name, "\0");
            if (pthread_mutex_unlock(&servers_box_mutex) == -1)
            {
                exit(EXIT_FAILURE);
            }
            return;
        }
    }
    if (pthread_mutex_unlock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    return;
}

void Server_Box_Add(Box box)
{

    if (pthread_mutex_lock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < 64; i++)
    {
        if (strcmp(servers_box[i].box_name, "\0") == 0)
        {
            servers_box[i] = box;
            servers_box[i].last = 1;
            if (i != 0)
                servers_box[i - 1].last = 0;
            if (pthread_mutex_unlock(&servers_box_mutex) == -1)
            {
                exit(EXIT_FAILURE);
            }
            return;
        }
    }

    if (pthread_mutex_unlock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
}

Box *Server_Box_Lookup(char *box_name)
{
    if (pthread_mutex_lock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    Box *box = NULL;
    for (size_t i = 0; i < 64; i++)
    {
        if (strcmp(servers_box[i].box_name, box_name) == 0)
        {
            box = &servers_box[i];
            break;
        }
    }
    if (pthread_mutex_unlock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    return box;
}

void Server_Box_List(char *pipe_name)
{
    char *message = (char *)malloc(58);
    memset(message, '\0', 58);
    ssize_t n;
    int pipe = open(pipe_name, O_RDWR);
    if (pipe == -1)
    {
        perror("erro ao abrir o manager pipe");
        exit(EXIT_FAILURE);
    }
    if (pthread_mutex_lock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    int loop = true;
    while (loop)
    {
        for (int i = 0; i < 64; i++)
        {
            if (strcmp(servers_box[i].box_name, "\0") != 0)
            {
                loop = false;
                break;
            }
        }
        if (loop == false)
            break;
        message[0] = 8;
        message[1] = 1;
        n = write(pipe, message, 58);
        if (n < 0)
        {
            perror("erro a escrever no manager pipe");
            exit(EXIT_FAILURE);
        }
        if (pthread_mutex_unlock(&servers_box_mutex) == -1)
        {
            exit(EXIT_FAILURE);
        }
        return;
    }
    for (int i = 0; i < 64; i++)
    {
        if (strcmp(servers_box[i].box_name, "\0") != 0)
        {
            int flag = 1;

            if (strcmp(servers_box[i + 1].box_name, "\0") == 0)
            {
                flag = 0;
                for (int j = i; j < 64; j++)
                {
                    if (strcmp(servers_box[j + 1].box_name, "\0") != 0)
                        flag = 1;
                }
            }
            if (flag)
                message[1] = 0;
            else
                message[1] = 1;

            message[0] = 8;
            memcpy(message + 2, servers_box[i].box_name + 1, 32);
            memcpy(message + 34, &servers_box[i].n_publishers, sizeof(uint64_t));
            memcpy(message + 42, &servers_box[i].n_subscribers, sizeof(uint64_t));
            memcpy(message + 50, &servers_box[i].box_size, sizeof(uint64_t));

            n = write(pipe, message, 58);
            if (n < 0)
            {
                perror("erro a escrever no manager pipe");
                exit(EXIT_FAILURE);
            }
        }
    }
    if (pthread_mutex_unlock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
}

void Server_Box_List_Manager(Box *servers_box_Manager)
{
    // Sort the List Alphabetically using Bubble Sort
    Box temp;
    for (unsigned int i = 0; i < 63; i++)
        for (unsigned int j = i + 1; j < 64; j++)
        {
            if (strcmp(servers_box_Manager[i].box_name, servers_box_Manager[j].box_name) > 0)
            {
                temp = servers_box_Manager[i];
                servers_box_Manager[i] = servers_box_Manager[j];
                servers_box_Manager[j] = temp;
                servers_box_Manager[j].last = 0;
                servers_box_Manager[i].last = 0;
            }
        }

    servers_box_Manager[63].last = 1;
}
void Server_Box_Init_Manager(Box *servers_box_Manager)
{
    for (unsigned int i = 0; i < 64; i++)
    {
        strcpy(servers_box_Manager[i].box_name, "\0");
        servers_box_Manager[i].box_size = (uint64_t)0;
        servers_box_Manager[i].n_publishers = (uint64_t)0;
        servers_box_Manager[i].n_subscribers = (uint64_t)0;
    }
}

void Server_Box_Destroy()
{
    free(servers_box);
    if (pthread_mutex_destroy(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
}

void Server_Box_Change_Values(char *box_name, int flag)
{
    if (pthread_mutex_lock(&servers_box_mutex) == -1)
    {
        exit(EXIT_FAILURE);
    }
    for (size_t i = 0; i < 64; i++)
    {
        if (strcmp(servers_box[i].box_name, box_name) == 0)
        {
            if (flag == 1)
            {
                servers_box[i].n_publishers++;
            }
            if (flag == 2)
            {
                servers_box[i].n_subscribers++;
            }
            if (flag == 3)
            {
                servers_box[i].n_publishers--;
            }
            if (flag == 4)
            {
                servers_box[i].n_subscribers--;
            }
            if (pthread_mutex_unlock(&servers_box_mutex) == -1)
            {
                exit(EXIT_FAILURE);
            }
            return;
        }
    }
}