#include <SDL3/SDL.h>

/* Version checks... */

#define SDL_NET_MAJOR_VERSION   3
#define SDL_NET_MINOR_VERSION   0
#define SDL_NET_PATCHLEVEL      0

#define SDL_NET_VERSION(X)                          \
{                                                   \
    (X)->major = SDL_NET_MAJOR_VERSION;             \
    (X)->minor = SDL_NET_MINOR_VERSION;             \
    (X)->patch = SDL_NET_PATCHLEVEL;                \
}

extern DECLSPEC const SDL_version * SDLCALL SDLNet_Linked_Version(void);


/* must call first/last... */

extern DECLSPEC int SDLCALL SDLNet_Init(void);
extern DECLSPEC void SDLCALL SDLNet_Quit(void);

/* hostname resolution API... */

typedef struct SDLNet_Address SDLNet_Address;

extern DECLSPEC SDLNet_Address * SDLCALL SDLNet_ResolveHostname(const char *host);  /* does not block! */
extern DECLSPEC int SDLCALL SDLNet_WaitForResolution(SDLNet_Address *address);  /* blocks until success or failure. Optional. */
extern DECLSPEC int SDLCALL SDLNet_GetAddressStatus(SDLNet_Address *address);  /* 0: still working, -1: failed (check SDL_GetError), 1: ready */
extern DECLSPEC const char * SDLCALL SDLNet_GetAddressString(SDLNet_Address *address);  /* human-readable string, like "127.0.0.1" or "::1" or whatever. NULL if GetAddressStatus != 1. String owned by address! */
extern DECLSPEC SDLNet_Address *SDLCALL SDLNet_RefAddress(SDLNet_Address *address);  /* +1 refcount; SDLNet_ResolveHost starts at 1. Returns `address` for convenience. */
extern DECLSPEC void SDLCALL SDLNet_UnrefAddress(SDLNet_Address *address);  /* when totally unref'd, gets freed. */
extern DECLSPEC void SDLCALL SDLNet_SimulateAddressResolutionLoss(int percent_loss);  /* make resolutions delay at random, fail some percent of them. */

extern DECLSPEC SDLNet_Address **SDLCALL SDLNet_GetLocalAddresses(int *num_addresses);  /* returns NULL-terminated array of SDLNet_Address*, of all known interfaces. */
extern DECLSPEC void SDLCALL SDLNet_FreeLocalAddresses(SDLNet_Address **addresses);  /* unrefs each address, frees array. */

/* Streaming (TCP) API... */

typedef struct SDLNet_StreamSocket SDLNet_StreamSocket;  /* a TCP socket. Reliable transmission, with the usual pros/cons */

/* Clients connect to servers, and then send/receive data on a stream socket. */
extern DECLSPEC SDLNet_StreamSocket * SDLCALL SDLNet_CreateClient(SDLNet_Address *address, Uint16 port);  /* Start connection to address:port. does not block! */
extern DECLSPEC int SDLCALL SDLNet_WaitForConnection(SDLNet_StreamSocket *sock);  /* blocks until success or failure. Optional. */

/* Servers listen for and accept connections from clients, and then send/receive data on a stream socket. */
typedef struct SDLNet_Server SDLNet_Server;   /* a listen socket internally. Binds to a port, accepts connections. */
extern DECLSPEC SDLNet_Server * SDLCALL SDLNet_CreateServer(SDLNet_Address *addr, Uint16 port);  /* Specify NULL for any/all interfaces, or something from GetLocalAddresses */
extern DECLSPEC int SDLCALL SDLNet_WaitForServerIncoming(SDLNet_Server *server);  /* blocks until a client is ready for to be accepted or there's a serious error. Optional. */
extern DECLSPEC int SDLCALL SDLNet_AcceptClient(SDLNet_Server *server, SDLNet_StreamSocket **client_stream);  /* Accept pending connection. Does not block, returns 0 and sets *client_stream=NULL if none available. -1 on errors, zero otherwise. */
extern DECLSPEC void SDLCALL SDLNet_DestroyServer(SDLNet_Server *server);

