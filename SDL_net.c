#include "SDL_net.h"

#ifdef __WINDOWS__
#define WIN32_LEAN_AND_MEAN 1
#define _WIN32_WINNT 0x0600  /* we need APIs that didn't arrive until Windows Vista. */
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET Socket;
typedef int SockLen;
typedef SOCKADDR_STORAGE AddressStorage;
static int write(SOCKET s, const char *buf, size_t count) {
    return send(s, buf, count, 0);
}
static int read(SOCKET s, char *buf, size_t count) {
    WSABUF wsabuf;
    wsabuf.buf = buf;
    wsabuf.len = count;
    DWORD count_received;
    int res = WSARecv(s, &wsabuf, 1, &count_received, 0, NULL, NULL);
    if (res != 0) {
        SDL_SetError("WSARecv(%d)", WSAGetLastError());
        return -1;
    }
    return (int)count_received;
}
#define EAI_SYSTEM 0
#define poll WSAPoll
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
typedef int Socket;
typedef socklen_t SockLen;
typedef struct sockaddr_storage AddressStorage;
#endif

typedef enum SDLNet_SocketType
{
    SOCKETTYPE_STREAM,
    SOCKETTYPE_DATAGRAM,
    SOCKETTYPE_SERVER
} SDLNet_SocketType;


const SDL_version *SDLNet_Linked_Version(void)
{
    static const SDL_version linked_version = {
        SDL_NET_MAJOR_VERSION, SDL_NET_MINOR_VERSION, SDL_NET_PATCHLEVEL
    };
    return &linked_version;
}

struct SDLNet_Address
{
    char *hostname;
    char *human_readable;
    char *errstr;
    SDL_AtomicInt refcount;
    SDL_AtomicInt status;  // 0==in progress, 1==resolved, -1==error
    struct addrinfo *ainfo;
    SDLNet_Address *resolver_next;  // a linked list for the resolution job queue.
};

#define MIN_RESOLVER_THREADS 2
#define MAX_RESOLVER_THREADS 10

static SDLNet_Address *resolver_queue = NULL;
static SDL_Thread *resolver_threads[MAX_RESOLVER_THREADS];
static SDL_Mutex *resolver_lock = NULL;
static SDL_Condition *resolver_condition = NULL;
static SDL_AtomicInt resolver_shutdown;
static SDL_AtomicInt resolver_num_threads;
static SDL_AtomicInt resolver_num_requests;
static SDL_AtomicInt resolver_percent_loss;

// I really want an SDL_random().  :(
static int random_seed = 0;
static int RandomNumber(void)
{
    // this is POSIX.1-2001's potentially bad suggestion, but we're not exactly doing cryptography here.
    random_seed = random_seed * 1103515245 + 12345;
    return (int) ((unsigned int) (random_seed / 65536) % 32768);
}

// between lo and hi (inclusive; it can return lo or hi itself, too!).
static int RandomNumberBetween(const int lo, const int hi)
{
    return (RandomNumber() % (hi + 1 - lo)) + lo;
}

// !!! FIXME: replace strerror.
// !!! FIXME: replace errno.
static int LastSocketError(void)
{
#ifdef __WINDOWS__
    return WSAGetLastError();
#else
    return errno;
#endif
}

static int CloseSocketHandle(Socket handle)
{
#ifdef __WINDOWS__
    return closesocket(handle);
#else
    return close(handle);
#endif
}

