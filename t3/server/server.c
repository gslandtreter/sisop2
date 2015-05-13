#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#include "chatclient.h"
#include "../common/chatcommon.h"

#define PORT 4557

strCliente * listaClientesConectados = NULL;
strSalaChat * listaSalasChat = NULL;

pthread_mutex_t clientListMutex = PTHREAD_MUTEX_INITIALIZER;

int newClientId = 0;
int newRoomId = 0;

int broadCastMessage(strMensagem * message, int roomId) {

    pthread_mutex_lock(&clientListMutex);

    struct sglib_strCliente_iterator iterator;
    strCliente * elemIterator;

    for(elemIterator = sglib_strCliente_it_init(&iterator, listaClientesConectados); elemIterator != NULL; elemIterator = sglib_strCliente_it_next(&iterator)) {

        if(elemIterator->roomId == roomId) {
            sendMessage(message, elemIterator);
        }
    }

    pthread_mutex_unlock(&clientListMutex);
}

void terminateClientConnection(strCliente * client) {

    sglib_strCliente_delete(&listaClientesConectados, client);

    strMensagem msgToSend;

    msgToSend.commandCode = SEND_MSG;
    sprintf(msgToSend.msgBuffer,"> %s saiu da sala!", client->nickName);
    msgToSend.bufferLength = strlen(msgToSend.msgBuffer);
    broadCastMessage(&msgToSend, client->roomId);

    close(client->socketDescriptor);

    printf("[INFO] Cliente [%d] desconectado!\n", client->id);

    free(client);
}

void terminateAllConnections(int signal) {

    printf("[SIGINT] Encerrando conexoes...\n");

    struct sglib_strCliente_iterator iterator;
    strCliente * elemIterator;

    for(elemIterator = sglib_strCliente_it_init(&iterator, listaClientesConectados); elemIterator != NULL; elemIterator = sglib_strCliente_it_next(&iterator)) {
        terminateClientConnection(elemIterator);
    }

    exit(0);
}

int sendMessage(strMensagem * message, strCliente * client) {

    int bytesTransfered = write(client->socketDescriptor, (char*) message, sizeof(strMensagem));
    if (bytesTransfered < 0)  {
        printf("ERROR writing to socket");
    }
    return bytesTransfered;
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

strSalaChat * getChatRoomByName(char * roomName) {

    if(!roomName || strlen(roomName) > 64)
        return NULL;

    struct sglib_strSalaChat_iterator iterator;
    strSalaChat * elemIterator;

    for(elemIterator = sglib_strSalaChat_it_init(&iterator, listaSalasChat); elemIterator != NULL; elemIterator = sglib_strSalaChat_it_next(&iterator)) {

        if(strcmp(elemIterator->roomName, roomName) == 0) {
            //Sala encontrada
            return elemIterator;
        }
    }
    //Sala nao encontrada
    return NULL;
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
    //novaSala->userCount = 0;

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
    strMensagem msgToSend;

    for(elemIterator = sglib_strSalaChat_it_init(&iterator, listaSalasChat); elemIterator != NULL; elemIterator = sglib_strSalaChat_it_next(&iterator)) {

        if(elemIterator->id == roomId) {
            //Sala encontrada

            int oldRoomId = client->roomId;

            if(oldRoomId != -1) {
                msgToSend.commandCode = SEND_MSG;
                sprintf(msgToSend.msgBuffer,"> %s saiu da sala!", client->nickName);
                msgToSend.bufferLength = strlen(msgToSend.msgBuffer);
                broadCastMessage(&msgToSend, oldRoomId);
            }

            pthread_mutex_lock(&clientListMutex);
            client->roomId = roomId;
            pthread_mutex_unlock(&clientListMutex);
            
            strSalaChat * newRoom = getChatRoom(roomId);

            printf("[INFO] Cliente [%d][%s] entrou na sala [%d] - %s\n", client->id, client->nickName, client->roomId, newRoom->roomName);

            msgToSend.commandCode = SEND_MSG;
            sprintf(msgToSend.msgBuffer,"> %s entrou na sala!", client->nickName);
            msgToSend.bufferLength = strlen(msgToSend.msgBuffer);

            broadCastMessage(&msgToSend, roomId);

            //Envia retorno ao cliente

            msgToSend.commandCode = ROOM_JOINED;
            sprintf(msgToSend.msgBuffer, "%s", newRoom->roomName);
            msgToSend.bufferLength = strlen(msgToSend.msgBuffer);

            sendMessage(&msgToSend, client);

            return roomId;
        }
    }

    //Sala nao encontrada
    return -1;
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

        case CREATE_ROOM: {

            char * roomName = message->msgBuffer;

            if(strlen(roomName) > 64) {
                break;
            }

            int newRoomId = createRoom(roomName);

            if(newRoomId < 0) {
                //Erro!
                printf("[ERRO] Erro ao criar sala! Cliente [%d]\n", client->id);

                response.commandCode = CREATE_ROOM_FAILED;
                bzero(response.msgBuffer, sizeof(response.msgBuffer));
                response.bufferLength = 0;

                sendMessage(&response, client);
                break;
            }
            else {
                printf("[INFO] Sala criada! - [%s][%d] - Cliente [%d]\n", roomName, newRoomId, client->id);
                clientJoinRoom(client, newRoomId);
            }

            break;
        }

        case GET_ROOM_ID: {

            char * roomName = message->msgBuffer;
            strSalaChat * room = getChatRoomByName(roomName);

            response.commandCode = GET_ROOM_ID_RESPONSE;

            //Ternary operator FTW
            sprintf(response.msgBuffer, "%d", (room ? room->id : -1));
            response.bufferLength = strlen(response.msgBuffer);

            sendMessage(&response, client);
            break;
        }

        default: {
            printf("[INFO] Pacote recebido. CommandCode: [%d] \n", message->commandCode);
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
        bzero(receivedMessage, sizeof(strMensagem));

        /* read from the socket */
        bytesTransfered = recv(client->socketDescriptor, receivedMessage, sizeof(strMensagem), MSG_WAITALL);
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

    //Interceptar sinal SIGINT para fechar as conexoes corretamente

    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = terminateAllConnections;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);

    //Criar socket

    struct sockaddr_in serv_addr, cli_addr;
    
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR opening socket");
        exit(1);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    //Ignore time-wait sockets on binding
    int optVal = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optVal, sizeof(optVal));

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

            printf("[INFO] Cliente [%d] conectado!\n", novoCliente->id);

            //imprimeClientes();
        }
    }
    
    close(sockfd);
    return 0; 
}