/* Use a connected socket, whether from a client or server. */
extern DECLSPEC SDLNet_Address * SDLCALL SDLNet_GetStreamSocketAddress(SDLNet_StreamSocket *sock);  /* Get the address of the other side of the connection */
extern DECLSPEC int SDLCALL SDLNet_GetConnectionStatus(SDLNet_StreamSocket *sock);  /* -1: connecting, 0: failed/dropped (check SDL_GetError), 1: okay */
extern DECLSPEC int SDLCALL SDLNet_WriteToStreamSocket(SDLNet_StreamSocket *sock, const void *buf, int buflen);  /* always queues what it can't send immediately. Does not block, -1 on out of memory, dead socket, etc */
extern DECLSPEC int SDLCALL SDLNet_GetStreamSocketPendingWrites(SDLNet_StreamSocket *sock);  /* returns number of bytes still pending to write, or -1 on dead socket, etc. 0 if no data pending to send. */
extern DECLSPEC int SDLCALL SDLNet_WaitForStreamPendingWrites(SDLNet_StreamSocket *sock); /* blocks until all pending data is sent. returns 0 on success, -1 on dead socket, etc. Optional. */
extern DECLSPEC int SDLCALL SDLNet_ReadStreamSocket(SDLNet_StreamSocket *sock, void *buf, int buflen);  /* read up to buflen bytes. Does not block, -1 on dead socket, etc, 0 if no data available. */
extern DECLSPEC void SDLCALL SDLNet_SimulateStreamPacketLoss(SDLNet_StreamSocket *sock, int percent_loss);  /* since streams are reliable, this holds back data and connections for some amount of time, and maybe even drops connections. */
extern DECLSPEC void SDLCALL SDLNet_DestroyStreamSocket(SDLNet_StreamSocket *sock);  /* Destroy your sockets when finished with them. Does not block, handles shutdown internally. */


/* Datagram (UDP) API... */

typedef struct SDLNet_DatagramSocket SDLNet_DatagramSocket;  /* a UDP socket. Unreliable, packet-based transmission, with the usual pros/cons */

typedef struct SDLNet_Datagram
{
    SDLNet_Address *addr;  /* this is unref'd by SDLNet_FreeDatagram. You only need to ref it if you want to keep it. */
    Uint16 port;  /* these do not have to come from the same port the receiver is bound to. */
    Uint8 *buf;
    int buflen;
} SDLNet_Datagram;

extern DECLSPEC int SDLCALL SDLNet_GetMaxDatagramSize(void);  /* Probably just hardcode to 1500? */
extern DECLSPEC SDLNet_DatagramSocket * SDLCALL SDLNet_CreateDatagramSocket(SDLNet_Address *addr, Uint16 port);  /* Specify NULL for any/all interfaces, or something from GetLocalAddresses */
extern DECLSPEC int SDLCALL SDLNet_SendDatagram(SDLNet_DatagramSocket *sock, SDLNet_Address *address, Uint16 port, const void *buf, int buflen);  /* always queues what it can't send immediately. Does not block, -1 on out of memory, dead socket, etc. */
extern DECLSPEC int SDLCALL SDLNet_ReceiveDatagram(SDLNet_DatagramSocket *sock, SDLNet_Datagram **dgram);  /* Get next available packet. Does not block, returns 0 and sets *dgram=NULL if none available. -1 on errors, zero otherwise. */
extern DECLSPEC int SDLCALL SDLNet_GetDatagramSocketPendingWrites(SDLNet_StreamSocket *sock);  /* returns number of bytes still pending to write, or -1 on dead socket, etc. 0 if no data pending to send. */
extern DECLSPEC void SDLCALL SDLNet_FreeDatagram(SDLNet_Datagram *dgram);  /* call this on return value from SDLNet_ReceiveDatagram when you're done with it. */
extern DECLSPEC void SDLCALL SDLNet_SimulateDatagramPacketLoss(SDLNet_DatagramSocket *sock, int percent_loss);
extern DECLSPEC void SDLCALL SDLNet_DestroyDatagramSocket(SDLNet_DatagramSocket *sock);  /* Destroy your sockets when finished with them. Does not block. */


