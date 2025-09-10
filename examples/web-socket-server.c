#define SDL_WEBSOCKET_ACCEPT_KEY_FUNCTION

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

bool onPreamble(const char*, const char*, const char*, void*);
bool onHeader(const char*, const char*, void*);
bool onOpen(void*);
bool onData(NET_WSPacketType, void*, int);
void onClose(void*);

int main(int argc, char **argv)
{
	const char *interface = NULL;
    Uint16 server_port = 2382;
    int simulate_failure = 0;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if ((SDL_strcmp(arg, "--port") == 0) && (i < (argc-1))) {
            server_port = (Uint16) SDL_atoi(argv[++i]);
        } else if ((SDL_strcmp(arg, "--simulate-failure") == 0) && (i < (argc-1))) {
            simulate_failure = (int) SDL_atoi(argv[++i]);
        } else {
            interface = arg;
        }
    }

	if (!NET_Init()) {
        SDL_Log("NET_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (interface) {
        SDL_Log("Attempting to listen on interface '%s', port %d", interface, (int) server_port);
    } else {
        SDL_Log("Attempting to listen on all interfaces, port %d", (int) server_port);
    }

    NET_Address *server_addr = NULL;
    if (interface) {
        server_addr = NET_ResolveHostname(interface);
        if (!server_addr || (NET_WaitUntilResolved(server_addr, -1) != NET_SUCCESS)) {
            SDL_Log("Failed to resolve interface for '%s': %s", interface, SDL_GetError());
            if (server_addr) {
                NET_UnrefAddress(server_addr);
            }
            NET_Quit();
            SDL_Quit();
            return 1;
        } else {
            SDL_Log("Interface '%s' resolves to '%s' ...", interface, NET_GetAddressString(server_addr));
        }
    }

    NET_Server *server = NET_CreateServer(server_addr, server_port);
    if (!server) {
        SDL_Log("Failed to create server: %s", SDL_GetError());
    } else {
        SDL_Log("Server is ready! Create a web socket connection to port %d", (int) server_port);
        int num_vsockets = 1;
        void *vsockets[128];
        SDL_zeroa(vsockets);
        vsockets[0] = server;
        while (NET_WaitUntilInputAvailable(vsockets, num_vsockets, -1) >= 0) {
            NET_StreamSocket *streamsocket = NULL;
            if (!NET_AcceptClient(server, &streamsocket)) {
                SDL_Log("NET_AcceptClient failed: %s", SDL_GetError());
                break;
            } else if (streamsocket) { // new connection!
                SDL_Log("New connection from %s!", NET_GetAddressString(NET_GetStreamSocketAddress(streamsocket)));
                if (num_vsockets >= (int) (SDL_arraysize(vsockets) - 1)) {
                    SDL_Log("  (too many connections, though, so dropping immediately.)");
                    NET_DestroyStreamSocket(streamsocket);
                } else {
                    if (simulate_failure) {
                        NET_SimulateStreamPacketLoss(streamsocket, simulate_failure);
                    }
                    NET_WSStream * ws = NET_CreateWSStream(streamsocket, onPreamble, onHeader, onOpen, onData, onClose, NULL);
                    if (!ws) {
                    	SDL_Log("NET_CreateWSStream: %s\n", SDL_GetError());
                    	break;
                    }
                    vsockets[num_vsockets++] = ws;
                }
            }

            for (int i = 1; i < num_vsockets; i++) {
                NET_WSStream * ws = (NET_WSStream *) vsockets[i];
            	if(!NET_UpdateWSStream(ws)){
            		SDL_Log("Dropping connection to '%s'", NET_GetAddressString(NET_GetWSStreamAddress(ws)));
                    NET_DestroyWSStream(ws);
                    vsockets[i] = NULL;
                    if (i < (num_vsockets - 1)) {
                        SDL_memmove(&vsockets[i], &vsockets[i+1], sizeof (vsockets[0]) * ((num_vsockets - i) - 1));
                    }
                    num_vsockets--;
                    i--;
            	}
            }
        }

        SDL_Log("Destroying server...");
        NET_DestroyServer(server);
    }

    SDL_Log("Shutting down...");
    NET_Quit();
    SDL_Quit();
    return 0;
}

bool onPreamble(const char *method, const char *route, const char *protocol, void *userdata)
{
	(void)userdata;
	SDL_Log("Method: %s; Route: %s; Protocol: %s\n", method, route, protocol);
	return true;
}

bool onHeader(const char *key, const char *value, void *userdata)
{
	(void)userdata;
	SDL_Log("Header %s=%s\n", key, value);
	return true;
}

bool onOpen(void *userdata)
{
	(void)userdata;
	return true;
}

bool onData(NET_WSPacketType type, void *buf, int len)
{
	if(type == WS_PACKET_TYPE_TEXT) {
		SDL_Log("Received: %.*s\n", len, (char*)buf);
	} else {
		SDL_Log("Received: %d bytes\n", len);
	}
	return true;
}

void onClose(void *userdata)
{
	(void)userdata;
}

bool NET_ConvertToSecWebSocketAcceptKey(SDL_INOUT_Z_CAP(maxlen) char *wsKeyPlusMagicString, int maxlen)
{
	char *buffer = SDL_strdup(wsKeyPlusMagicString);
	if (!buffer) {
		return false;
	}

	// Prepare to perform SHA1 hash
    EVP_MD_CTX *ctx = EVP_MD_CTX_create();
    const EVP_MD *md = EVP_sha1();
    EVP_DigestInit_ex(ctx, md, NULL);

	// SHA1 hash the key + magic string
    EVP_DigestUpdate(ctx, wsKeyPlusMagicString, maxlen);

    unsigned int len;
    EVP_DigestFinal_ex(ctx, (unsigned char*)buffer, &len);
    EVP_MD_CTX_destroy(ctx);
    EVP_cleanup();

  	// Base64 encode the contents of buffer and place into key + magic string
    int size = EVP_EncodeBlock((unsigned char*)wsKeyPlusMagicString, (unsigned char*)buffer, len);
    wsKeyPlusMagicString[size] = '\0';
    SDL_free(buffer);
    return true;
}