#ifndef CHATCOMMON_H
#define CHATCOMMON_H

#define MSG_BUFFER_SIZE 1024;

enum CHAT_CMD_CODE {

	SEND_MSG,
	MSG_RECEIVED,
	MSG_BROADCAST,
	CHANGE_NAME,
	NAME_CHANGED_OK,
	CREATE_ROOM,
	JOIN_ROOM,
	LEAVE_ROOM,
	ROOM_JOINED,
	GET_ROOM_ID,
	GET_ROOM_ID_RESPONSE,
	ERROR,
	DISCONNECT
};

typedef enum CHAT_CMD_CODE CHAT_COMMAND_CODE;

struct structMsg {
    CHAT_COMMAND_CODE commandCode;
    
    int bufferLength;
    char msgBuffer[1024];
};

typedef struct structMsg strMensagem;


#endif