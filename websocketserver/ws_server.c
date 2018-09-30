#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <jansson.h>
#include <time.h>
#include "dbase_api_obj.h"
#include "ws_server.h"

/* ********** ********** ********** ********** **********
 * The main function as an infinite loop that queues the
 * message and service it (asynchronously) at the next pass
 * 
*/


static void *wsMsgBuffer = NULL;
static struct lws_context *clientContext = NULL;
static connectionInfo *connectionToMasterServer = NULL;
static char tmpBuffer[256];
static int isMaster = 0;


// There is a pool of connections that can contain either client connection
// or a connection to a master server
static connectionInfo connections[WS_MAX_CONNECTIONS];

static requestStruct requests[WS_MAX_QUEUED_REQUESTS];

// lastRequestReceived is global.  Last message processed and last message sent
// is per connection 
static requestStruct *lastRequestReceived = NULL;
//static int glbChatMessageCounter = 0;

extern int DB_get_val32(int elem, int instance, unsigned int *value);


// This callback function is used for the commmunication between the GUI and the server.
// It is part of the libwesockets API
static int callback_http(
	struct lws *wsi,
	enum lws_callback_reasons reason,
	void *user,
	void *input,
	size_t len)
{
	int i=0;
	connectionInfo **connectionPtrPtr= (connectionInfo **)user;
	char buffer[WS_MSG_MAX_SIZE];
	switch (reason) 
	{
		case LWS_CALLBACK_ESTABLISHED:
			printf("New connection from Client \n");
			connectionInfo *clientConn = findAvailableConnectionInfo();
			if(clientConn != NULL)
			{
				*connectionPtrPtr = clientConn;
				clientConn->status = CONN_INIT;
				clientConn->username[0] = 0;
				clientConn->lastRequestProcessed = lastRequestReceived;
				clientConn->lastRequestReplySent = lastRequestReceived;
				clientConn->wsi = wsi;
			}
			else
			{
				printf("Connection limit reached\n");
				lws_close_reason(wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, NULL, 0);
			}
			break;
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
			printf("Connection with MASTER_SERVER is a confirmed success\n");
			*connectionPtrPtr = connectionToMasterServer;
			strcpy(connectionToMasterServer->username, "MasterServer");
			sendUnsolicitedMessage(connectionToMasterServer->wsi, MSG_LOGIN_TO_MASTER, REQ_OK, "", "LocalWebServer", "LocalWebServer");
			sendUnsolicitedMessage(NULL, MSG_CONNECT_TO_MASTER, REQ_OK, "", "LocalWebServer", "Connection to Master Server Successful.");
			connectionToMasterServer->status = CONN_LOGIN;
			break;
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("Connection with MASTER_SERVER failed %d\n", (int)connectionToMasterServer);
			*connectionPtrPtr = connectionToMasterServer;
			connectionToMasterServer->status = CONN_NONE;
			for(i=0; i < WS_MAX_CONNECTIONS; i++)
			{
				if (&connections[i] != connectionToMasterServer && connections[i].status == CONN_LOGIN)
				{
					unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
					unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
					int size = 0;
					size = sprintf((char *)text, "{\"type\":%d, \"code\": %d, \"extras\":\"\", \"from\":\"%s\", \"value\":\"%s\"}", MSG_CTL_MESSAGE, REQ_CONNECTION_TO_MASTER_FAILED, "Local Webserver 2", "Cannot connect to Master Server");
					lws_write(connections[i].wsi, text, size, LWS_WRITE_TEXT);
				}
			}
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			printf("Reception from MASTER SERVER\n");
		case LWS_CALLBACK_RECEIVE:
			memset(tmpBuffer, 0, 256);
			memcpy(tmpBuffer, input ,len);
			printf("Received: %s\n", tmpBuffer);
			parseWsMessage((connectionInfo *)*connectionPtrPtr, tmpBuffer, len);
			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			printf("LWS_CALLBACK_CLIENT_WRITEABLE\n");
			// This is an ugly patch since I don't know how to assign user at the client creation
			*connectionPtrPtr = connectionToMasterServer;
			printf("HERE WE GO (%d)!!!!!\n", (int)(*connectionPtrPtr));
		case LWS_CALLBACK_SERVER_WRITEABLE:
			printf("LWS_CALLBACK_SERVER_WRITEABLE\n");
			printf("SERVER_WRITEABLE of %d %d, %d\n", (int)(*connectionPtrPtr), (*connectionPtrPtr)->indexInArray, (*connectionPtrPtr)->status);
			if ((*connectionPtrPtr)->status == CONN_LOGIN)
			{
				sendRequestReply(wsi, *connectionPtrPtr);
			}
			else
			{
				printf("Cannot reply to a client that is not logged in.\n");
			}
			break;
		case LWS_CALLBACK_CLOSED:
			break;
		case LWS_CALLBACK_WSI_DESTROY:
			printf("Connection Closed %s\n", (*connectionPtrPtr)->username);
			(*connectionPtrPtr)->status = CONN_NONE;
			(*connectionPtrPtr)->connectionType = CONN_NONE;
			(*connectionPtrPtr)->wsi = NULL;
			sendUnsolicitedMessage(NULL, MSG_CTL_MESSAGE, 0, "", (*connectionPtrPtr)->username, "This user has left the communication channel");
			sprintf(buffer, "%s : has left the chat communication channel.", (*connectionPtrPtr)->username);
			for(i = 0; i < WS_MAX_CONNECTIONS; i++)
			{
				if(connections[i].status == CONN_LOGIN)
				{
					//sendUnsolicitedReply(connections[i].wsi, "The Server", buffer, MSG_CHAT_MESSAGE);
				}
			}
			if ((*connectionPtrPtr) == connectionToMasterServer)
			{
				printf("Deleting pointer of connectionToMasterServer\n");
				connectionToMasterServer = NULL;
				
			}
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			break;
		default:
			//printf ("Callback http %d\n", reason);
			break;
	}
	
	return 0;

}

