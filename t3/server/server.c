#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include "chatclient.h"
#include "../common/chatcommon.h"

#define PORT 4557

strCliente * listaClientesConectados = NULL;
strSalaChat * listaSalasChat = NULL;

int newClientId = 0;
int newRoomId = 0;

void terminateClientConnection(strCliente * client) {
    sglib_strCliente_delete(&listaClientesConectados, client);

    close(client->socketDescriptor);

    printf("[INFO] Cliente [%d] desconectado!\n", client->id);

    free(client);
}

int sendMessage(strMensagem * message, strCliente * client) {

    int bytesTransfered = write(client->socketDescriptor, (char*) message, sizeof(strMensagem));
    if (bytesTransfered < 0)  {
        printf("ERROR writing to socket");
    }
    return bytesTransfered;
}

int broadCastMessage(strMensagem * message, int roomId) {

    //TODO: MUTEX
    struct sglib_strCliente_iterator iterator;
    strCliente * elemIterator;

    for(elemIterator = sglib_strCliente_it_init(&iterator, listaClientesConectados); elemIterator != NULL; elemIterator = sglib_strCliente_it_next(&iterator)) {

        if(elemIterator->roomId == roomId) {
            sendMessage(message, elemIterator);
        }
    }
}

int createRoom(char * roomName) {

    if(!roomName || strlen(roomName) > 64) {
        //Sala invalida, ou nome muito grande
        return -1;
    }

    struct sglib_strSalaChat_iterator iterator;
    strSalaChat * elemIterator;

    for(elemIterator = sglib_strSalaChat_it_init(&iterator, listaSalasChat); elemIterator != NULL; elemIterator = sglib_strSalaChat_it_next(&iterator)) {

        if(!strcmp(roomName, elemIterator->roomName)) {
            //Ja existe uma sala com esse nome.
            return -1;
        }
    }

    //A sala ainda nao existe, criar sala!
    strSalaChat * novaSala = (strSalaChat *) malloc(sizeof(strSalaChat));

    novaSala->id = newRoomId++;
    strcpy(novaSala->roomName, roomName);
    novaSala->userCount = 0;

    sglib_strSalaChat_add(&listaSalasChat, novaSala);

    printf("[INFO] Criada sala [%d] [%s]\n", novaSala->id, novaSala->roomName);

    return novaSala->id;
}

int clientJoinRoom(strCliente * client, int roomId) {

    //TODO: MUTEX
    if(!client) {
        return -1;
    }

    if(client->roomId == roomId) {
        //Cliente ja esta na sala
        return roomId;
    }

    struct sglib_strSalaChat_iterator iterator;
    strSalaChat * elemIterator;

    for(elemIterator = sglib_strSalaChat_it_init(&iterator, listaSalasChat); elemIterator != NULL; elemIterator = sglib_strSalaChat_it_next(&iterator)) {

        if(elemIterator->id == roomId) {
            //Sala encontrada
            client->roomId = roomId;

            printf("[INFO] Cliente [%d][%s] entrou na sala [%d]\n", client->id, client->nickName, client->roomId);
            return roomId;
        }
    }

    //Sala nao encontrada
    return -1;
}

strSalaChat * getChatRoom(int roomId) {

    struct sglib_strSalaChat_iterator iterator;
    strSalaChat * elemIterator;

    for(elemIterator = sglib_strSalaChat_it_init(&iterator, listaSalasChat); elemIterator != NULL; elemIterator = sglib_strSalaChat_it_next(&iterator)) {

        if(elemIterator->id == roomId) {
            //Sala encontrada
            return elemIterator;
        }
    }
    //Sala nao encontrada
    return NULL;
}

