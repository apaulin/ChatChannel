#ifndef __WEBSOCKET_SERVER_H__
#define __WEBSOCKET_SERVER_H__

#define WS_MSG_MAX_SIZE 512
#define WS_USERNAME_MAX_LENGTH 32
#define WS_MAX_CONNECTIONS 10
#define WS_MAX_QUEUED_MESSAGES 2
#define WS_WEBSOCKET_PORT 8010


typedef enum {
	MSG_UNKNOWN = 0,
	MSG_LOGIN = 1,
	MSG_CONNECT_TO_MASTER,
	MSG_CHAT_MESSAGE,
	MSG_CHAT_PROPAGATE,
	MSG_CTL_MESSAGE
} MSG_TYPE;

typedef enum {
	CONN_DISCONNECTED = 0,
	CONN_INIT = 1,
	CONN_LOGIN,
	CONN_END,
	CONN_SLAVE,
	CONN_NONE
} CONN_STATUS;



typedef struct CHATMESSAGE{
	int index;
	int number;
	struct timespec time;
	char from[WS_USERNAME_MAX_LENGTH];
	char message[WS_MSG_MAX_SIZE];
	struct CHATMESSAGE *next;
} chatMessage;

typedef struct {
	int index;
	CONN_STATUS status;
	char username[WS_USERNAME_MAX_LENGTH];
	int uid;
	chatMessage *lastMessageProcessed;
	chatMessage *lastMessageSent;
	struct lws *wsi;

} connectionInfo;

typedef struct {
	int index;
	MSG_TYPE type;
	connectionInfo *connection;
	int uid;
	char value[WS_MSG_MAX_SIZE];
	char extras[WS_MSG_MAX_SIZE]; 
} requestStruct;


int parseWsMessage(connectionInfo *connInfo, char *msg, int len);
int parseRequest(char *msg, int len, connectionInfo *connInfo, requestStruct *req);
int startWsClient(const char *serverIp, int serverPort);
void sendNextMessage(struct lws *wsi, connectionInfo *conn);
void sendMessage2(struct lws *wsi, const char *from, const char* msg, MSG_TYPE messageType);
requestStruct* getEmptyRequest();
void addLocalChatMessage(connectionInfo *, const char *msg);
void cleanRequest(requestStruct *req);
void processMessages();




#endif