void sendRequestReply(struct lws *wsi, connectionInfo *conn)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	int size = 0;
	
	printf("sendRequestReply\n");
	//printf("sendRequestReply %d-%d\n", (int)wsi, (int)conn->lastRequestReplySent);
	requestStruct *currentReq = NULL;
	if (conn->lastRequestReplySent == NULL)
	{
		currentReq = conn->lastRequestProcessed;
	}
	else
	{
		currentReq = conn->lastRequestReplySent->next;
	}
	//printf("sendRequestReply %d, %d \n", conn->indexInArray, connectionToMasterServer->indexInArray);
	
	switch (currentReq->type)
	{
		case MSG_LOGIN_TO_MASTER:
			printf("This is a login to master\n");
		case MSG_LOGIN:
			printf("Responding to a LOGIN request \n");
			size = sprintf((char *)text, "{\"type\":%d, \"code\":0, \"extras\":\"\", \"from\":\"%s\", \"value\":\"%s\"}", MSG_CHAT_MESSAGE, currentReq->from, currentReq->value);
			lws_write(wsi, text, size, LWS_WRITE_TEXT);
			break;
		case MSG_CHAT_MESSAGE:
		case MSG_CTL_MESSAGE: /* For now only display messages */
			printf("Chat Message!!!!!!\n");
			if (conn != currentReq->connection)
			{
				printf("To a different connection\n");
				//printf("Writing to %s from %s : %s\n", conn->username , currentReq->from, currentReq->value);			
				size = sprintf((char *)text, "{\"type\":%d, \"code\":0, \"extras\":\"\", \"from\":\"%s\", \"value\":\"%s\"}", MSG_CHAT_MESSAGE, currentReq->from, currentReq->value);
				lws_write(wsi, text, size, LWS_WRITE_TEXT);
			}
			else
			{
				printf("Skipping re-sent\n");
			}
			break;
		case MSG_CONNECT_TO_MASTER:
			if (conn == connectionToMasterServer)
			{
				printf("A connection to the MASTER_SERVER was created.  You now need to login.\n");
			}
			else
			{
				if (currentReq->code == REQ_OK)
				{
					size = sprintf((char *)text, "{\"type\":%d, \"code\":0, \"extras\":\"\", \"from\":\"Local Server\", \"value\":\"Connection to MASTER_SERVER was initiated\"}", MSG_CHAT_MESSAGE);
				}
				else if (currentReq->connection == conn)
				{
					size = sprintf((char *)text, "{\"type\":%d, \"code\":%d, \"extras\":\"\", \"from\":\"Local Server\", \"value\":\"Connection to MASTER_SERVER failed\"}", MSG_CTL_MESSAGE, currentReq->code);
				}
				lws_write(wsi, text, size, LWS_WRITE_TEXT);
			}
			break;
		default:
			printf("This type(%d) is not supported\n", currentReq->type);
	}
	conn->lastRequestReplySent = currentReq;
	usleep(10000);
}

