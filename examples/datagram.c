#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

static NET_DatagramSocket *sock = NULL;  /* you talk over this, client or server. */
static NET_Address *server_addr = NULL;  /* address of the server you're talking to, NULL if you _are_ the server. */
static Uint16 server_port = 9781;

static void print_usage(const char *prog) {
    SDL_Log("USAGE: %s <hostname|ip|-> [--help] [--server] [--port X] [--simulate-failure Y]", prog);
}

static void run_datagram(int argc, char **argv)
{
    const char *hostname = NULL;
    bool is_server = false;
    int simulate_failure = 0;
    int i;
    NET_Address *socket_address = NULL;
    const SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, NET_PROP_DATAGRAM_SOCKET_ALLOW_BROADCAST_BOOLEAN, true);

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (SDL_strcmp(arg, "--help") == 0) {
            print_usage(argv[0]);
            return;
        } else if (SDL_strcmp(arg, "--server") == 0) {
            is_server = true;
        } else if ((SDL_strcmp(arg, "--port") == 0) && (i < (argc-1))) {
            server_port = (Uint16) SDL_atoi(argv[++i]);
        } else if ((SDL_strcmp(arg, "--simulate-failure") == 0) && (i < (argc-1))) {
            simulate_failure = (int) SDL_atoi(argv[++i]);
        } else {
            hostname = arg;
        }
    }

    simulate_failure = SDL_clamp(simulate_failure, 0, 100);
    if (simulate_failure) {
        SDL_Log("Simulating failure at %d percent", simulate_failure);
    }

    if (!is_server && !hostname) {
        print_usage(argv[0]);
        return;
    }

    if (is_server) {
        if (hostname) {
            SDL_Log("SERVER: Resolving binding hostname '%s' ...", hostname);
            socket_address = NET_ResolveHostname(hostname);
            if (socket_address) {
                if (NET_WaitUntilResolved(socket_address, -1) == NET_FAILURE) {
                    NET_UnrefAddress(socket_address);
                    socket_address = NULL;
                }
            }
        #if 0
        } else {
            int num_addresses;
            NET_Address **addresses;
            addresses = NET_GetLocalAddresses(&num_addresses);
            if (addresses == NULL || num_addresses <= 0) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to to get local addresses: %s", SDL_GetError());
            } else {
                socket_address = addresses[0];
                NET_RefAddress(socket_address);
            }
        #endif
        }
        if (socket_address) {
            SDL_Log("SERVER: Listening on %s:%d.", NET_GetAddressString(socket_address), server_port);
        } else {
            SDL_Log("SERVER: Listening on port %d", server_port);
        }
    } else {
        if (SDL_strcmp(hostname, "-") == 0) {
            SDL_Log("CLIENT: Broadcasting instead of unicasting to a specific address");
        } else {
            SDL_Log("CLIENT: Resolving server hostname '%s' ...", hostname);
            server_addr = NET_ResolveHostname(hostname);
            if (server_addr) {
                if (NET_WaitUntilResolved(server_addr, -1) == NET_FAILURE) {
                    NET_UnrefAddress(server_addr);
                    server_addr = NULL;
                }
            }

            if (!server_addr) {
                SDL_Log("CLIENT: Failed! %s", SDL_GetError());
                SDL_Log("CLIENT: Giving up.");
                return;
            }
        
            SDL_Log("CLIENT: Server is at %s:%d.", NET_GetAddressString(server_addr), (int) server_port);
        }
    }

    /* server _must_ be on the requested port. Clients can take anything available, server will respond to where it sees it come from. */
    sock = NET_CreateDatagramSocket(socket_address, is_server ? server_port : 0, props);
    SDL_DestroyProperties(props);
    NET_UnrefAddress(socket_address);
    if (!sock) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create datagram socket: %s", SDL_GetError());
    } else {
        if (simulate_failure) {
            NET_SimulateDatagramPacketLoss(sock, simulate_failure);
        }

        if (!is_server) {
            Uint8 buf[128];
            SDL_zeroa(buf);
            SDL_Log("CLIENT: %s %d bytes...", server_addr ? "Sending" : "Broadcasting", (int) sizeof (buf));
            NET_SendDatagram(sock, server_addr, server_port, buf, sizeof (buf));
        } else {
            while (NET_WaitUntilInputAvailable((void **) &sock, 1, -1) >= 0) {
                NET_Datagram *dgram = NULL;
                if (NET_ReceiveDatagram(sock, &dgram) && (dgram != NULL)) {
                    SDL_Log("SERVER: got %d-byte datagram from %s:%d", (int) dgram->buflen, NET_GetAddressString(dgram->addr), (int) dgram->port);
                    NET_DestroyDatagram(dgram);
                }
            }
        }
    }

    SDL_Log("Shutting down...");

    NET_UnrefAddress(server_addr);
    server_addr = NULL;
    NET_DestroyDatagramSocket(sock);
    sock = NULL;
}

int main(int argc, char **argv)
{
    if (!NET_Init()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "NET_Init failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    run_datagram(argc, argv);

    NET_Quit();
    return 0;
}

