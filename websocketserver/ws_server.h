#ifndef __WEBSOCKET_SERVER_H__
#define __WEBSOCKET_SERVER_H__

#define WS_MSG_MAX_SIZE 512
#define WS_USERNAME_MAX_LENGTH 32
#define WS_MAX_CONNECTIONS 10
#define WS_MAX_QUEUED_REQUESTS 10
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
	CONN_BROWSER_TO_SERVER,
	CONN_SERVER_SLAVE_TO_SERVER_MASTER,
	CONN_NONE
} CONN_STATUS;

typedef struct t_connectionInfo connectionInfo;

typedef struct t_requestStruct{
	int indexInArray;
	MSG_TYPE type;
	connectionInfo *connection;
	int uid;
	char value[WS_MSG_MAX_SIZE];
	char extras[WS_MSG_MAX_SIZE]; 
	char from[WS_MSG_MAX_SIZE]; 
	int error;
	struct t_requestStruct *next;
} requestStruct;

typedef struct  t_connectionInfo{
	int indexInArray;
	CONN_STATUS status;
	CONN_STATUS connectionType;
	char username[WS_USERNAME_MAX_LENGTH];
	int uid;
	requestStruct *lastRequestProcessed;
	requestStruct *lastRequestReplySent;
	struct lws *wsi;
} connectionInfo;



int parseWsMessage(connectionInfo *connInfo, char *msg, int len);
int parseRequest(char *msg, int len, connectionInfo *connInfo, requestStruct **req);
int startWsClient(requestStruct *request, connectionInfo * serverConn);
void sendRequestReply(struct lws *wsi, connectionInfo *conn);
void sendUnsolicitedReply(struct lws *wsi, const char *from, const char* msg, MSG_TYPE messageType);
requestStruct* getEmptyRequest();
void cleanRequest(requestStruct *req);
void processMessages();
connectionInfo *findAvailableConnectionInfo();



#endif
