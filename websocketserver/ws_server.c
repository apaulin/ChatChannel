#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <jansson.h>
#include <time.h>
#include "dbase_api_obj.h"
#include "ws_server.h"

static void *wsMsgBuffer = NULL;
static struct lws_context *clientContext = NULL;
//static struct lws *clientWebsocket = NULL;
//static CONN_STATUS clientWebsocketStatus = CONN_DISCONNECTED; 
static connectionInfo serverCurrentStatus;


static connectionInfo connections[WS_MAX_CONNECTIONS];
static chatMessage chatMessages[WS_MAX_QUEUED_MESSAGES];
static chatMessage *lastMessageReceived = NULL;
static int glbChatMessageCounter = 0;

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
	int connectionAvailable = 0;
	switch (reason) 
	{
		case LWS_CALLBACK_ESTABLISHED:
			printf("New connection from Client \n");
			for(i = 0; i < WS_MAX_CONNECTIONS; i++)
			{
				if(connections[i].status == CONN_NONE)
				{
					*connectionPtrPtr = &connections[i];
					connections[i].status = CONN_INIT;
					connections[i].username[0] = 0;
					connections[i].lastMessageProcessed = lastMessageReceived;
					connections[i].lastMessageSent = lastMessageReceived;
					connections[i].wsi = wsi;
					break;
				}
			}
			if (connectionAvailable == 0)
			{
				printf("Connection limit reached\n");
				lws_close_reason(wsi, LWS_CLOSE_STATUS_POLICY_VIOLATION, NULL, 0);
			}
			break;
		case LWS_CALLBACK_RECEIVE:
			printf("Received: %s\n", (char *)input);
			parseWsMessage((connectionInfo *)*connectionPtrPtr, (char *)input, len);
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			//printf("LWS_CALLBACK_SERVER_WRITEABLE\n");
			if ((*connectionPtrPtr)->status == CONN_LOGIN)
			{
				sendNextMessage(wsi, *connectionPtrPtr);
			}
			break;
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_WSI_DESTROY:
			printf("Connection Closed %s\n", (*connectionPtrPtr)->username);
			(*connectionPtrPtr)->status = CONN_NONE;
			(*connectionPtrPtr)->wsi = NULL;
			sprintf(buffer, "%s : has left the chat communication channel.", (*connectionPtrPtr)->username);
			for(i = 0; i < WS_MAX_CONNECTIONS; i++)
			{
				if(connections[i].status == CONN_LOGIN)
				{
					sendMessage2(connections[i].wsi, "The Server", buffer, MSG_CHAT_MESSAGE);
				}
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

void sendNextMessage(struct lws *wsi, connectionInfo *conn)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	int size = 0;
	
	chatMessage *msg = conn->lastMessageSent->next;
	
	//chatMessage *cMsg = &chatMessages[conn->lastMessageReceived % WS_MAX_QUEUED_MESSAGES];
	printf("Writing from %s : %s\n", conn->username , msg->message);
	
	size = sprintf((char *)text, "{\"type\":%d, \"from\":\"%s\",\"value\":\"%s\", \"time\":%d}", MSG_CHAT_MESSAGE, msg->from, msg->message, (unsigned int)msg->time.tv_sec);
	lws_write(wsi, text, size, LWS_WRITE_TEXT);
	conn->lastMessageSent = msg;
	usleep(10000);
}

void sendMessage2(struct lws *wsi, const char *from, const char* msg, MSG_TYPE messageType)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	int size = 0;	
	struct timespec myTime;
	clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &myTime );
	size = sprintf((char *)text, "{\"type\":%d, \"from\":\"%s\",\"value\":\"%s\", \"time\":%d}", messageType, from, msg, (unsigned int)myTime.tv_sec);
	lws_write(wsi, text, size, LWS_WRITE_TEXT);
	usleep(10000);
}

void sendNextMessageFromClientToServer(struct lws *wsi)
{
	unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
	unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
	int size = 0;
	
	
	chatMessage *cMsg = serverCurrentStatus.lastMessageProcessed;
	if (cMsg != NULL)
	{
		printf("Writing from %s : %s\n", "Client to Master",  cMsg->message);
	
		printf("Writing to %s : %s\n", "ClientServer", cMsg->message);
		size = sprintf((char *)text, "{\"type\":%d, \"from\":\"%s\",\"value\":\"%s\", \"time\":%d}", MSG_CHAT_MESSAGE, cMsg->from, cMsg->message, (unsigned int)cMsg->time.tv_sec);
		lws_write(wsi, text, size, LWS_WRITE_TEXT);
		serverCurrentStatus.lastMessageProcessed = serverCurrentStatus.lastMessageProcessed->next;
	}
	usleep(10000);
}


