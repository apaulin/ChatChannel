#include <stdio.h>
#include <stdlib.h>
#include <libwebsockets.h>
#include <jansson.h>

int parseWsMessage(char *msg, int len);
#define WS_MSG_MAX_SIZE 512
static void *wsMsgBuffer = NULL;

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
		default:
			printf ("Callback http %d\n", reason);
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
		lws_service(context, 900000);
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
		}
		else
		{
			printf("Cannot find type in %s\n", wsMsgBuffer);
		}
	}
	
	json_decref(root);
	
	return ret;
}
