#include <stdio.h>
#include <stdlib.h>
#include <libwebsockets.h>
#include <jansson.h>
#include <time.h>
#include "ws_server.h"

static void *wsMsgBuffer = NULL;
static struct lws_context *clientContext = NULL;
static struct lws *clientWebsocket = NULL;
static CONN_STATUS clientWebsocketStatus = CONN_DISCONNECTED; 

static connectionInfo connections[WS_MAX_CONNECTIONS];
static chatMessage chatMessages[WS_MESSAGE_CACHE_SIZE];
static int chatMessageCounter = 0;
static int chatMessageIndex = -1;

static int callback_http(
	struct lws *wsi,
	enum lws_callback_reasons reason,
	void *user,
	void *input,
	size_t len)
{
	int i=0;
	int *index = (int *)user;
	switch (reason) 
	{
		case LWS_CALLBACK_ESTABLISHED:
			printf("New connection from Client \n");
			printf("Sending time reference\n");
			struct timespec ref;
			clock_gettime(CLOCK_REALTIME, &ref);
			sendGenericIntMessage(wsi, (int)MSG_TYPE_REF, ref.tv_sec);
			
			for(i = 0; i < WS_MAX_CONNECTIONS; i++)
			{
				if(connections[i].status == CONN_NONE)
				{
					*index = i;
					connections[i].status = CONN_INIT;
					connections[i].username[0] = 0;
					connections[i].numberOfText = 0;
					connections[i].lastMessageReceived = 0;
					connections[i].wsi = wsi;
					break;
				}
			}
			for(i = chatMessageIndex+1; i < WS_MESSAGE_CACHE_SIZE;i++)
			{
				if (chatMessages[i].id != -1)
				{
					sendChatMessage(wsi, *index, i, MSG_CHAT_MESSAGE);
				}
			}
			for(i = 0; i <= chatMessageIndex;i++)
			{
				if (chatMessages[i].id != -1)
				{
					sendChatMessage(wsi, *index, i, MSG_CHAT_MESSAGE);
				}
			}
			break;
		case LWS_CALLBACK_RECEIVE:
			printf("Received: %s\n", (char *)input);
			parseWsMessage((int *)user, (char *)input, len);
			break;
		case LWS_CALLBACK_SERVER_WRITEABLE:
			printf("LWS_CALLBACK_SERVER_WRITEABLE\n");
			sendChatMessage(wsi, *index, chatMessageIndex, MSG_CHAT_MESSAGE);
			break;
		case LWS_CALLBACK_CLOSED:
			printf("Connection Closed %d\n", *index);
			connections[*index].status = CONN_NONE;
			connections[*index].wsi = NULL;

			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			break;
		default:
			//printf ("Callback http %d\n", reason);
			break;
	}
	
	return 0;

}

void sendChatMessage(struct lws *wsi, int userIndex, int messageIndex, MSG_TYPE messageType)
{
		printf("Writing to %d : %s at %d seconds.\n", userIndex, chatMessages[messageIndex].message, chatMessages[messageIndex].time.tv_sec);
		unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + WS_MSG_HEADER_SIZE + LWS_SEND_BUFFER_POST_PADDING];
		unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
		json_t *msgJson = json_object();
		json_object_set_new(msgJson, "type", json_integer(3));
		json_object_set_new(msgJson, "from", json_string(chatMessages[messageIndex].from));
		json_object_set_new(msgJson, "value", json_string(chatMessages[messageIndex].message));
		json_object_set_new(msgJson, "time", json_integer((unsigned int)chatMessages[messageIndex].time.tv_sec));


		char *msgJsonString = json_dumps(msgJson, JSON_COMPACT);
		int size = sprintf((char *)text, "%s", msgJsonString);		
		lws_write(wsi, text, size, LWS_WRITE_TEXT);
		free(msgJsonString);
		json_decref(msgJson);
}

void sendGenericIntMessage(struct lws *wsi, int messageType, long int value)
{
		printf("Writing %d : %d at %d seconds.\n", messageType, value);
		unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + WS_MSG_MAX_SIZE + LWS_SEND_BUFFER_POST_PADDING];
		unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
		int size = sprintf((char *)text, "{\"type\":%d, \"value\":%d}", messageType, value);
		lws_write(wsi, text, size, LWS_WRITE_TEXT);
}
 


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
			parseWsMessage((int *)user, (char *)input, len);
			break;
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("Client Connection Closed\n");
			clientWebsocketStatus = CONN_DISCONNECTED;
			clientWebsocket = NULL;
			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			if (clientWebsocketStatus == CONN_INIT)
			{
				unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 256 + LWS_SEND_BUFFER_POST_PADDING];
				unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
				int size = sprintf((char *)text, "{\"type\":1,\"value\":\"%s\"}", "SlaveClient");
				lws_write(wsi, text, size, LWS_WRITE_TEXT);
				clientWebsocketStatus = CONN_LOGIN;			
			}
			else
			{
				sendChatMessage(wsi, -1, chatMessageIndex, MSG_CHAT_PROPAGATE);
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
	for(i = 0 ; i < WS_MAX_CONNECTIONS; i++)
	{
		connections[i].index = -1;
		connections[i].status = CONN_NONE;
		connections[i].username[0] = 0;
		connections[i].numberOfText = 0;
		connections[i].lastMessageReceived = 0;
		connections[i].wsi = NULL;
	}
	
	for(i=0; i < WS_MESSAGE_CACHE_SIZE; i++)
	{
		chatMessages[i].id = -1;
		chatMessages[i].from[0] = 0;
		chatMessages[i].message[0] = 0;
	}
	
	printf("Starting WebSocket Server\n");
	struct lws_context_creation_info contextInfo;
	memset(&contextInfo, 0, sizeof(contextInfo));
	if (argc == 2 && strcmp(argv[1], "master") == 0)
	{
		printf("Starting in server mode.\n");
		contextInfo.port = 8020;
	}
	else
	{
		contextInfo.port = 8010;
	}
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
		usleep(50000);
	}
	
	
	return 0;
}


