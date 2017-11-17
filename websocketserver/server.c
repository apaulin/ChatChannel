#include <stdio.h>
#include <stdlib.h>
#include <libwebsockets.h>
#include <jansson.h>

static int callback_http(
	struct lws *wsi,
	enum lws_callback_reasons reason,
	void *user,
	void *input,
	size_t len)
{

	json_t *root;
	json_error_t error;

	switch (reason) 
	{
		case LWS_CALLBACK_ESTABLISHED:
			printf("New connection from Client \n");
			break;
		case LWS_CALLBACK_RECEIVE:
			printf("Received: %s\n", (char *)input);
			root = json_loads(input, 0, &error);
			if (!root)
			{
				printf("Error on parsing JSON line: %d, text: %s \n", error.line, error.text); 
			}
			else
			{
				json_t *type = json_object_get(root, "type");
				const char *type_text = NULL;
				if (type != NULL)
				{
					type_text = json_string_value(type);
					printf("Type of command : %s\n", type_text);
				}
			}
			
			json_decref(root);
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
	
	struct lws_context *context = lws_create_context(&contextInfo);
	
	while(1)
	{
		lws_service(context, 900000);
		usleep(50000);
	}
	
	
	return 0;
}