/* This is used for messages coming from the server
 * 
 * */
void sendUnsolicitedMessage(struct lws *wsi, MSG_TYPE type, unsigned int code, const char *extras, const char *from, const char* value)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	int size = 0;	
	struct timespec myTime;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &myTime );
	size = sprintf((char *)text, "{\"type\":%d, \"code\": %d, \"extras\": \"%s\", \"from\":\"%s\",\"value\":\"%s\"}", type, code, extras, from, value);
	if (wsi != NULL)
	{
		lws_write(wsi, text, size, LWS_WRITE_TEXT);
	}
	else
	{
		int i = 0;
		for(i = 0; i < WS_MAX_CONNECTIONS; i++)
		{
			if(connections[i].status == CONN_LOGIN)
			{
				lws_write(connections[i].wsi, text, size, LWS_WRITE_TEXT);
			}
		}
	}
	usleep(10000);
}


static struct lws_protocols serverProtocols[] =
{
	{
		"http-only",
		&callback_http,
		sizeof(int),
		0
	},
	{
		NULL, NULL, 0, 0
		
	}
};	
		
int main(int argc, char *argv[])
{
	int i = 0;
	unsigned int featureEnableValue = 0;

	DB_get_val32(DB_ELEM_CHAT_COMM_MODE, 0, &featureEnableValue);
	
	if (featureEnableValue == 0)
	{
		printf("Chat Comm Feature not enabled");
		return -1;
	}
		

	for(i = 0 ; i < WS_MAX_CONNECTIONS; i++)
	{
		connections[i].indexInArray = i;
		connections[i].status = CONN_NONE;
		connections[i].username[0] = 0;
		connections[i].lastRequestProcessed = NULL;
		connections[i].lastRequestReplySent = NULL;
		connections[i].connectionType = CONN_NONE;
		connections[i].wsi = NULL;
	}
		
	
	for(i=0; i < WS_MAX_QUEUED_REQUESTS; i++)
	{
		requests[i].indexInArray = i;
		requests[i].from[0] = 0;
		requests[i].extras[0] = 0;
		requests[i].value[0] = 0;
		if(i < (WS_MAX_QUEUED_REQUESTS -1))
		{
			requests[i].next = &requests[i+1];
		}
		else
		{
			requests[i].next = &requests[0];
		}
	}

	
	printf("Starting WebSocket Server\n");
	struct lws_context_creation_info contextInfo;
	memset(&contextInfo, 0, sizeof(contextInfo));
	contextInfo.port = WS_WEBSOCKET_PORT;
	contextInfo.iface = NULL;
	contextInfo.protocols = serverProtocols;
	contextInfo.extensions = NULL;
	contextInfo.token_limits = NULL;
	contextInfo.ssl_private_key_password = NULL;
	contextInfo.ssl_cert_filepath = NULL;
	contextInfo.ssl_private_key_filepath = NULL;
	contextInfo.ssl_ca_filepath = NULL;
	contextInfo.ssl_cipher_list = NULL;
	contextInfo.http_proxy_address = NULL;
	contextInfo.http_proxy_port = 0;
	contextInfo.gid = -1;
	contextInfo.uid = -1;
	contextInfo.options = 0;
	contextInfo.user = NULL;
	contextInfo.ka_time = 900;
	contextInfo.ka_probes = 5;
	contextInfo.ka_interval = 30;
	
	wsMsgBuffer = malloc(WS_MSG_MAX_SIZE*sizeof(char));
	
	struct lws_context *context = lws_create_context(&contextInfo);
	
	while(1)
	{
		lws_service(context, 250);
		if(connectionToMasterServer != NULL)
		{
			lws_service(clientContext,250);
		}
		processMessages();
		usleep(50000);
	}
	
	
	return 0;
}

