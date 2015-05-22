#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ncurses.h>

#include "../common/chatcommon.h"

#define PORT 4557

int socketDescriptor;
pthread_t readThreadDescriptor;
pthread_mutex_t screenMutex = PTHREAD_MUTEX_INITIALIZER;


char windowTitle[64];

WINDOW * chatWindow, * inputWindow;

void screenShutdown() {
    if (!isendwin())
        endwin();
}

void screenSetup() {
    initscr();
    cbreak();
    echo();
    intrflush(stdscr, false);
    keypad(stdscr, true);
    atexit(screenShutdown);
}

void clearInputWindow() {

    pthread_mutex_lock(&screenMutex);

    wclear(inputWindow);
    wborder(inputWindow, '|', '|', '-', '-', '+', '+', '+', '+');

    wmove(inputWindow, 1, 2);

    wrefresh(inputWindow);

    pthread_mutex_unlock(&screenMutex);
}

void clearChatWindow() {

    pthread_mutex_lock(&screenMutex);

    wclear(chatWindow);
    wborder(chatWindow, '|', '|', '-', '-', '+', '+', '+', '+');
    wrefresh(chatWindow);

    pthread_mutex_unlock(&screenMutex);
}

void printToChatWindow(char * strMensagem) {

    pthread_mutex_lock(&screenMutex);

    scroll(chatWindow);

    wmove(chatWindow, LINES - 5, 1);
    wprintw(chatWindow, "  %s\n", strMensagem);

    wborder(chatWindow, '|', '|', '-', '-', '+', '+', '+', '+');

    int parent_x, parent_y;
    getmaxyx(stdscr, parent_y, parent_x);

    mvwprintw(chatWindow, 0, (parent_x - strlen(windowTitle)) / 2, windowTitle);

    wrefresh(chatWindow);
    wrefresh(inputWindow);

    pthread_mutex_unlock(&screenMutex);
}

void joinChatRoom(int roomId) {

    strMensagem mensagem;

    mensagem.commandCode = JOIN_ROOM;
    sprintf(mensagem.msgBuffer, "%d", roomId);

    mensagem.bufferLength = strlen(mensagem.msgBuffer);

    sendMessage(&mensagem);
}

int parseReceivedMessage(strMensagem * message) {

    switch(message->commandCode) {

        case SEND_MSG: {

            printToChatWindow(message->msgBuffer);
            break;
        }

        case ROOM_JOINED: {

            clearChatWindow();

            char szBuffer[1024];
            sprintf(szBuffer, "-- Voce entrou na sala %s --\n", message->msgBuffer);

            sprintf(windowTitle, "  %s  ", message->msgBuffer);

            printToChatWindow(szBuffer);

            clearInputWindow();
            break;
        }

        case CREATE_ROOM_FAILED: {
            printToChatWindow("Erro ao criar sala de chat!");
            break;
        }

        case GET_ROOM_ID_RESPONSE: {

            int newRoomId = atoi(message->msgBuffer);

            if(newRoomId < 0) {
                printToChatWindow("Erro ao entrar na sala!");
            }
            else {
                joinChatRoom(newRoomId);
            }
            break;
        }
    }
    return 0;
}

int sendMessage(strMensagem * message) {
    /* write in the socket */
    int bytesTransfered = write(socketDescriptor, (char *) message, sizeof(strMensagem));
    if (bytesTransfered < 0) {
        printf("ERROR writing to socket\n");
        exit(1);
    }

    return bytesTransfered;
}

void * readThread (void * arg) {

    int bytesTransfered = 0;

    //Recieve loop
    while(1) {
        
        strMensagem receivedMessage;
        bzero(&receivedMessage, sizeof(receivedMessage));

        /* read from the socket */
        bytesTransfered = recv(socketDescriptor, &receivedMessage, sizeof(strMensagem), MSG_WAITALL);
        if (bytesTransfered < 0)  {
            printf("ERROR reading from socket");
            close(socketDescriptor);
            exit(1);
            break;
        }
        else if (bytesTransfered == 0) {
            //EOF
            close(socketDescriptor);
            exit(0);
            break;
        }
        else {
            //OK
            receivedMessage.msgBuffer[strcspn(receivedMessage.msgBuffer, "\r\n")] = 0; //Remove quebra de linha final
            parseReceivedMessage(&receivedMessage);
        }
    }
}

WINDOW * drawNameWindow() {

    int parent_x, parent_y;
    getmaxyx(stdscr, parent_y, parent_x);

    echo();
    WINDOW * nameWindow = newwin(3, 35, (parent_y - 4) / 2, (parent_x - 35) / 2);
    wborder(nameWindow, '|', '|', '-', '-', '+', '+', '+', '+');
    mvwprintw(nameWindow, 0, 2, "Digite o seu Nickname");

    wmove(nameWindow, 1, 2);
    wrefresh(nameWindow);

    return nameWindow;
}

void createChatRoom(char * roomName) {
    if(!roomName)
        return;

    if(strlen(roomName) > 64) {
        printToChatWindow("Nome da sala muito longo!");
        return;
    }
    else if(strlen(roomName) == 0) {
        printToChatWindow("Nome da sala muito curto!");
        return;
    }

    strMensagem mensagem;
    mensagem.commandCode = CREATE_ROOM;
    strcpy(mensagem.msgBuffer, roomName);

    mensagem.bufferLength = strlen(mensagem.msgBuffer);

    sendMessage(&mensagem);
}

int changeName(const char * newName) {

    if(!newName || strlen(newName) > 64 || strlen(newName) == 0) {
        //Nome muito longo!
        return 0;
    }

    strMensagem mensagem;
    mensagem.commandCode = CHANGE_NAME;
    strcpy(mensagem.msgBuffer, newName);
    mensagem.bufferLength = strlen(mensagem.msgBuffer);

    sendMessage(&mensagem);
}