// This callback function is used when this MMR is the client of another radio.
// It is part of the libwesockets API
static int callback_client_http(
	struct lws *wsi,
	enum lws_callback_reasons reason,
	void *user,
	void *input,
	size_t len)
{
	int *index = (int *)user;
	switch (reason) 
	{
		case LWS_CALLBACK_CLIENT_ESTABLISHED:
		case LWS_CALLBACK_ESTABLISHED:
			printf("Client New connection Established \n");
			*index = -1;
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			printf("Client Received: %s\n", (char *)input);
			//parseWsMessage((int *)user, (char *)input, len);
			//parseWsMessage((int *)user, (char *)input, len);
			break;
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("Client Connection Closed\n");
			serverCurrentStatus.status = CONN_DISCONNECTED;
			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			if (serverCurrentStatus.status == CONN_INIT)
			{
				unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
				unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
				int size = sprintf((char *)text, "{\"type\":1,\"value\":\"%s\"}", "SlaveClient");
				lws_write(wsi, text, size, LWS_WRITE_TEXT);
				serverCurrentStatus.status = CONN_LOGIN;			
			}
			else
			{
				sendNextMessageFromClientToServer(wsi);
			}
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			break;
		default:
			//printf ("Client default reason %d\n", reason);
			break;
	}
	
	return 0;

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
		
static struct lws_protocols clientProtocols[] =
{
	{
		"http-only",
		&callback_client_http,
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
		connections[i].index = i;
		connections[i].status = CONN_NONE;
		connections[i].username[0] = 0;
		connections[i].lastMessageProcessed = NULL;
		connections[i].wsi = NULL;
	}
	
	serverCurrentStatus.index = -1;
	serverCurrentStatus.status = CONN_NONE;
	serverCurrentStatus.username[0] = 0;
	serverCurrentStatus.lastMessageProcessed = NULL;
	serverCurrentStatus.wsi = NULL;
	
	
	for(i=0; i < WS_MAX_QUEUED_MESSAGES; i++)
	{
		chatMessages[i].index = i;
		chatMessages[i].number = -1;
		chatMessages[i].from[0] = 0;
		chatMessages[i].message[0] = 0;
		if(i < (WS_MAX_QUEUED_MESSAGES -1))
		{
			chatMessages[i].next = &chatMessages[i+1];
		}
		else
		{
			chatMessages[i].next = &chatMessages[0];
		}
	}

		//theChatMessage.id = -1;
		//theChatMessage.from[0] = 0;
		//theChatMessage.message[0] = 0;



	
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
		lws_service(clientContext,250);
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
		if (connections[i].status == CONN_LOGIN)
		{
			if (connections[i].lastMessageProcessed != lastMessageReceived)
			{
				if (connections[i].lastMessageProcessed == NULL)
				{
					connections[i].lastMessageProcessed = lastMessageReceived;
				}
				else
				{
					chatMessage *tmp = connections[i].lastMessageProcessed->next;
					while(tmp != lastMessageReceived->next)
					{
						printf("Sending message to %d\n", i);
						lws_callback_on_writable(connections[i].wsi);
						tmp = tmp->next;
					}
					connections[i].lastMessageProcessed = lastMessageReceived;
					printf("Last message processed of %d, is now %d\n", connections[i].index, connections[i].lastMessageProcessed->index);
				}
			}
		}
	}
	//lws_callback_on_writable(connInfo->wsi);
	
}


int parseWsMessage(connectionInfo *connInfo, char *msg, int len)
{
	int ret = 0;
	requestStruct req;
		
	ret = parseRequest(msg, len, connInfo, &req); 
	
	if (ret == 0)
	{
		switch (req.type)
		{
			case MSG_LOGIN:
				if (connInfo->status == CONN_INIT)
				{
					printf("Login with value %s\n", req.value);
					strncpy(connInfo->username, req.value, WS_USERNAME_MAX_LENGTH);
					// I remove the next line so when I call lws_callback_on_writable for this it will know to send a login reply
					//connInfo->status = CONN_LOGIN;
					//connInfo->lastMessageReceived = glbChatMessageCounter;
					addLocalChatMessage(connInfo, "******New Connection*********");
					connInfo->lastMessageProcessed = lastMessageReceived;
					connInfo->lastMessageSent = lastMessageReceived;
					connInfo->status = CONN_LOGIN;
					//for(i = 0; i < WS_MAX_CONNECTIONS; i++)
					//{
					//	if (connections[i].status == CONN_LOGIN)
					//	{
					//		lws_callback_on_writable(connections[i].wsi);
					//	}
					//}
					//lws_callback_on_writable(connInfo->wsi);
				}
				else
				{
					printf("Connection in wrong state to login %d\n", connInfo->status);						
				}
				break;
			case MSG_CONNECT_TO_MASTER:
				printf("Synching with Server %s\n", req.value);
				startWsClient(req.value, WS_WEBSOCKET_PORT);
				break;
			case MSG_CHAT_MESSAGE:
				addLocalChatMessage(connInfo, req.value);
				//for(i = 0; i < WS_MAX_CONNECTIONS; i++)
				//{
				//	if (connections[i].status == CONN_LOGIN)
				//	{
				//		lws_callback_on_writable(connections[i].wsi);
				//	}
				//}
				//if (serverCurrentStatus.status == CONN_SLAVE && serverCurrentStatus.wsi != NULL)
				//{
				//	lws_callback_on_writable(serverCurrentStatus.wsi);
				//}
				break;
			case MSG_CHAT_PROPAGATE:
				/*
				chatMessageIndex++;
				chatMessageIndex = chatMessageIndex%WS_MAX_QUEUED_MESSAGES;
				chatMessages[chatMessageIndex].id = ++chatMessageCounter;
				value = json_object_get(root, "value");
				from = json_object_get(root, "from");
				valueStr = json_string_value(value);
				//printf("Got %s\n", valueStr);
				fromStr = json_string_value(from);
				//printf("Got %s\n", fromStr);
				strncpy(chatMessages[chatMessageIndex].message, valueStr, WS_MSG_MAX_SIZE);
				strncpy(chatMessages[chatMessageIndex].from, fromStr, WS_USERNAME_MAX_LENGTH);
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &chatMessages[chatMessageIndex].time );
				printf("Parsing message request in slot %d --> %s.\n", 0, chatMessages[chatMessageIndex].message);
				for(i = 0; i < WS_MAX_CONNECTIONS; i++)
				{
					if (connections[i].status == CONN_LOGIN)
					{
						if (type_int == MSG_CHAT_MESSAGE || connInfo != &connections[i])
						{
							lws_callback_on_writable(connections[i].wsi);
						}
					}
					else
					{
						//printf("Connection slot %d is in %d\n", i, connections[i].status);
					}
				}
				//printf("Exiting regular connection, checking connection to server. %d\n", (int)connIndex);
				if (clientWebsocketStatus.wsi != NULL && connInfo != NULL)
				{
					printf("Forwarding message to master\n");
					lws_callback_on_writable(clientWebsocketStatus.wsi);
				}
				else if (connInfo == NULL)
				{
					printf("Message was from Master, no need to forward it.\n");
				}
				*/
				break;
			default:
				printf ("Unknown message type %d\n", req.type);
				break;
		}
	}
	else
	{
		printf("Could not parse request, invalid format\n");
	}
		
	return ret;
}