void processMessages()
{
	int i = 0;
	for(i = 0; i < WS_MAX_CONNECTIONS; i++)
	{
		if (connections[i].lastRequestProcessed != lastRequestReceived)
		{
			printf("Processing\n");
			requestStruct *tmp = NULL;
			if (connections[i].lastRequestProcessed == NULL)
			{
				tmp = lastRequestReceived;
				connections[i].lastRequestProcessed = lastRequestReceived;
			}
			else
			{
				tmp = connections[i].lastRequestProcessed->next;
				connections[i].lastRequestProcessed = connections[i].lastRequestProcessed->next;
			}

			while(tmp != lastRequestReceived->next)
			{
				if (connections[i].wsi != NULL && connections[i].status == CONN_LOGIN)
				{
					printf("Request processed, preparing to send message\n");
					//printf("Sending message to %d of type %d, ptr: %d\n", i, connections[i].connectionType, (int)connections[i].wsi);
					lws_callback_on_writable(connections[i].wsi);
					connections[i].lastRequestProcessed = tmp;
				}
				tmp = tmp->next;
			}
			//printf("Last request processed of connection %d, is now request %d vs %d\n", connections[i].indexInArray, connections[i].lastRequestProcessed->indexInArray, lastRequestReceived->indexInArray);
		}
	}
	
}


int parseWsMessage(connectionInfo *connInfo, char *msg, int len)
{
	int ret = 0;
	requestStruct *req;
	printf("Parsing new message.\n");
		
	ret = parseRequest(msg, len, connInfo, &req); 
	
	if (ret == 0)
	{
		switch (req->type)
		{
			case MSG_LOGIN_TO_MASTER:
				printf("Received login request from slave server\n");
				isMaster = 1;
				connInfo->connectionType = CONN_SERVER_MASTER_FROM_SERVER_SLAVE;
			case MSG_LOGIN:
				if (connInfo->status == CONN_INIT)
				{
					printf("Login with value %s\n", req->value);
					strncpy(connInfo->username, req->value, WS_USERNAME_MAX_LENGTH);
					//addLocalChatMessage(connInfo, "******New Connection*********");
					connInfo->lastRequestProcessed = lastRequestReceived;
					connInfo->lastRequestReplySent = lastRequestReceived;
					connInfo->status = CONN_LOGIN;
					strncpy(req->from, req->value, WS_USERNAME_MAX_LENGTH);
					sprintf(req->value, "New connection to your server by %s", req->from);
				}
				else
				{
					printf("Connection in wrong state to login %d\n", connInfo->status);						
				}
				break;
			case MSG_CONNECT_TO_MASTER:
				printf("Synching with Server %s\n", req->value);
				if (connectionToMasterServer == NULL && isMaster == 0)
				{
					connectionToMasterServer = findAvailableConnectionInfo();
					connectionToMasterServer->status = CONN_INIT;
					startWsClient(req, connectionToMasterServer);
				}
				else if (isMaster != 0)
				{
					printf("This server IS a master\n");
					req->code = REQ_MASTER_CANNOT_CONNECT_TO_MASTER;
					
				}
				else
				{
					req->code = REQ_ALREADY_CONNECTED_TO_MASTER;
					printf("Already connected to a master or IS the master\n");
				}
				break;
			case MSG_CHAT_MESSAGE:
				break;
			case MSG_CHAT_PROPAGATE:
				break;
			default:
				printf ("Unknown message type %d\n", req->type);
				break;
		}
	}
	else
	{
		printf("Could not parse request, invalid format\n");
	}
		
	return ret;
}

