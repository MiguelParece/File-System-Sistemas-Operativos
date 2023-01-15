#ifndef __UTILS_LIBRABRY_H__
#define __UTILS_LIBRABRY_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>

#include "logging.h"
#include "fs/config.h"
#include "fs/operations.h"
#include "fs/state.h"
#include "sys/stat.h"
#include "pthread.h"
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pthread.h>

#define MESSAGE_CODE_PUBLISHER_REGISTRATION 1
#define MESSAGE_CODE_SUBSCRIBER_REGISTRATION 2
#define MESSAGE_CODE_CREATE_BOX 3
#define MESSAGE_CODE_CREATE_BOX_RESPONSE 4
#define MESSAGE_CODE_REMOVE_BOX 5
#define MESSAGE_CODE_REMOVE_BOX_RESPONSE 6
#define MESSAGE_CODE_LIST_BOXES 7
#define MESSAGE_CODE_BOX_INFO 8


typedef struct
{
    uint8_t last;
    char box_name[34];
    uint64_t n_publishers;
    uint64_t n_subscribers;
    uint64_t box_size;
    pthread_mutex_t b_mutex;
    pthread_cond_t cond;
} Box;

extern Box *servers_box;
extern pthread_mutex_t servers_box_mutex;

// Initialize all the values of the servers_box with "ghost" boxes. Their attributes have values,
//   all are set to 0 but they are not recognized as usuable boxes by the Server
void Server_Box_Init_All();
// Given a pointer to a box_nome, remove it from the servers_box transforming its position into
// a "ghost" box
void Server_Box_Remove(char *box_name);
// Given a box, add it to first position filled with a "ghost" box
void Server_Box_Add(Box box);
//Find the bxox that has box_name as it's name 
Box* Server_Box_Lookup(char *box_name);
// Given a client's pipe_name send messages that contain all the non "ghost" boxes (1 box per message)
//  to the client's pipe_name
void Server_Box_List(char *pipe_name);
// Sort Alphabetically the boxes in the server_box and define what is the last
// printable box
void Server_Box_List_Manager(Box *servers_box_Manager);
// Increment the box.size of the box that has the box_name as it's name
void Server_Box_Increment_Size(char *box_name, uint64_t value);
// Destroy the servers_box. Free it and destroy the global mutex.
void Server_Box_Destroy();
//Initialize all the boxes from the servers_box_Manager with "ghost" boxes
void Server_Box_Init_Manager(Box *servers_box_Manager);
//Given a box_name, change the value of a certain atribute of the box that has the box name as its name
//flag=1 : Increment the number of publishers
//flag=2 : Increment the number of subscribers
//flag=3 : Decrement the number of publishers
//flag=4 : Decrement the number of subscribers
void Server_Box_Change_Values(char *box_name, int flag);

#endif