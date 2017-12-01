#include <stdio.h>
#include <stdlib.h>
#include <libwebsockets.h>
#include <jansson.h>

int parseWsMessage(char *msg, int len);
#define WS_MSG_MAX_SIZE 512
static void *wsMsgBuffer = NULL;
static struct lws_context *clientContext = NULL;
static struct lws *clientWebsocket = NULL;



static int callback_http(
	struct lws *wsi,
	enum lws_callback_reasons reason,
	void *user,
	void *input,
	size_t len)
{
	switch (reason) 
	{
		case LWS_CALLBACK_ESTABLISHED:
			printf("New connection from Client \n");
			break;
		case LWS_CALLBACK_RECEIVE:
			printf("Received: %s\n", (char *)input);
			parseWsMessage((char *)input, len);
			break;
		case LWS_CALLBACK_CLOSED:
			printf("Connection Closed\n");
			break;
		case LWS_CALLBACK_GET_THREAD_ID:
			break;
		default:
			printf ("Callback http %d\n", reason);
			break;
	}
	
	return 0;

}

static int callback_client_http(
	struct lws *wsi,
	enum lws_callback_reasons reason,
	void *user,
	void *input,
	size_t len)
{
	switch (reason) 
	{
		case LWS_CALLBACK_ESTABLISHED:
			printf("Client New connection Established \n");
			break;
		case LWS_CALLBACK_CLIENT_RECEIVE:
			printf("Client Received: %s\n", (char *)input);
			parseWsMessage((char *)input, len);
			break;
		case LWS_CALLBACK_CLOSED:
		case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
			printf("Client Connection Closed\n");
			break;
		case LWS_CALLBACK_CLIENT_WRITEABLE:
			printf("Got a request to send something to the other user.\n");
			unsigned char buf[LWS_SEND_BUFFER_PRE_PADDING + 256 + LWS_SEND_BUFFER_POST_PADDING];
			unsigned char *text = &buf[LWS_SEND_BUFFER_PRE_PADDING];
			int size = sprintf(text, "Hello From Proxy!");
			lws_write(clientWebsocket, text, size, LWS_WRITE_TEXT);
		case LWS_CALLBACK_GET_THREAD_ID:
			break;
		default:
			printf ("Client default reason %d\n", reason);
			break;
	}
	
	return 0;

}


static struct lws_protocols serverProtocols[] =
{
	{
		"http-only",
		&callback_http,
		0,
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
		0,
		0
	},
	{
		NULL, NULL, 0, 0
		
	}
};	


int main()
{
	printf("Starting WebSocket Server\n");
	struct lws_context_creation_info contextInfo;
	memset(&contextInfo, 0, sizeof(contextInfo));
	contextInfo.port = 8010;
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


int parseWsMessage(char *msg, int len)
{
	int ret = 0;
	json_error_t error;

	if (len >= WS_MSG_MAX_SIZE)
	{
		printf("Message too big %d\n", len);
		ret = -1;
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
		const char *type_text = NULL;
		int type_int;
		if (type != NULL)
		{
			type_int = json_integer_value(type);
			printf("Type of command : %d\n", type_int);
			if (type_int == 12)
			{
				startWsClient("127.0.0.1", 8011);
			}
			else
			{
				lws_callback_on_writable(clientWebsocket);
			}
		}
		else
		{
			printf("Cannot find type in %s\n", wsMsgBuffer);
		}
	}
	
	json_decref(root);
	
	return ret;
}

int startWsClient(const char *serverIp, int serverPort)
{
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
	connectInfo.port = 8010;
	connectInfo.path = "/";
	connectInfo.host = lws_canonical_hostname(clientContext);
	connectInfo.origin = "origin";
	connectInfo.protocol = clientProtocols[0].name;
	printf("Connecting Client Websocket\n");
	clientWebsocket = lws_client_connect_via_info(&connectInfo);
	
	
}