int parseReceivedMessage(strMensagem * message, strCliente * client) {

    switch(message->commandCode) {

        strMensagem response;
        case CHANGE_NAME: {

            int nameLength = strlen(message->msgBuffer);

            if(nameLength > 64) {
                //Nome muito longo
                response.commandCode = ERROR;
                sendMessage(&response, client);
                printf("[ERROR] Cliente [%d] erro ao alterar nome. Nome muito longo!\n", client->id);
                break;
            }
            else {
                bzero(client->nickName, sizeof(client->nickName));
                strcpy(client->nickName, message->msgBuffer);

                response.commandCode = NAME_CHANGED_OK;
                sendMessage(&response, client);

                printf("[INFO] Cliente [%d] alterou o nome para [%s]\n", client->id, client->nickName);
            }
            break;
        }

        case SEND_MSG: {
            
            response.commandCode = SEND_MSG;
            sprintf(response.msgBuffer, "[%s] %s", client->nickName, message->msgBuffer);
            response.bufferLength = strlen(response.msgBuffer);

            printf("[CHAT][%d] [%s] %s\n", client->roomId, client->nickName, message->msgBuffer);

            broadCastMessage(&response, client->roomId);
            break;
        }

        case JOIN_ROOM: {

            int newRoomId = atoi(message->msgBuffer);
            
            if(clientJoinRoom(client, newRoomId) == newRoomId) {
                //Mudou de sala com sucesso
                strSalaChat * newRoom = getChatRoom(newRoomId);

                response.commandCode = ROOM_JOINED;
                sprintf(response.msgBuffer, "%s", newRoom->roomName);
                response.bufferLength = strlen(response.msgBuffer);

                sendMessage(&response,client);
            }
            else {
                //Erro ao entrar na sala (sala nao encontrada).
                response.commandCode = ROOM_JOIN_FAILED;

                bzero(response.msgBuffer, sizeof(response.msgBuffer));
                response.bufferLength = 0;
                sendMessage(&response,client);
            }

            break;
        }
    }
    return 0;
}

void * threadCliente (void * arg) {

    strCliente * client = (strCliente *) arg;

    int bytesTransfered = 0;

    //Recieve loop
    while(1) {
        
        strMensagem * receivedMessage = (strMensagem *) malloc(sizeof(strMensagem));
        bzero(receivedMessage, sizeof(receivedMessage));

        /* read from the socket */
        bytesTransfered = read(client->socketDescriptor, receivedMessage, sizeof(strMensagem));
        if (bytesTransfered < 0)  {
            printf("ERROR reading from socket");
            break;
        }
        else if (bytesTransfered == 0) {
            //EOF
            break;
        }
        else {
            //OK
            receivedMessage->msgBuffer[strcspn(receivedMessage->msgBuffer, "\r\n")] = 0; //Remove quebra de linha final
            parseReceivedMessage(receivedMessage, client);
        }

        free(receivedMessage);
    }
    terminateClientConnection(client);
}

void imprimeClientes() {

    struct sglib_strCliente_iterator iterator;
    strCliente * elemIterator;

    for(elemIterator = sglib_strCliente_it_init(&iterator, listaClientesConectados); elemIterator != NULL; elemIterator = sglib_strCliente_it_next(&iterator)) {
        printf("    [INFO] Cliente conectado: [%d] [%s]\n", elemIterator->id, elemIterator->nickName);
    }

    printf("[INFO] Total de clientes conectados: %d\n", sglib_strCliente_len(listaClientesConectados));
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, n;
    socklen_t clilen;
    char buffer[256];

    struct sockaddr_in serv_addr, cli_addr;
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR opening socket");
        exit(1);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    bzero(&(serv_addr.sin_zero), 8);     
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        printf("ERROR on binding");
        exit(1);
    }
    
    listen(sockfd, 5);
    
    clilen = sizeof(struct sockaddr_in);

    printf("[INFO] Servidor inicializado...\n");

    //Cria sala principal
    createRoom("Geral");

    while (1) {

        if ((newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen)) == -1) {
            printf("ERROR on accept");
            exit(1);
        }
        else {
            //Conexao estabelecida

            strCliente * novoCliente = (strCliente *) malloc(sizeof(strCliente));
            newClientId++;

            novoCliente->id = newClientId;
            novoCliente->roomId = -1;
            novoCliente->socketDescriptor = newsockfd;
            strcpy(novoCliente->nickName, "Sem Nome");

            sglib_strCliente_add(&listaClientesConectados, novoCliente);

            pthread_create(&novoCliente->threadId, NULL, threadCliente, (void *) novoCliente);

            imprimeClientes();
        }
    }
    
    close(sockfd);
    return 0; 
}