int startWsClient(requestStruct *request, connectionInfo *serverConn)
{
	// serverIp is in value of the request
	int ret = 0;
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = serverProtocols;
	info.gid = -1;
	info.uid = -1;
	
	printf("Creating Client Context with connection %d\n", (int)serverConn);
	clientContext = lws_create_context(&info);
	
	//Connect to server
	struct lws_client_connect_info connectInfo;
	memset(&connectInfo, 0, sizeof(connectInfo));
	connectInfo.context = clientContext;
	connectInfo.address = request->value;
	connectInfo.port = WS_WEBSOCKET_PORT;
	connectInfo.path = "/";
	connectInfo.host = lws_canonical_hostname(clientContext);
	connectInfo.origin = "origin";
	connectInfo.protocol = serverProtocols[0].name;
	printf("Connecting Client Websocket\n");
	serverConn->wsi = lws_client_connect_via_info(&connectInfo);
	if (serverConn->wsi != NULL)
	{
		printf("Connect to MASTER_SERVER attempt %d\n", (int)serverConn->wsi);
		serverConn->status = CONN_INIT;	
		request->code = REQ_OK;
	}
	else
	{
		printf("Fail to connect to MASTER_SERVER\n");
		request->code = REQ_CONNECTION_TO_MASTER_FAILED;
		serverConn->status = CONN_NONE;
		ret = -1;
		
	}
	
	return ret;
}


int parseRequest(char *msg, int len, connectionInfo *connection, requestStruct **req)
{
	int ret = 0;
	json_error_t error;
	printf("Parsing request %s\n", msg);

	if (lastRequestReceived == NULL)
	{
		lastRequestReceived = requests;
	}
	else
	{
		lastRequestReceived = lastRequestReceived->next;
	}
	*req = lastRequestReceived;
	

	if (len >= WS_MSG_MAX_SIZE)
	{
		printf("Message too big %d\n", len);
		ret = -1;
	}
	else
	{
		lastRequestReceived->type = MSG_UNKNOWN;
		lastRequestReceived->code = -1;
		lastRequestReceived->value[0] = 0;
		lastRequestReceived->extras[0] = 0;
		lastRequestReceived->code = REQ_OK;
		memcpy(wsMsgBuffer, msg, len);

		json_t *root = json_loads(msg, 0, &error);
		if (!root)
		{
			printf("Error on parsing JSON line: %d, text: %s \n", error.line, error.text); 
		}
		else
		{
			json_t *type = NULL;
			json_t *code = NULL;
			json_t *value = NULL;
			json_t *extras = NULL;
			json_t *from = NULL;

			const char *valueStr = NULL;
			const char *extrasStr = NULL;
			const char *fromStr = NULL;
			
			type = json_object_get(root, "type");
			code = json_object_get(root, "code");
			value = json_object_get(root, "value");
			extras = json_object_get(root, "extras");
			if (type != NULL && code != NULL && value != NULL && extras != NULL)
			{
				lastRequestReceived->type = json_integer_value(type);
				lastRequestReceived->code = json_integer_value(code);
				valueStr = json_string_value(value);
				strncpy(lastRequestReceived->value, valueStr, WS_MSG_MAX_SIZE);
				extrasStr = json_string_value(extras);
				strncpy(lastRequestReceived->extras, extrasStr, WS_MSG_MAX_SIZE);
				lastRequestReceived->connection = connection;				
			}
			
			from = json_object_get(root, "from");
			fromStr = json_string_value(from);
			if (fromStr != NULL)
			{
				strncpy(lastRequestReceived->from, fromStr, WS_MSG_MAX_SIZE);
			}
			else
			{
				lastRequestReceived->from[0] = 0;
			}

			if (connection->connectionType  == CONN_SERVER_SLAVE_TO_SERVER_MASTER)
			{
				printf("This request comes from the server\n");
				strncpy(connection->username, fromStr, WS_USERNAME_MAX_LENGTH);
				printf("And comes from %s\n", fromStr);
				
				
			}
				
		}
		json_decref(root);
	}
	
	printf("Parsing of type %d successful\n", lastRequestReceived->type);
	return ret;
}

void cleanRequest(requestStruct *req)
{
	req->type = MSG_UNKNOWN;
}


connectionInfo *findAvailableConnectionInfo()
{
	connectionInfo *ret = NULL;
	int i = 0;
	for(i = 0 ; i < WS_MAX_CONNECTIONS; i++)
	{
		if (connections[i].status == CONN_DISCONNECTED || connections[i].status == CONN_NONE)
		{
			ret = &connections[i];
			break;
		}
	}
	
	return ret;
}
