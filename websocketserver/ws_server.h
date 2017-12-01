#ifndef __WEBSOCKET_SERVER_H__
#define __WEBSOCKET_SERVER_H__

#define WS_MSG_MAX_SIZE 512
typedef enum {
	MSG_LOGIN = 1,
	MSG_CONNECT_TO_MASTER,
	MSG_CHAT_MESSAGE
} MSG_TYPE;

typedef enum {
	CONN_INIT = 1,
	CONN_LOGIN,
	CONN_END,
	CONN_NONE
} CONN_STATUS;


typedef struct {
	int index;
	CONN_STATUS status;
	char username[32];
	unsigned int numberOfText;
	unsigned int lastMessageReceived;
	struct lws *wsi;

} connectionInfo;

typedef struct {
	int id;
	char from[32];
	char message[256];
} chatMessage;


int parseWsMessage(int *userIndex, char *msg, int len);
int startWsClient(const char *serverIp, int serverPort);
void sendMessage(struct lws *wsi, int index, int messageIndex);




#endif