// this blocks!
static int ResolveAddress(SDLNet_Address *addr)
{
    SDL_assert(addr != NULL);  // we control all this, so this shouldn't happen.
    struct addrinfo *ainfo = NULL;
    int rc;

//SDL_Log("getaddrinfo '%s'", addr->hostname);
    rc = getaddrinfo(addr->hostname, NULL, NULL, &ainfo);
//SDL_Log("rc=%d", rc);
    if (rc != 0) {
        addr->errstr = SDL_strdup((rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
        return -1;  // error
    } else if (ainfo == NULL) {
        addr->errstr = SDL_strdup("Unknown error (query succeeded but result was NULL!)");
        return -1;
    }

    char buf[128];
    rc = getnameinfo(ainfo->ai_addr, ainfo->ai_addrlen, buf, sizeof (buf), NULL, 0, NI_NUMERICHOST);
    if (rc != 0) {
        addr->errstr = SDL_strdup((rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
        freeaddrinfo(ainfo);
        return -1;  // error
    }

    addr->human_readable = SDL_strdup(buf);
    addr->ainfo = ainfo;
    return 1;  // success (zero means "still in progress").
}

static int SDLCALL ResolverThread(void *data)
{
    const int threadnum = (int) ((size_t) data);
    //SDL_Log("ResolverThread #%d starting up!", threadnum);

    SDL_LockMutex(resolver_lock);

    while (!SDL_AtomicGet(&resolver_shutdown)) {
        SDLNet_Address *addr = SDL_AtomicGetPtr((void **) &resolver_queue);
        if (!addr) {
            if (SDL_AtomicGet(&resolver_num_threads) > MIN_RESOLVER_THREADS) {  // nothing pending and too many threads waiting in reserve? Quit.
                SDL_DetachThread(resolver_threads[threadnum]);  // detach ourselves so no one has to wait on us.
                SDL_AtomicSetPtr((void **) &resolver_threads[threadnum], NULL);
                break;  // we quit. They'll spawn new threads if necessary.
            }

            // Block until there's something to do.
            SDL_WaitCondition(resolver_condition, resolver_lock);  // surrenders the lock, sleeps until alerted, then relocks.
            continue;  // check for shutdown and new work again!
        }

        SDL_AtomicSetPtr((void **) &resolver_queue, addr->resolver_next);   // take this task off the list, then release the lock so others can work.
        SDL_UnlockMutex(resolver_lock);

        //SDL_Log("ResolverThread #%d got new task ('%s')", threadnum, addr->hostname);

        const int simulated_loss = SDL_AtomicGet(&resolver_percent_loss);

        if (simulated_loss && (RandomNumberBetween(0, 100) > simulated_loss)) {
            // won the percent_loss lottery? Delay resolving this address between 250 and 7000 milliseconds
            SDL_Delay(RandomNumberBetween(250, 2000 + (50 * simulated_loss)));
        }

        int outcome;
        if (!simulated_loss || (RandomNumberBetween(0, 100) > simulated_loss)) {
            outcome = ResolveAddress(addr);            
        } else {
            outcome = -1;
            addr->errstr = SDL_strdup("simulated failure");
        }

        SDL_AtomicSet(&addr->status, outcome);
        //SDL_Log("ResolverThread #%d finished current task (%s, '%s' => '%s')", threadnum, (outcome < 0) ? "failure" : "success", addr->hostname, (outcome < 0) ? addr->errstr : addr->human_readable);

        SDLNet_UnrefAddress(addr);  // we're done with it, but others might still own it.

        SDL_AtomicAdd(&resolver_num_requests, -1);

        // okay, we're done with this task, grab the lock so we can see what's next.
        SDL_LockMutex(resolver_lock);
        SDL_BroadcastCondition(resolver_condition);  // wake up anything waiting on results, and also give all resolver threads a chance to see if they are still needed.
    }

    SDL_AtomicAdd(&resolver_num_threads, -1);
    SDL_UnlockMutex(resolver_lock);  // we're quitting, let go of the lock.

    //SDL_Log("ResolverThread #%d ending!", threadnum);
    return 0;
}

static SDL_Thread *SpinResolverThread(const int num)
{
    char name[16];
    SDL_snprintf(name, sizeof (name), "SDLNetRslv%d", num);
    SDL_assert(resolver_threads[num] == NULL);
    SDL_AtomicAdd(&resolver_num_threads, 1);
    resolver_threads[num] = SDL_CreateThreadWithStackSize(ResolverThread, name, 64 * 1024, (void *) ((size_t) num));
    if (!resolver_threads[num]) {
        SDL_AtomicAdd(&resolver_num_threads, -1);
    }
    return resolver_threads[num];
}

static void DestroyAddress(SDLNet_Address *addr)
{
    if (addr) {
        if (addr->ainfo) {
            freeaddrinfo(addr->ainfo);
        }
        SDL_free(addr->hostname);
        SDL_free(addr->human_readable);
        SDL_free(addr->errstr);
        SDL_free(addr);
    }
}

int SDLNet_Init(void)
{
    char *origerrstr = NULL;

    #ifdef __WINDOWS__
    WSADATA data;
    if (WSAStartup(MAKEWORD(1, 1), &data) != 0) {
        return SDL_SetError("WSAStartup() failed");
    }
    #else
    signal(SIGPIPE, SIG_IGN);
    #endif

    SDL_zeroa(resolver_threads);
    SDL_AtomicSet(&resolver_shutdown, 0);
    SDL_AtomicSet(&resolver_num_threads, 0);
    SDL_AtomicSet(&resolver_num_requests, 0);
    SDL_AtomicSet(&resolver_percent_loss, 0);
    resolver_queue = NULL;

    resolver_lock = SDL_CreateMutex();
    if (!resolver_lock) {
        goto failed;
    }

    resolver_condition = SDL_CreateCondition();
    if (!resolver_condition) {
        goto failed;
    }

    for (int i = 0; i < MIN_RESOLVER_THREADS; i++) {
        if (!SpinResolverThread(i)) {
            goto failed;
        }
    }

    random_seed = (int) ((unsigned int) (SDL_GetPerformanceCounter() & 0xFFFFFFFF));

    return 0;  // good to go.

failed:
    origerrstr = SDL_strdup(SDL_GetError());

    SDLNet_Quit();

    if (origerrstr) {
        SDL_SetError("%s", origerrstr);
        SDL_free(origerrstr);
    }

    return -1;
}

void SDLNet_Quit(void)
{
    if (resolver_lock && resolver_condition) {
        SDL_LockMutex(resolver_lock);
        SDL_AtomicSet(&resolver_shutdown, 1);
        for (int i = 0; i < SDL_arraysize(resolver_threads); i++) {
            if (resolver_threads[i]) {
                SDL_BroadcastCondition(resolver_condition);
                SDL_UnlockMutex(resolver_lock);
                SDL_WaitThread(resolver_threads[i], NULL);
                SDL_LockMutex(resolver_lock);
                resolver_threads[i] = NULL;
            }
        }
        SDL_UnlockMutex(resolver_lock);
    }

    SDL_AtomicSet(&resolver_shutdown, 0);
    SDL_AtomicSet(&resolver_num_threads, 0);
    SDL_AtomicSet(&resolver_num_requests, 0);
    SDL_AtomicSet(&resolver_percent_loss, 0);

    if (resolver_condition) {
        SDL_DestroyCondition(resolver_condition);
        resolver_condition = NULL;
    }

    if (resolver_lock) {
        SDL_DestroyMutex(resolver_lock);
        resolver_lock = NULL;
    }

    resolver_queue = NULL;

    #ifdef __WINDOWS__
    WSACleanup();
    #endif
}

SDLNet_Address *SDLNet_ResolveHostname(const char *host)
{
    SDLNet_Address *addr = SDL_calloc(1, sizeof (SDLNet_Address));
    if (!addr) {
        SDL_OutOfMemory();
        return NULL;
    }

    addr->hostname = SDL_strdup(host);
    if (!addr->hostname) {
        SDL_free(addr);
        SDL_OutOfMemory();
        return NULL;
    }

    SDL_AtomicSet(&addr->refcount, 2);  // one for creation, one for the resolver thread to unref when done.

    SDL_LockMutex(resolver_lock);

    // !!! FIXME: this should append to the list, not prepend; as is, new requests will make existing pending requests take longer to start processing.
    SDL_AtomicSetPtr((void **) &addr->resolver_next, SDL_AtomicGetPtr((void **) &resolver_queue));
    SDL_AtomicSetPtr((void **) &resolver_queue, addr);

    const int num_threads = SDL_AtomicGet(&resolver_num_threads);
    const int num_requests = SDL_AtomicAdd(&resolver_num_requests, 1) + 1;
    //SDL_Log("num_threads=%d, num_requests=%d", num_threads, num_requests);
    if ((num_requests >= num_threads) && (num_threads < MAX_RESOLVER_THREADS)) {  // all threads are busy? Maybe spawn a new one.
        // if this didn't actually spin one up, it is what it is...the existing threads will eventually get there.
        for (int i = 0; i < SDL_arraysize(resolver_threads); i++) {
            if (!resolver_threads[i]) {
                SpinResolverThread(i);
                break;
            }
        }
    }

    SDL_SignalCondition(resolver_condition);
    SDL_UnlockMutex(resolver_lock);

    return addr;
}

int SDLNet_WaitUntilResolved(SDLNet_Address *addr, Sint32 timeout)
{
    if (!addr) {
        return SDL_InvalidParamError("address");  // obviously nothing to wait for.
    }

    // we _could_ use a different lock for this, but this is Good Enough.

    if (timeout) {
        SDL_LockMutex(resolver_lock);
        if (timeout < 0) {
            while (SDL_AtomicGet(&addr->status) == 0) {
                SDL_WaitCondition(resolver_condition, resolver_lock);
            }
        } else {
            const Uint64 endtime = (SDL_GetTicks() + timeout);
            SDL_LockMutex(resolver_lock);
            while (SDL_AtomicGet(&addr->status) == 0) {
                const Uint64 now = SDL_GetTicks();
                if (now >= endtime) {
                    break;
                }
                SDL_WaitConditionTimeout(resolver_condition, resolver_lock, (endtime - now));
            }
        }
        SDL_UnlockMutex(resolver_lock);
    }

    return SDLNet_GetAddressStatus(addr);  // so we set the error string if necessary.
}

int SDLNet_GetAddressStatus(SDLNet_Address *addr)
{
    if (!addr) {
        return SDL_InvalidParamError("address");
    }
    const int retval = SDL_AtomicGet(&addr->status);
    if (retval == -1) {
        SDL_SetError("%s", (const char *) SDL_AtomicGetPtr((void **) &addr->errstr));
    }
    return retval;
}

const char *SDLNet_GetAddressString(SDLNet_Address *addr)
{
    return addr ? (const char *) SDL_AtomicGetPtr((void **) &addr->human_readable) : NULL;
}

int SDLNet_CompareAddresses(const SDLNet_Address *sdlneta, const SDLNet_Address *sdlnetb)
{
    const struct addrinfo *a;
    const struct addrinfo *b;

    if (sdlneta == sdlnetb) {  // same pointer?
        return 0;
    } else if (sdlneta && !sdlnetb) {
        return -1;
    } else if (!sdlneta && sdlnetb) {
        return 1;
    }

    a = sdlneta->ainfo;
    b = sdlnetb->ainfo;
    if (a == b) {  // same pointer?
        return 0;
    } else if (a && !b) {
        return -1;
    } else if (!a && b) {
        return 1;
    } else if (a->ai_family < b->ai_family) {
        return -1;
    } else if (a->ai_family > b->ai_family) {
        return 1;
    } else if (a->ai_addrlen < b->ai_addrlen) {
        return -1;
    } else if (a->ai_addrlen > b->ai_addrlen) {
        return 1;
    }

    return SDL_memcmp(a->ai_addr, b->ai_addr, a->ai_addrlen);
}

SDLNet_Address *SDLNet_RefAddress(SDLNet_Address *addr)
{
    if (addr) {
        SDL_AtomicIncRef(&addr->refcount);
    }
    return addr;
}

void SDLNet_UnrefAddress(SDLNet_Address *addr)
{
    if (addr && SDL_AtomicDecRef(&addr->refcount)) {
        DestroyAddress(addr);
    }
}

void SDLNet_SimulateAddressResolutionLoss(int percent_loss)
{
    percent_loss = SDL_min(100, percent_loss);
    percent_loss = SDL_max(0, percent_loss);
    SDL_AtomicSet(&resolver_percent_loss, percent_loss);
}

SDLNet_Address **SDLNet_GetLocalAddresses(int *num_addresses)
{
    // !!! FIXME: write me!
    return NULL;
}

void SDLNet_FreeLocalAddresses(SDLNet_Address **addresses)
{
    if (addresses) {
        for (int i = 0; addresses[i] != NULL; i++) {
            SDLNet_UnrefAddress(addresses[i]);
        }
        SDL_free(addresses);
    }
}

static struct addrinfo *MakeAddrInfoWithPort(const SDLNet_Address *addr, const int socktype, const Uint16 port)
{
    const struct addrinfo *ainfo = addr ? addr->ainfo : NULL;
    SDL_assert(!addr || ainfo);

    // we need to set up a sockaddr with the port in it for connect(), etc, which is kind of a pain, since we
    // want to keep things generic and also not set up a port at resolve time.
    struct addrinfo hints;
    SDL_zero(hints);
    hints.ai_family = ainfo ? ainfo->ai_family : AF_UNSPEC;
    hints.ai_socktype = socktype;
    //hints.ai_protocol = ainfo ? ainfo->ai_protocol : 0;
    hints.ai_flags = AI_NUMERICHOST | AI_NUMERICSERV | (!ainfo ? AI_PASSIVE : 0);

    char service[16];
    SDL_snprintf(service, sizeof (service), "%d", (int) port);

    struct addrinfo *addrwithport = NULL;
    int rc = getaddrinfo(addr ? addr->human_readable : NULL, service, &hints, &addrwithport);
    if (rc != 0) {
        SDL_SetError("Failed to prepare address with port: %s", (rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
        return NULL;
    }

    return addrwithport;
}


struct SDLNet_StreamSocket
{
    SDLNet_SocketType socktype;
    SDLNet_Address *addr;
    Uint16 port;
    Socket handle;
    int status;
    Uint8 *pending_output_buffer;
    int pending_output_len;
    int pending_output_allocation;
    int percent_loss;
    Uint64 simulated_failure_until;
};

static int MakeSocketNonblocking(Socket handle)
{
    #ifdef __WINDOWS__
    DWORD one = 1;
    return ioctlsocket(handle, FIONBIO, &one);
    #else
    return fcntl(handle, F_SETFL, fcntl(handle, F_GETFL, 0) | O_NONBLOCK);
    #endif
}

static SDL_bool WouldBlock(const int err)
{
    #ifdef __WINDOWS__
    return (err == WSAEWOULDBLOCK) ? SDL_TRUE : SDL_FALSE;
    #else
    return ((err == EWOULDBLOCK) || (err == EAGAIN) || (err == EINPROGRESS)) ? SDL_TRUE : SDL_FALSE;
    #endif
}

SDLNet_StreamSocket *SDLNet_CreateClient(SDLNet_Address *addr, Uint16 port)
{
    if (addr == NULL) {
        SDL_InvalidParamError("address");
        return NULL;
    } else if (SDL_AtomicGet(&addr->status) != 1) {
        SDL_SetError("Address is not resolved");
        return NULL;
    }

    SDLNet_StreamSocket *sock = (SDLNet_StreamSocket *) SDL_calloc(1, sizeof (SDLNet_StreamSocket));
    if (!sock) {
        SDL_OutOfMemory();
        return NULL;
    }

    sock->socktype = SOCKETTYPE_STREAM;
    sock->addr = addr;
    sock->port = port;

    // we need to set up a sockaddr with the port in it for connect(), which is kind of a pain, since we
    // want to keep things generic and also not set up a port at resolve time.
    struct addrinfo *addrwithport = MakeAddrInfoWithPort(addr, SOCK_STREAM, port);
    if (!addrwithport) {
        SDL_free(sock);
        return NULL;
    }

    sock->handle = socket(addrwithport->ai_family, addrwithport->ai_socktype, addrwithport->ai_protocol);
    if (sock->handle == INVALID_SOCKET) {
        SDL_SetError("Failed to create socket: %s", strerror(errno));
        freeaddrinfo(addrwithport);
        SDL_free(sock);
        return NULL;
    }

    if (MakeSocketNonblocking(sock->handle) < 0) {
        CloseSocketHandle(sock->handle);
        freeaddrinfo(addrwithport);
        SDL_free(sock);
        SDL_SetError("Failed to make new socket non-blocking");
        return NULL;
    }

    const int rc = connect(sock->handle, addrwithport->ai_addr, addrwithport->ai_addrlen);

    freeaddrinfo(addrwithport);

    if (rc == SOCKET_ERROR) {
        const int err = LastSocketError();
        if (!WouldBlock(err)) {
            SDL_SetError("Connection failed at startup: %s", strerror(err));
            CloseSocketHandle(sock->handle);
            SDL_free(sock);
            return NULL;
        }
    }

    SDLNet_RefAddress(addr);
    return sock;
}

static int CheckClientConnection(SDLNet_StreamSocket *sock, int timeoutms)
{
    if (!sock) {
        return SDL_InvalidParamError("sock");
    } else if (sock->status == 0) {  // still pending?
        /*!!! FIXME: add this later?
        if (sock->simulated_failure_ticks) {
            if (SDL_GetTicks() >= sock->simulated_failure_ticks) {
                sock->status = SDL_SetError("simulated failure");
        } else */
        if (SDLNet_WaitUntilInputAvailable((void **) &sock, 1, timeoutms) == -1) {
            sock->status = -1;  // just abandon the whole enterprise.
        }
    }
    return sock->status;
}

int SDLNet_WaitUntilConnected(SDLNet_StreamSocket *sock, Sint32 timeout)
{
    return CheckClientConnection(sock, (int) timeout);
}

int SDLNet_GetConnectionStatus(SDLNet_StreamSocket *sock)
{
    return CheckClientConnection(sock, 0);
}


struct SDLNet_Server
{
    SDLNet_SocketType socktype;
    SDLNet_Address *addr;  // bound to this address (NULL for any).
    Uint16 port;
    Socket handle;
};

SDLNet_Server *SDLNet_CreateServer(SDLNet_Address *addr, Uint16 port)
{
    if (addr && SDL_AtomicGet(&addr->status) != 1) {
        SDL_SetError("Address is not resolved");  // strictly speaking, this should be a local interface, but a resolved address can fail later.
        return NULL;
    }

    SDLNet_Server *server = (SDLNet_Server *) SDL_calloc(1, sizeof (SDLNet_Server));
    if (!server) {
        SDL_OutOfMemory();
        return NULL;
    }

    server->socktype = SOCKETTYPE_SERVER;
    server->addr = addr;
    server->port = port;

    struct addrinfo *addrwithport = MakeAddrInfoWithPort(addr, SOCK_STREAM, port);
    if (!addrwithport) {
        SDL_free(server);
        return NULL;
    }

    server->handle = socket(addrwithport->ai_family, addrwithport->ai_socktype, addrwithport->ai_protocol);
    if (server->handle == INVALID_SOCKET) {
        SDL_SetError("Failed to create listen socket: %s", strerror(errno));
        freeaddrinfo(addrwithport);
        SDL_free(server);
        return NULL;
    }

    if (MakeSocketNonblocking(server->handle) < 0) {
        CloseSocketHandle(server->handle);
        freeaddrinfo(addrwithport);
        SDL_free(server);
        SDL_SetError("Failed to make new listen socket non-blocking");
        return NULL;
    }

    int rc = bind(server->handle, addrwithport->ai_addr, addrwithport->ai_addrlen);
    freeaddrinfo(addrwithport);

    if (rc == SOCKET_ERROR) {
        const int err = LastSocketError();
        SDL_assert(!WouldBlock(err));  // binding shouldn't be a blocking operation.
        SDL_SetError("Failed to bind listen socket: %s", strerror(err));
        CloseSocketHandle(server->handle);
        SDL_free(server);
        return NULL;
    }

    rc = listen(server->handle, 16);
    if (rc == SOCKET_ERROR) {
        const int err = LastSocketError();
        SDL_assert(!WouldBlock(err));  // listen shouldn't be a blocking operation.
        SDL_SetError("Failed to listen on socket: %s", strerror(err));
        CloseSocketHandle(server->handle);
        SDL_free(server);
        return NULL;
    }

    SDLNet_RefAddress(addr);
    return server;
}

int SDLNet_AcceptClient(SDLNet_Server *server, SDLNet_StreamSocket **client_stream)
{
    if (!client_stream) {
        return SDL_InvalidParamError("client_stream");
    }

    *client_stream = NULL;

    if (!server) {
        return SDL_InvalidParamError("server");
    }

    AddressStorage from;
    SockLen fromlen = sizeof (from);
    const Socket handle = accept(server->handle, (struct sockaddr *) &from, &fromlen);
    if (handle == INVALID_SOCKET) {
        const int err = LastSocketError();
        return WouldBlock(err) ? 0 : SDL_SetError("Failed to accept new connection: %s", strerror(err));
    }

    if (MakeSocketNonblocking(handle) < 0) {
        CloseSocketHandle(handle);
        return SDL_SetError("Failed to make incoming socket non-blocking");
    }

    // !!! FIXME: this all seems inefficient in the name of keeping addresses generic.
    char hostbuf[128];
    char portbuf[16];
    const int rc = getnameinfo((struct sockaddr *) &from, fromlen, hostbuf, sizeof (hostbuf), portbuf, sizeof (portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
        CloseSocketHandle(handle);
        return SDL_SetError("Failed to determine incoming connection's address: %s", (rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
    }

    SDLNet_Address *fromaddr = (SDLNet_Address *) SDL_calloc(1, sizeof (SDLNet_Address));
    if (!fromaddr) {
        CloseSocketHandle(handle);
        return SDL_OutOfMemory();
    }
    SDL_AtomicSet(&fromaddr->status, 1);

    struct addrinfo hints;
    SDL_zero(hints);
    hints.ai_family = ((struct sockaddr *) &from)->sa_family;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_NUMERICHOST;

    const int gairc = getaddrinfo(hostbuf, NULL, &hints, &fromaddr->ainfo);
    if (gairc != 0) {
        SDL_free(fromaddr);
        CloseSocketHandle(handle);
        return SDL_SetError("Failed to determine incoming connection's address: %s", (rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
    }

    fromaddr->human_readable = SDL_strdup(hostbuf);
    if (!fromaddr->human_readable) {
        freeaddrinfo(fromaddr->ainfo);
        SDL_free(fromaddr);
        CloseSocketHandle(handle);
        return SDL_OutOfMemory();
    }

    SDLNet_StreamSocket *sock = (SDLNet_StreamSocket *) SDL_calloc(1, sizeof (SDLNet_StreamSocket));
    if (!sock) {
        SDL_free(fromaddr->human_readable);
        freeaddrinfo(fromaddr->ainfo);
        SDL_free(fromaddr);
        CloseSocketHandle(handle);
        return SDL_OutOfMemory();
    }

    sock->socktype = SOCKETTYPE_STREAM;
    sock->addr = fromaddr;
    sock->port = (Uint16) SDL_atoi(portbuf);
    sock->handle = handle;
    sock->status = 1;  // connected

    *client_stream = sock;
    return 0;
}

void SDLNet_DestroyServer(SDLNet_Server *server)
{
    if (server) {
        if (server->handle != INVALID_SOCKET) {
            CloseSocketHandle(server->handle);
        }
        SDLNet_UnrefAddress(server->addr);
        SDL_free(server);
    }
}


SDLNet_Address *SDLNet_GetStreamSocketAddress(SDLNet_StreamSocket *sock)
{
    if (!sock) {
        SDL_InvalidParamError("sock");
        return NULL;
    }
    return SDLNet_RefAddress(sock->addr);
}

static void UpdateStreamSocketSimulatedFailure(SDLNet_StreamSocket *sock)
{
    if (sock->percent_loss && (RandomNumberBetween(0, 100) > sock->percent_loss)) {
        // won the percent_loss lottery? Refuse to move more data for between 250 and 7000 milliseconds.
        sock->simulated_failure_until = SDL_GetTicks() + (Uint64) (RandomNumberBetween(250, 2000 + (50 * sock->percent_loss)));
    } else {
        sock->simulated_failure_until = 0;
    }
}

// see if any pending data can finally be sent, etc
static int PumpStreamSocket(SDLNet_StreamSocket *sock)
{
    if (!sock) {
        return SDL_InvalidParamError("sock");
    } else if (sock->pending_output_len > 0) {
        // !!! FIXME: there should be some small chance of streams dropping connection to simulate failure.
        if (sock->simulated_failure_until && (SDL_GetTicks() < sock->simulated_failure_until)) {
            return 0;  // streams are reliable, so instead of packet loss, we introduce lag.
        }

        const int bw = (int) write(sock->handle, sock->pending_output_buffer, sock->pending_output_len);
        if (bw < 0) {
            const int err = LastSocketError();
            return WouldBlock(err) ? 0 : SDL_SetError("Failed to write to socket: %s", strerror(err));
        } else if (bw < sock->pending_output_len) {
            SDL_memmove(sock->pending_output_buffer, sock->pending_output_buffer + bw, sock->pending_output_len - bw);
        }
        sock->pending_output_len -= bw;

        UpdateStreamSocketSimulatedFailure(sock);
    }

    return 0;
}

int SDLNet_WriteToStreamSocket(SDLNet_StreamSocket *sock, const void *buf, int buflen)
{
    if (PumpStreamSocket(sock) < 0) {  // try to flush any queued data to the socket now, before we handle more.
        return -1;
    } else if (buf == NULL) {
        return SDL_InvalidParamError("buf");
    } else if (buflen < 0) {
        return SDL_InvalidParamError("buflen");
    } else if (buflen == 0) {
        return 0;  // nothing to do.
    }

    if (sock->pending_output_len == 0) {  // nothing queued? See if we can just send this without queueing.
        // don't ever try to send directly if simulating packet loss; we'll always queue and mess with it then.
        if (sock->percent_loss == 0) {
            const int bw = (int) write(sock->handle, buf, buflen);
            if (bw < 0) {
                const int err = LastSocketError();
                if (!WouldBlock(err)) {
                    return SDL_SetError("Failed to write to socket: %s", strerror(err));
                }
            } else if (bw == buflen) {  // sent the whole thing? We're good to go here.
                return 0;
            } else /*if (bw < buflen)*/ {  // partial write? We'll queue the rest.
                buf = ((const Uint8 *) buf) + bw;
                buflen -= (int) bw;
            }
        }
    }

    // queue this up for sending later.
    const size_t min_alloc = sock->pending_output_len + buflen;
    if (min_alloc > sock->pending_output_allocation) {
        size_t newlen = SDL_max(1, sock->pending_output_allocation);
        while (newlen < min_alloc) {
            newlen *= 2;
        }
        void *ptr = SDL_realloc(sock->pending_output_buffer, newlen);
        if (!ptr) {
            return SDL_OutOfMemory();
        }
        sock->pending_output_buffer = (Uint8 *) ptr;
        sock->pending_output_allocation = newlen;
    }

    SDL_memcpy(sock->pending_output_buffer + sock->pending_output_len, buf, buflen);
    sock->pending_output_len += buflen;

    return 0;
}

int SDLNet_GetStreamSocketPendingWrites(SDLNet_StreamSocket *sock)
{
    if (PumpStreamSocket(sock) < 0) {
        return -1;
    }
    return sock->pending_output_len;
}

int SDLNet_WaitUntilStreamSocketDrained(SDLNet_StreamSocket *sock, int timeoutms)
{
    if (!sock) {
        return SDL_InvalidParamError("sock");
    }

    if (timeoutms != 0) {
        const Uint64 endtime = (timeoutms > 0) ? (SDL_GetTicks() + timeoutms) : 0;
        while (SDLNet_GetStreamSocketPendingWrites(sock) > 0) {
            struct pollfd pfd;
            SDL_zero(pfd);
            pfd.fd = sock->handle;
            pfd.events = POLLOUT;
            const int rc = poll(&pfd, 1, timeoutms);
            if (rc == SOCKET_ERROR) {
                return SDL_SetError("Socket poll failed: %s", strerror(errno));
            } else if (rc == 0) {
                break;  // timed out
            }

            if (timeoutms > 0) {   // We must have woken up for a pending write, etc. Figure out remaining wait time.
                const Uint64 now = SDL_GetTicks();
                if (now < endtime) {
                    timeoutms = (int) (endtime - now);
                } else {
                    break;  // time has expired, break out.
                }
            } // else timeout is meant to be infinite, but we woke up for a write, etc, so go back to an infinite poll until we fail or buffer is drained.
        }
    }

    return SDLNet_GetStreamSocketPendingWrites(sock);
}

int SDLNet_ReadStreamSocket(SDLNet_StreamSocket *sock, void *buf, int buflen)
{
    if (PumpStreamSocket(sock) < 0) {  // try to flush any queued data to the socket now, before we go further.
        return -1;
    } else if (sock->simulated_failure_until && (SDL_GetTicks() < sock->simulated_failure_until)) {
        return 0;  // streams are reliable, so instead of packet loss, we introduce lag.
    }

    if (buf == NULL) {
        return SDL_InvalidParamError("buf");
    } else if (buflen < 0) {
        return SDL_InvalidParamError("buflen");
    } else if (buflen == 0) {
        return 0;  // nothing to do.
    }

    const int br = (int) read(sock->handle, buf, buflen);
    if (br == 0) {
        return SDL_SetError("End of stream");
    } else if (br < 0) {
        const int err = LastSocketError();
        return WouldBlock(err) ? 0 : SDL_SetError("Failed to read from socket: %s", strerror(err));
    }

    UpdateStreamSocketSimulatedFailure(sock);

    return br;
}

void SDLNet_SimulateStreamPacketLoss(SDLNet_StreamSocket *sock, int percent_loss)
{
    if (!sock) {
        return;
    }

    PumpStreamSocket(sock);

    percent_loss = SDL_min(100, percent_loss);
    percent_loss = SDL_max(0, percent_loss);
    sock->percent_loss = percent_loss;

    UpdateStreamSocketSimulatedFailure(sock);
}

// !!! FIXME: docs should note that this will THROW AWAY pending writes in _our_ buffers (not the kernel-level buffers) if you didn't wait for them to finish.
void SDLNet_DestroyStreamSocket(SDLNet_StreamSocket *sock)
{
    if (sock) {
        PumpStreamSocket(sock);  // try one last time to send any last pending data.

        SDLNet_UnrefAddress(sock->addr);
        if (sock->handle != INVALID_SOCKET) {
            CloseSocketHandle(sock->handle);  // !!! FIXME: what does this do with non-blocking sockets? Release the descriptor but the kernel continues sending queued buffers behind the scenes?
        }
        SDL_free(sock->pending_output_buffer);
        SDL_free(sock);
    }
}



struct SDLNet_DatagramSocket
{
    SDLNet_SocketType socktype;
    SDLNet_Address *addr;  // bound to this address (NULL for any).
    Uint16 port;
    Socket handle;
    int percent_loss;
    Uint8 recv_buffer[64*1024];
    SDLNet_Datagram **pending_output;
    int pending_output_len;
    int pending_output_allocation;
    SDLNet_Address *latest_recv_addrs[64];
    int latest_recv_addrs_idx;
};


SDLNet_DatagramSocket *SDLNet_CreateDatagramSocket(SDLNet_Address *addr, Uint16 port)
{
    if (addr && SDL_AtomicGet(&addr->status) != 1) {
        SDL_SetError("Address is not resolved");  // strictly speaking, this should be a local interface, but a resolved address can fail later.
        return NULL;
    }

    SDLNet_DatagramSocket *sock = (SDLNet_DatagramSocket *) SDL_calloc(1, sizeof (SDLNet_DatagramSocket));
    if (!sock) {
        SDL_OutOfMemory();
        return NULL;
    }

    sock->socktype = SOCKETTYPE_DATAGRAM;
    sock->addr = addr;
    sock->port = port;

    struct addrinfo *addrwithport = MakeAddrInfoWithPort(addr, SOCK_DGRAM, port);
    if (!addrwithport) {
        SDL_free(sock);
        return NULL;
    }

    sock->handle = socket(addrwithport->ai_family, addrwithport->ai_socktype, addrwithport->ai_protocol);
    if (sock->handle == INVALID_SOCKET) {
        SDL_SetError("Failed to create socket: %s", strerror(errno));
        freeaddrinfo(addrwithport);
        SDL_free(sock);
        return NULL;
    }

    if (MakeSocketNonblocking(sock->handle) < 0) {
        CloseSocketHandle(sock->handle);
        freeaddrinfo(addrwithport);
        SDL_free(sock);
        SDL_SetError("Failed to make new socket non-blocking");
        return NULL;
    }

    const int rc = bind(sock->handle, addrwithport->ai_addr, addrwithport->ai_addrlen);
    freeaddrinfo(addrwithport);

    if (rc == SOCKET_ERROR) {
        const int err = LastSocketError();
        SDL_assert(!WouldBlock(err));  // binding shouldn't be a blocking operation.
        SDL_SetError("Failed to bind socket: %s", strerror(err));
        CloseSocketHandle(sock->handle);
        SDL_free(sock);
        return NULL;
    }

    SDLNet_RefAddress(addr);
    return sock;
}

static int SendOneDatagram(SDLNet_DatagramSocket *sock, SDLNet_Address *addr, Uint16 port, const void *buf, int buflen)
{
    struct addrinfo *addrwithport = MakeAddrInfoWithPort(addr, SOCK_DGRAM, port);
    if (!addrwithport) {
        return -1;
    }
    const int rc = sendto(sock->handle, buf, buflen, 0, addrwithport->ai_addr, addrwithport->ai_addrlen);
    freeaddrinfo(addrwithport);

    if (rc == SOCKET_ERROR) {
        const int err = LastSocketError();
        return WouldBlock(err) ? 0 : SDL_SetError("Failed to send from socket: %s", strerror(err));
    }

    SDL_assert(rc == buflen);
    return 1;
}

// see if any pending data can finally be sent, etc
static int PumpDatagramSocket(SDLNet_DatagramSocket *sock)
{
    if (!sock) {
        return SDL_InvalidParamError("sock");
    }

    while (sock->pending_output_len > 0) {
        SDLNet_Datagram *dgram = sock->pending_output[0];
        const int rc = SendOneDatagram(sock, dgram->addr, dgram->port, dgram->buf, dgram->buflen);
        if (rc < 0) {  // failure!
            return -1;
        } else if (rc == 0) {  // wouldblock
            break;  // stop trying to send packets for now.
        }

        /* else if (rc > 0) */
        SDLNet_FreeDatagram(dgram);
        sock->pending_output_len--;
        SDL_memmove(sock->pending_output, sock->pending_output + 1, sock->pending_output_len * sizeof (SDLNet_Datagram *));
        sock->pending_output[sock->pending_output_len] = NULL;
    }

    return 0;
}


int SDLNet_SendDatagram(SDLNet_DatagramSocket *sock, SDLNet_Address *addr, Uint16 port, const void *buf, int buflen)
{
    if (PumpDatagramSocket(sock) < 0) {  // try to flush any queued data to the socket now, before we handle more.
        return -1;
    } else if (addr == NULL) {
        return SDL_InvalidParamError("address");
    } else if (buf == NULL) {
        return SDL_InvalidParamError("buf");
    } else if (buflen < 0) {
        return SDL_InvalidParamError("buflen");
    } else if (buflen > (64*1024)) {
        return SDL_SetError("buffer is too large to send in a single datagram packet");
    } else if (buflen == 0) {
        return 0;  // nothing to do.  (!!! FIXME: but strictly speaking, a UDP packet with no payload is legal.)
    } else if (sock->percent_loss && (RandomNumberBetween(0, 100) > sock->percent_loss)) {
        return 0;  // you won the percent_loss lottery. Drop this packet as if you sent it and it never arrived.
    }

    if (sock->pending_output_len == 0) {  // nothing queued? See if we can just send this without queueing.
        const int rc = SendOneDatagram(sock, addr, port, buf, buflen);
        if (rc < 0) {
            return -1;  // error string was already set in SendOneDatagram.
        } else if (rc == 1) {
            return 0;   // successfully sent.
        }
        // if rc==0, it wasn't sent, because we would have blocked. Queue it for later, below.
    }

    // queue this up for sending later.
    const size_t min_alloc = sock->pending_output_len + 1;
    if (min_alloc > sock->pending_output_allocation) {
        size_t newlen = SDL_max(1, sock->pending_output_allocation);
        while (newlen < min_alloc) {
            newlen *= 2;
        }
        void *ptr = SDL_realloc(sock->pending_output, newlen * sizeof (SDLNet_Datagram *));
        if (!ptr) {
            return SDL_OutOfMemory();
        }
        sock->pending_output = (SDLNet_Datagram **) ptr;
        sock->pending_output_allocation = newlen;
    }

    SDLNet_Datagram *dgram = (SDLNet_Datagram *) SDL_malloc(sizeof (SDLNet_Datagram) + buflen);
    if (!dgram) {
        return SDL_OutOfMemory();
    }

    dgram->buf = (Uint8 *) (dgram+1);
    SDL_memcpy(dgram->buf, buf, buflen);

    dgram->addr = SDLNet_RefAddress(addr);
    dgram->port = port;
    dgram->buflen = buflen;

    sock->pending_output[sock->pending_output_len++] = dgram;

    return 0;
}


int SDLNet_ReceiveDatagram(SDLNet_DatagramSocket *sock, SDLNet_Datagram **dgram)
{
    if (!dgram) {
        return SDL_InvalidParamError("dgram");
    }

    *dgram = NULL;

    if (PumpDatagramSocket(sock) < 0) {  // try to flush any queued data to the socket now, before we go further.
        return -1;
    }

    AddressStorage from;
    SockLen fromlen = sizeof (from);
    const int br = recvfrom(sock->handle, sock->recv_buffer, sizeof (sock->recv_buffer), 0, (struct sockaddr *) &from, &fromlen);
    if (br == SOCKET_ERROR) {
        const int err = LastSocketError();
        return WouldBlock(err) ? 0 : SDL_SetError("Failed to receive datagrams: %s", strerror(err));
    } else if (sock->percent_loss && (RandomNumberBetween(0, 100) > sock->percent_loss)) {
        // you won the percent_loss lottery. Drop this packet as if it never arrived.
        return 0;
    }

    // !!! FIXME: this all seems inefficient in the name of keeping addresses generic.
    char hostbuf[128];
    char portbuf[16];
    const int rc = getnameinfo((struct sockaddr *) &from, fromlen, hostbuf, sizeof (hostbuf), portbuf, sizeof (portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0) {
        return SDL_SetError("Failed to determine incoming packet's address: %s", (rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
    }

    // Cache the last X addresses we saw; if we see it again, refcount it and reuse it.
    SDLNet_Address *fromaddr = NULL;
    for (int i = sock->latest_recv_addrs_idx - 1; i >= 0; i--) {
        SDLNet_Address *a = sock->latest_recv_addrs[i];
        SDL_assert(a != NULL);  // can't be NULL, we either set this before or wrapped around to set again, but it can't be NULL.
        if (SDL_strcmp(a->human_readable, hostbuf) == 0) {
            fromaddr = a;
            break;
        }
    }

    if (!fromaddr) {
        const int idx = sock->latest_recv_addrs_idx;
        for (int i = SDL_arraysize(sock->latest_recv_addrs) - 1; i >= idx; i--) {
            SDLNet_Address *a = sock->latest_recv_addrs[i];
            if (a == NULL) {
                break;  // ran out of already-seen entries.
            }
            if (SDL_strcmp(a->human_readable, hostbuf) == 0) {
                fromaddr = a;
                break;
            }
        }
    }

    const SDL_bool create_fromaddr = (!fromaddr) ? SDL_TRUE : SDL_FALSE;
    if (create_fromaddr) {
        fromaddr = (SDLNet_Address *) SDL_calloc(1, sizeof (SDLNet_Address));
        if (!fromaddr) {
            return SDL_OutOfMemory();
        }
        SDL_AtomicSet(&fromaddr->status, 1);

        struct addrinfo hints;
        SDL_zero(hints);
        hints.ai_family = ((struct sockaddr *) &from)->sa_family;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = 0;
        hints.ai_flags = AI_NUMERICHOST;

        const int gairc = getaddrinfo(hostbuf, NULL, &hints, &fromaddr->ainfo);
        if (gairc != 0) {
            SDL_free(fromaddr);
            return SDL_SetError("Failed to determine incoming packet's address: %s", (rc == EAI_SYSTEM) ? strerror(errno) : gai_strerror(rc));
        }

        fromaddr->human_readable = SDL_strdup(hostbuf);
        if (!fromaddr->human_readable) {
            freeaddrinfo(fromaddr->ainfo);
            SDL_free(fromaddr);
            return SDL_OutOfMemory();
        }
    }

    SDLNet_Datagram *dg = SDL_malloc(sizeof (SDLNet_Datagram) + br);
    if (!dg) {
        if (create_fromaddr) {
            freeaddrinfo(fromaddr->ainfo);
            SDL_free(fromaddr);
        }
        return SDL_OutOfMemory();
    }

    dg->buf = (Uint8 *) (dg+1);
    SDL_memcpy(dg->buf, sock->recv_buffer, br);
    dg->addr = SDLNet_RefAddress(fromaddr);
    dg->port = (Uint16) SDL_atoi(portbuf);
    dg->buflen = br;

    *dgram = dg;

    if (create_fromaddr) {
        // keep track of the last X addresses we saw.
        SDLNet_UnrefAddress(sock->latest_recv_addrs[sock->latest_recv_addrs_idx]);  // okay if "oldest" address slot is still NULL.
        sock->latest_recv_addrs[sock->latest_recv_addrs_idx++] = SDLNet_RefAddress(fromaddr);
        sock->latest_recv_addrs_idx %= SDL_arraysize(sock->latest_recv_addrs);
    }

    return 0;
}

void SDLNet_FreeDatagram(SDLNet_Datagram *dgram)
{
    if (dgram) {
        SDLNet_UnrefAddress(dgram->addr);
        SDL_free(dgram);  // the buffer is allocated in the same block as the main struct.
    }
}

void SDLNet_SimulateDatagramPacketLoss(SDLNet_DatagramSocket *sock, int percent_loss)
{
    if (!sock) {
        return;
    }

    PumpDatagramSocket(sock);

    percent_loss = SDL_min(100, percent_loss);
    percent_loss = SDL_max(0, percent_loss);
    sock->percent_loss = percent_loss;
}

void SDLNet_DestroyDatagramSocket(SDLNet_DatagramSocket *sock)
{
    if (sock) {
        PumpDatagramSocket(sock);  // try one last time to send any last pending data.

        if (sock->handle != INVALID_SOCKET) {
            CloseSocketHandle(sock->handle);  // !!! FIXME: what does this do with non-blocking sockets? Release the descriptor but the kernel continues sending queued buffers behind the scenes?
        }
        for (int i = 0; i < SDL_arraysize(sock->latest_recv_addrs); i++) {
            SDLNet_UnrefAddress(sock->latest_recv_addrs[i]);
        }
        for (int i = 0; i < sock->pending_output_len; i++) {
            SDLNet_FreeDatagram(sock->pending_output[i]);
        }
        SDLNet_UnrefAddress(sock->addr);
        SDL_free(sock->pending_output);
        SDL_free(sock);
    }
}

typedef union SDLNet_GenericSocket
{
    SDLNet_SocketType socktype;
    SDLNet_StreamSocket stream;
    SDLNet_DatagramSocket dgram;
    SDLNet_Server server;
} SDLNet_GenericSocket;


int SDLNet_WaitUntilInputAvailable(void **vsockets, int numsockets, int timeoutms)
{
    SDLNet_GenericSocket **sockets = (SDLNet_GenericSocket **) vsockets;
    if (!sockets) {
        return SDL_InvalidParamError("sockets");
    } else if (numsockets == 0) {
        return 0;
    }

    struct pollfd stack_pfds[32];
    struct pollfd *pfds = stack_pfds;
    struct pollfd *malloced_pfds = NULL;

    if (numsockets > SDL_arraysize(stack_pfds)) {  // allocate if there's a _ton_ of these.
        malloced_pfds = (struct pollfd *) SDL_malloc(numsockets * sizeof (*pfds));
        if (!malloced_pfds) {
            return SDL_OutOfMemory();
        }
        pfds = malloced_pfds;
    }

    int retval = 0;
    const Uint64 endtime = (timeoutms > 0) ? (SDL_GetTicks() + timeoutms) : 0;

    while (SDL_TRUE) {
        SDL_memset(pfds, '\0', sizeof (*pfds) * numsockets);

        for (int i = 0; i < numsockets; i++) {
            SDLNet_GenericSocket *sock = sockets[i];
            struct pollfd *pfd = &pfds[i];

            switch (sock->socktype) {
                case SOCKETTYPE_STREAM:
                    pfd->fd = sock->stream.handle;
                    if (sock->stream.status == 0) {
                        pfd->events = POLLOUT;  // marked as writable when connection is complete.
                    } else if (sock->stream.pending_output_len > 0) {
                        pfd->events = POLLIN|POLLOUT;  // poll for input or when we can write more of the pending buffer.
                    } else {
                        pfd->events = POLLIN;  // poll for input or when we can write more of the pending buffer.
                    }
                    break;

                case SOCKETTYPE_DATAGRAM:
                    pfd->fd = sock->dgram.handle;
                    if (sock->dgram.pending_output_len > 0) {
                        pfd->events = POLLIN|POLLOUT;  // poll for input or when we can write more of the pending buffer.
                    } else {
                        pfd->events = POLLIN;  // poll for input or when we can write more of the pending buffer.
                    }
                    break;

                case SOCKETTYPE_SERVER:
                    pfd->fd = sock->server.handle;
                    pfd->events = POLLIN;  // poll for new connections.
                    break;
            }
        }

        const int rc = poll(pfds, numsockets, timeoutms);

        if (rc == SOCKET_ERROR) {
            SDL_free(malloced_pfds);
            return SDL_SetError("Socket poll failed: %s", strerror(errno));
        }

        for (int i = 0; i < numsockets; i++) {
            SDLNet_GenericSocket *sock = sockets[i];
            const struct pollfd *pfd = &pfds[i];
            const SDL_bool failed = ((pfd->revents & (POLLERR|POLLHUP|POLLNVAL)) != 0) ? SDL_TRUE : SDL_FALSE;
            const SDL_bool writable = (pfd->revents & POLLOUT) ? SDL_TRUE : SDL_FALSE;
            const SDL_bool readable = (pfd->revents & POLLIN) ? SDL_TRUE : SDL_FALSE;

            if (readable || failed) {
                retval++;
            }

            switch (sock->socktype) {
                case SOCKETTYPE_STREAM:
                    if (sock->stream.status == 0) {
                        if (failed) {
                            int err = 0;
                            SockLen errsize = sizeof (err);
                            getsockopt(pfd->fd, SOL_SOCKET, SO_ERROR, (char*)&err, &errsize);
                            sock->stream.status = SDL_SetError("Socket failed to connect: %s", strerror(err));
                        } else if (writable) {
                            sock->stream.status = 1;
                        }
                    } else if (writable) {
                        PumpStreamSocket(&sock->stream);
                    }
                    break;

                case SOCKETTYPE_DATAGRAM:
                    if (writable) {
                        PumpDatagramSocket(&sock->dgram);
                    }
                    break;

                case SOCKETTYPE_SERVER:
                    // we already checked `readable`.
                    break;
            }
        }

        if ((retval > 0) || (endtime == 0)) {
            break;  // something has input available, or we are doing a no-block poll.
        } else if (timeoutms > 0) {   // We must have woken up for a pending write, etc. Figure out remaining wait time.
            const Uint64 now = SDL_GetTicks();
            if (now < endtime) {
                timeoutms = (int) (endtime - now);
            } else {
                break;  // time has expired, break out.
            }
        } // else timeout is meant to be infinite, but we woke up for a write, etc, so go back to an infinite poll.
    }

    SDL_free(malloced_pfds);

    return retval;
}


#if 0  // some test code.
#include <stdio.h>

int main(int argc, char **argv)
{
    if (SDLNet_Init() < 0) {
        SDL_Log("SDLNet_Init() failed: %s", SDL_GetError());
        return 1;
    }

#if 1
    SDLNet_StreamSocket *stream;
    if (argc > 1) {
        SDLNet_Server *server = SDLNet_CreateServer(NULL, 7997);
        SDLNet_WaitUntilConnected(server);
        SDLNet_AcceptClient(server, &stream);
    } else {
        SDLNet_Address *addr = SDLNet_ResolveHostname("localhost");
        SDLNet_WaitUntilResolved(addr, -1);
        stream = SDLNet_CreateClient(addr, 7997);
        SDLNet_WaitUntilConnected(stream, -1);
    }

    printf("\n\nConnected!\n\n");

    fcntl(fileno(stdin), F_SETFL, fcntl(fileno(stdin), F_GETFL, 0) | O_NONBLOCK);

    while (1) {
        char buf[128];
        buf[0] = '\0';
        fgets(buf, sizeof (buf), stdin);
        if (buf[0]) {
            SDLNet_WriteToStreamSocket(stream, buf, SDL_strlen(buf));
        }
        const int br = SDLNet_ReadStreamSocket(stream, buf, sizeof (buf));
        if (br > 0) {
            fwrite(buf, 1, br, stdout);
        }
    }
#endif

#if 0
    //SDLNet_SimulateAddressResolutionLoss(3000, 30);

    SDLNet_Address **addrs = (SDLNet_Address **) SDL_calloc(argc, sizeof (SDLNet_Address *));
    for (int i = 1; i < argc; i++) {
        addrs[i] = SDLNet_ResolveHostname(argv[i]);
    }

    for (int i = 1; i < argc; i++) {
        SDLNet_WaitUntilResolved(addrs[i], -1);
    }
#endif

    SDLNet_Quit();
    return 0;
}
#endif