void doHandShake () {
    strMensagem mensagem;
    char buffer[256];
    int bytesTransfered;

    do {
        //Nickname

        WINDOW * nameWindow = drawNameWindow();

        bzero(buffer, 256);
        wgetstr(nameWindow, buffer);

        changeName(buffer);

        bzero(&mensagem, sizeof(strMensagem));

        /* read from the socket */
        bytesTransfered = recv(socketDescriptor, &mensagem, sizeof(strMensagem), MSG_WAITALL);
        if (bytesTransfered < 0) {
            printf("ERROR reading from socket\n");
            exit(1);
        }

        delwin(nameWindow);
    } while(mensagem.commandCode != NAME_CHANGED_OK);
}

int stringStartsWith(char * str1, char * str2) {

    char strBuffer[512];

    if(strlen(str2) > strlen(str1))
        return 0;

    strcpy(strBuffer, str1);
    int i;

    for(i = 0; i < strlen(str2); i++) {
        strBuffer[i] = tolower(strBuffer[i]);
    }

    if (strncmp(strBuffer, str2, strlen(str2)) == 0) {
        return 1;
    }

    else return 0;
}

void getRoomIdRequest(char * roomName) {

    if(!roomName)
        return;

    strMensagem mensagem;
    mensagem.commandCode = GET_ROOM_ID;
    strcpy(mensagem.msgBuffer, roomName);
    mensagem.bufferLength = strlen(mensagem.msgBuffer);

    sendMessage(&mensagem);
}

void parseUserInput(char * userInput) {

    if(!userInput || strlen(userInput) == 0) 
        return;

    strMensagem mensagem;

    if (stringStartsWith(userInput, "/")) {
        // Comando

        if(stringStartsWith(userInput, "/help")) {
            printToChatWindow(" ---- Comandos disponiveis:      ---- ");
            printToChatWindow(" ------------------------------------ ");
            printToChatWindow(" ---- /changename [novo nome]    ---- ");
            printToChatWindow(" ---- /createroom [nome da sala] ---- ");
            printToChatWindow(" ---- /joinroom [nome da sala]   ---- ");
            printToChatWindow(" ---- /leaveroom                 ---- ");
            printToChatWindow(" ---- /quit                      ---- ");
            printToChatWindow(" ------------------------------------ ");
        }
        else if(stringStartsWith(userInput, "/changename ")) {
            char * newName = strchr(userInput, ' ') + 1;
            if(changeName(newName)) {
                printToChatWindow("Nome alterado com sucesso!");
            }
            else {
                printToChatWindow("Erro ao alterar nome!");
            }

        }

        else if(stringStartsWith(userInput, "/createroom ")) {
            char * roomName = strchr(userInput, ' ') + 1;
            createChatRoom(roomName);
        }

        else if(stringStartsWith(userInput, "/leaveroom")) {
            //Entra na sala geral.
            joinChatRoom(0);
        }

        else if(stringStartsWith(userInput, "/joinroom ")) {
            char * roomName = strchr(userInput, ' ') + 1;
            getRoomIdRequest(roomName);
        }

        else if(stringStartsWith(userInput, "/quit")) {
            close(socketDescriptor);
            exit(0);
        }
        
        else {
            //Comando invalido
            printToChatWindow("Comando invalido!");
        }
    }

    else {
        // Mensagem normal
        mensagem.commandCode = SEND_MSG;
        strcpy(mensagem.msgBuffer, userInput);
        mensagem.bufferLength = strlen(mensagem.msgBuffer);

        sendMessage(&mensagem);
    }
}

void mainLoop() {

    char buffer[256];
    int bytesTransfered;

    while(1) {

        bzero(buffer, 256);

        wmove(inputWindow, 1, 2);
        wrefresh(inputWindow);

        wgetstr(inputWindow, buffer);
        clearInputWindow();

        parseUserInput(buffer);
    }

}

void drawWindows() {
    int parent_x, parent_y;
    int inputWindowSize = 3;

    getmaxyx(stdscr, parent_y, parent_x);

    //Desenha janelas
    chatWindow = newwin(parent_y - inputWindowSize, parent_x, 0, 0);
    inputWindow = newwin(inputWindowSize, parent_x, parent_y - inputWindowSize, 0);

    wborder(chatWindow, '|', '|', '-', '-', '+', '+', '+', '+');
    wborder(inputWindow, '|', '|', '-', '-', '+', '+', '+', '+');

    scrollok(chatWindow, 1);

    wrefresh(chatWindow);
    wrefresh(inputWindow);
}


int main(int argc, char *argv[]) {
    int bytesTransfered;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    screenSetup();

    //Passar servidor por parametro
    if(argc < 2) {
        server = gethostbyname("127.0.0.1");
    }
    else {
        server = gethostbyname(argv[1]);
    }

    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(1);
    }
    
    if ((socketDescriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1)  {
        printf("ERROR opening socket\n");
        exit(1);
    }
    
    serv_addr.sin_family = AF_INET;     
    serv_addr.sin_port = htons(PORT);    
    serv_addr.sin_addr = *((struct in_addr *)server->h_addr);
    bzero(&(serv_addr.sin_zero), 8);     
    
    printf("Conectando ao servidor... ");
    
    if (connect(socketDescriptor,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
        printf("ERROR connecting\n");
        exit(1);
    }

    printf("Conectado!\n");

    doHandShake();
    drawWindows();

    pthread_create(&readThreadDescriptor, NULL, readThread, NULL);

    //Sala geral
    joinChatRoom(0);

    mainLoop();
    
    close(socketDescriptor);
    return 0;
}