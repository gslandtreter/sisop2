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
    wrefresh(inputWindow);

    pthread_mutex_unlock(&screenMutex);
}

void printToChatWindow(char * strMensagem) {

    pthread_mutex_lock(&screenMutex);

    scroll(chatWindow);

    wmove(chatWindow, LINES - 5, 1);
    wprintw(chatWindow, "  %s\n", strMensagem);

    wborder(chatWindow, '|', '|', '-', '-', '+', '+', '+', '+');

    wrefresh(chatWindow);
    wrefresh(inputWindow);

    pthread_mutex_unlock(&screenMutex);
}

int parseReceivedMessage(strMensagem * message) {

    switch(message->commandCode) {

        case SEND_MSG: {

            printToChatWindow(message->msgBuffer);
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
        bytesTransfered = read(socketDescriptor, &receivedMessage, sizeof(strMensagem));
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

void joinChatRoom(int roomId) {

    strMensagem mensagem;

    mensagem.commandCode = JOIN_ROOM;
    sprintf(mensagem.msgBuffer, "%d", roomId);

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

        mensagem.commandCode = CHANGE_NAME;
        strcpy(mensagem.msgBuffer, buffer);
        mensagem.bufferLength = strlen(mensagem.msgBuffer);
        
        sendMessage(&mensagem);

        bzero(&mensagem, sizeof(strMensagem));

        /* read from the socket */
        bytesTransfered = read(socketDescriptor, &mensagem, sizeof(strMensagem));
        if (bytesTransfered < 0) {
            printf("ERROR reading from socket\n");
            exit(1);
        }

        delwin(nameWindow);
    } while(mensagem.commandCode != NAME_CHANGED_OK);
}

void mainLoop() {

    strMensagem mensagem;
    char buffer[256];
    int bytesTransfered;

    while(1) {

        bzero(buffer, 256);

        wmove(inputWindow, 1, 2);
        wrefresh(inputWindow);

        wgetstr(inputWindow, buffer);
        clearInputWindow();

        mensagem.commandCode = SEND_MSG;
        strcpy(mensagem.msgBuffer, buffer);
        mensagem.bufferLength = strlen(mensagem.msgBuffer);
        
        sendMessage(&mensagem);
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

    //Sala geral
    joinChatRoom(0);

    pthread_create(&readThreadDescriptor, NULL, readThread, NULL);
    drawWindows();
    mainLoop();
    
    close(socketDescriptor);
    return 0;
}