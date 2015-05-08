#ifndef CHATCLIENT_H
#define CHATCLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include "sglib.h"
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>


#define CLIENTLIST_COMPARATOR(e1, e2) (e1->id - e2->id)
#define SALACHATLIST_COMPARATOR(e1, e2) (e1->id - e2->id)

struct structCliente {

    int id;
    int socketDescriptor;
    int roomId;

    pthread_t threadId;
    char nickName[64];

    struct structCliente * next;
};

typedef struct structCliente strCliente;

struct structSalaChat {

	int id;
	char roomName[64];
	int userCount;

	struct structSalaChat * next;
};

typedef struct structSalaChat strSalaChat;


SGLIB_DEFINE_LIST_PROTOTYPES(strCliente, CLIENTLIST_COMPARATOR, next)
SGLIB_DEFINE_LIST_PROTOTYPES(strSalaChat, SALACHATLIST_COMPARATOR, next)

#endif