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
	MSG_CONNECT_TO_MASTER, //2
	MSG_CHAT_MESSAGE, //3
	MSG_CHAT_PROPAGATE,  //4
	MSG_CTL_MESSAGE, //5
	MSG_LOGIN_TO_MASTER //6
} MSG_TYPE;

typedef enum {
	CONN_DISCONNECTED = 0,
	CONN_INIT = 1,
	CONN_LOGIN, //2
	CONN_END, //3
	CONN_SLAVE, //4
	CONN_BROWSER_TO_SERVER, //5
	CONN_SERVER_SLAVE_TO_SERVER_MASTER, // 6 : The slave side of the link
	CONN_SERVER_MASTER_FROM_SERVER_SLAVE, // 7 : The master side of the link.
	CONN_NONE
} CONN_STATUS;

typedef enum {
	REQ_UNKNOWN = -1,
	REQ_OK = 0,
	REQ_CONNECTION_TO_MASTER_FAILED = 1,
	REQ_MASTER_CANNOT_CONNECT_TO_MASTER = 2,
	REQ_ALREADY_CONNECTED_TO_MASTER = 3,
	REQ_CONNECTION_TO_LOCAL_SERVER_LOST = 4,
	REQ_EMPTY_USERNAME = 5
} REQUEST_CODE;


typedef struct t_connectionInfo connectionInfo;

typedef struct t_requestStruct{
	int indexInArray;
	MSG_TYPE type;
	connectionInfo *connection;
	REQUEST_CODE code;
	char value[WS_MSG_MAX_SIZE];
	char extras[WS_MSG_MAX_SIZE]; 
	char from[WS_MSG_MAX_SIZE]; 
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
void sendUnsolicitedMessage(struct lws *wsi, MSG_TYPE type, unsigned int code, const char *extras, const char *from, const char* value);
requestStruct* getEmptyRequest();
void cleanRequest(requestStruct *req);
void processMessages();
connectionInfo *findAvailableConnectionInfo();



#endif