int startWsClient(const char *serverIp, int serverPort)
{
	int ret = 0;
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof(info));
	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = clientProtocols;
	info.gid = -1;
	info.uid = -1;
	
	printf("Creating Client Context\n");
	clientContext = lws_create_context(&info);
	
	//Connect to server
	struct lws_client_connect_info connectInfo;
	memset(&connectInfo, 0, sizeof(connectInfo));
	connectInfo.context = clientContext;
	connectInfo.address = serverIp;
	connectInfo.port = serverPort;
	connectInfo.path = "/";
	connectInfo.host = lws_canonical_hostname(clientContext);
	connectInfo.origin = "origin";
	connectInfo.protocol = clientProtocols[0].name;
	printf("Connecting Client Websocket\n");
	serverCurrentStatus.wsi = lws_client_connect_via_info(&connectInfo);
	serverCurrentStatus.status = CONN_INIT;

	lws_callback_on_writable(serverCurrentStatus.wsi);
	
	
	return ret;
}


int parseRequest(char *msg, int len, connectionInfo *connection, requestStruct *req)
{
	int ret = 0;
	json_error_t error;
	printf("Parsing request %s\n", msg); 

	if (len >= WS_MSG_MAX_SIZE)
	{
		printf("Message too big %d\n", len);
		ret = -1;
	}
	else
	{
		req->type = MSG_UNKNOWN;
		req->uid = -1;
		req->value[0] = 0;
		req->extras[0] = 0;
		memcpy(wsMsgBuffer, msg, len);

		json_t *root = json_loads(msg, 0, &error);
		if (!root)
		{
			printf("Error on parsing JSON line: %d, text: %s \n", error.line, error.text); 
		}
		else
		{
			json_t *type = NULL;
			json_t *uid = NULL;
			json_t *value = NULL;
			json_t *extras = NULL;

			const char *valueStr = NULL;
			const char *extrasStr = NULL;
			
			type = json_object_get(root, "type");
			uid = json_object_get(root, "uid");
			value = json_object_get(root, "value");
			extras = json_object_get(root, "extras");
			if (type != NULL && uid != NULL && value != NULL && extras != NULL)
			{
				req->type = json_integer_value(type);
				req->uid = json_integer_value(uid);
				valueStr = json_string_value(value);
				strncpy(req->value, valueStr, WS_MSG_MAX_SIZE);
				extrasStr = json_string_value(extras);
				strncpy(req->extras, extrasStr, WS_MSG_MAX_SIZE);
				req->connection = connection;				
			}
				
		}
		json_decref(root);
	}
	
	
	return ret;
}

void addLocalChatMessage(connectionInfo *conn, const char *msg)
{
	if (lastMessageReceived == NULL)
	{
		lastMessageReceived = chatMessages;
	}
	else
	{
		lastMessageReceived = lastMessageReceived->next;
	}
	lastMessageReceived->number = ++glbChatMessageCounter;
	strncpy(lastMessageReceived->from, conn->username, WS_USERNAME_MAX_LENGTH);
	strncpy(lastMessageReceived->message, msg, WS_MSG_MAX_SIZE);	
}

void cleanRequest(requestStruct *req)
{
	req->type = MSG_UNKNOWN;
}