int parseWsMessage(int *connIndex, char *msg, int len)
{
	int ret = 0;
	json_error_t error;
	int i =0;

	if (len >= WS_MSG_MAX_SIZE)
	{
		printf("Message too big %d\n", len);
		return -1;
	}
	else
	{
		memset(wsMsgBuffer, 0, WS_MSG_MAX_SIZE*sizeof(char));
		memcpy(wsMsgBuffer, msg, len);
	}

	json_t *root = json_loads(wsMsgBuffer, 0, &error);
	if (!root)
	{
		printf("Error on parsing JSON line: %d, text: %s \n", error.line, error.text); 
	}
	else
	{
		json_t *type = json_object_get(root, "type");
		json_t *value = NULL;
		json_t *from = NULL;

		const char *valueStr = NULL;
		const char *fromStr = NULL;

		//const char *type_text = NULL;
		int type_int;
		if (type != NULL)
		{
			type_int = json_integer_value(type);
			printf("Type of command : %d : %s\n", type_int, (char *)wsMsgBuffer);
			switch (type_int)
			{
				case MSG_LOGIN:
					if (connections[*connIndex].status == CONN_INIT)
					{
						value = json_object_get(root, "value");
						const char *valueStr = json_string_value(value);
						printf("Login with value %s\n", valueStr);
						strncpy(connections[*connIndex].username, valueStr, 32);
						connections[*connIndex].status = CONN_LOGIN;
					}
					else
					{
						printf("Connection in wrong state to login %d\n", connections[*connIndex].status);						
					}
					break;
				case MSG_CONNECT_TO_MASTER:
					value = json_object_get(root, "value");
					valueStr = json_string_value(value);
					printf("Synching with Server %s\n", valueStr);
					startWsClient(valueStr, 8010);
					break;
				case MSG_CHAT_MESSAGE:
				case MSG_CHAT_PROPAGATE:
					chatMessageIndex++;
					chatMessageIndex = chatMessageIndex%WS_MESSAGE_CACHE_SIZE;
					chatMessages[chatMessageIndex].id = ++chatMessageCounter;
					value = json_object_get(root, "value");
					from = json_object_get(root, "from");
					valueStr = json_string_value(value);
					printf("Got %s\n", valueStr);
					fromStr = json_string_value(from);
					printf("Got %s\n", fromStr);
					strncpy(chatMessages[chatMessageIndex].message, valueStr, 255);
					strncpy(chatMessages[chatMessageIndex].from, fromStr, 32);
					clock_gettime(CLOCK_REALTIME, &chatMessages[chatMessageIndex].time );
					printf("Parsing message request in slot %d --> %s.%d:%d\n", 
					       chatMessageIndex, 
					       chatMessages[chatMessageIndex].message,
					       chatMessages[chatMessageIndex].time.tv_sec,
					       chatMessages[chatMessageIndex].time.tv_nsec);
					for(i = 0; i < WS_MAX_CONNECTIONS; i++)
					{
						if (connections[i].status == CONN_LOGIN)
						{
							if (type_int == MSG_CHAT_MESSAGE || i != *connIndex)
							{
								lws_callback_on_writable(connections[i].wsi);
							}
						}
						else
						{
							printf("Connection slot %d is in %d\n", i, connections[i].status);
						}
					}
					printf("Exiting regular connection, checking connection to server. %d\n", (int)connIndex);
					if (clientWebsocket != NULL && *connIndex != -1)
					{
						printf("Forwarding message to master\n");
						lws_callback_on_writable(clientWebsocket);
					}
					else if (*connIndex == -1)
					{
						printf("Message was from Master, no need to forward it.\n");
					}
					
					break;
				default:
					printf ("Unknown message type %d\n", type_int);
					break;
			}
		}
		else
		{
			printf("Cannot find type in %s\n", (char *)wsMsgBuffer);
		}
	}
	
	json_decref(root);
	
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
	clientWebsocket = lws_client_connect_via_info(&connectInfo);
	clientWebsocketStatus = CONN_INIT;
	lws_callback_on_writable(clientWebsocket);
	
	
	return ret;
}
