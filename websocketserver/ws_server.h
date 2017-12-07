#ifndef __WEBSOCKET_SERVER_H__
#define __WEBSOCKET_SERVER_H__

#define WS_MSG_MAX_SIZE 256
#define WS_MSG_HEADER_SIZE 60
#define WS_MESSAGE_CACHE_SIZE 20
#define WS_MAX_CONNECTIONS 10
typedef enum {
	MSG_LOGIN = 1,
	MSG_CONNECT_TO_MASTER,
	MSG_CHAT_MESSAGE,
	MSG_CHAT_PROPAGATE,
	MSG_TYPE_REF
} MSG_TYPE;

typedef enum {
	CONN_DISCONNECTED = 0,
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
	struct timespec time;
	char from[32];
	char message[WS_MSG_MAX_SIZE];
} chatMessage;


int parseWsMessage(int *userIndex, char *msg, int len);
int startWsClient(const char *serverIp, int serverPort);
void sendChatMessage(struct lws *wsi, int index, int messageIndex, MSG_TYPE messageType);
void sendGenericIntMessage(struct lws *wsi, int messageType, long int value);




#endif
