/*
  SDL_net: A simple networking library for use with SDL
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/**
 * \file SDL_net.h
 *
 * Header file for SDL_net library
 *
 * A simple library to help with networking.
 */

#ifndef SDL_NET_H_
#define SDL_NET_H_

#include <SDL3/SDL.h>
#include <SDL3/SDL_begin_code.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/* Version checks... */

#define SDL_NET_MAJOR_VERSION   3
#define SDL_NET_MINOR_VERSION   0
#define SDL_NET_PATCHLEVEL      0

/**
 * Query the verion of the SDL_net library in use at compile time.
 *
 * This macro copies the version listen in the SDL_net headers into a
 * struct of the app's choosing.
 *
 * \threadsafety It is safe to use this macro from any thread.
 *
 * \since This macro is available since SDL_Net 3.0.0.
 */
#define SDL_NET_VERSION(X)                          \
{                                                   \
    (X)->major = SDL_NET_MAJOR_VERSION;             \
    (X)->minor = SDL_NET_MINOR_VERSION;             \
    (X)->patch = SDL_NET_PATCHLEVEL;                \
}

/**
 * Query the verion of the SDL_net library in use at runtime.
 *
 * The returned value points to static, internal, read-only memory. Do not
 * modify or free it. The pointer remains valid as long as the library is
 * loaded by the system.
 *
 * This function can be safely called before SDLNet_Init().
 *
 * \returns An object with the runtime library version. Never returns NULL.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC const SDL_Version * SDLCALL SDLNet_LinkedVersion(void);


/* init/quit functions... */

/**
 * Initialize the SDL_net library.
 *
 * This must be successfully called once before (almost) any other SDL_net
 * function can be used.
 *
 * It is safe to call this multiple times; the library will only initialize
 * once, and won't deinitialize until SDLNet_Quit() has been called a matching
 * number of times. Extra attempts to init report success.
 *
 * \returns 0 on success, -1 on error; call SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_net 3.0.0.
 *
 * \sa SDLNet_Quit
 */
extern DECLSPEC int SDLCALL SDLNet_Init(void);

/**
 * Deinitialize the SDL_net library.
 *
 * This must be called when done with the library, probably at the end of your
 * program.
 *
 * It is safe to call this multiple times; the library will only deinitialize
 * once, when this function is called the same number of times as SDLNet_Init
 * was successfully called.
 *
 * Once you have successfully deinitialized the library, it is safe to call
 * SDLNet_Init to reinitialize it for further use.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_Quit
 */
extern DECLSPEC void SDLCALL SDLNet_Quit(void);


/* hostname resolution API... */

typedef struct SDLNet_Address SDLNet_Address;  /**< Opaque struct that deals with computer-readable addresses. */

/**
 * Resolve a human-readable hostname.
 *
 * SDL_net doesn't operate on human-readable hostnames (like "www.libsdl.org")
 * but on computer-readable addresses. This function converts from one to the
 * other. This process is known as "resolving" an address.
 *
 * You can also use this to turn IP address strings (like "159.203.69.7") into
 * SDLNet_Address objects.
 *
 * Note that resolving an address is an asynchronous operation, since the
 * library will need to ask a server on the internet to get the information it
 * needs, and this can take time (and possibly fail later). This function will
 * not block. It either returns NULL (catastrophic failure) or an unresolved
 * SDLNet_Address. Until the address resolves, it can't be used.
 *
 * If you want to block until the resolution is finished, you can call
 * SDLNet_WaitUntilResolved(). Otherwise, you can do a non-blocking check with
 * SDLNet_GetAddressStatus().
 *
 * When you are done with the returned SDLNet_Address, call
 * SDLNet_UnrefAddress() to dispose of it. You need to do this even if
 * resolution later fails asynchronously.
 *
 * \param host The hostname to resolve.
 * \returns A new SDLNet_Address on success, NULL on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WaitUntilResolved
 * \sa SDLNet_GetAddressStatus
 * \sa SDLNet_RefAddress
 * \sa SDLNet_UnrefAddress
 */
extern DECLSPEC SDLNet_Address * SDLCALL SDLNet_ResolveHostname(const char *host);

/**
 * Block until an address is resolved.
 *
 * The SDLNet_Address objects returned by SDLNet_ResolveHostname take time to
 * do their work, so it is does so _asynchronously_ instead of making your
 * program wait an indefinite amount of time.
 *
 * However, if you want your program to sleep until the address resolution is
 * complete, you can call this function.
 *
 * This function takes a timeout value, represented in milliseconds, of how
 * long to wait for resolution to complete. Specifying a timeout of -1
 * instructs the library to wait indefinitely, and a timeout of 0 just checks
 * the current status and returns immediately (and is functionally equivalent
 * to calling SDLNet_GetAddressStatus).
 *
 * Resolution can fail after some time (DNS server took awhile to reply that
 * the hostname isn't recognized, etc), so be sure to check the result of this
 * function instead of assuming it worked!
 *
 * Once an address is successfully resolved, it can be used to connect to the
 * host represented by the address.
 *
 * If you don't want your program to block, you can call
 * SDLNet_GetAddressStatus from time to time until you get a non-zero result.
 *
 * \param address The SDLNet_Address object to wait on.
 * \param timeout Number of milliseconds to wait for resolution to complete.
 *                -1 to wait indefinitely, 0 to check once without waiting.
 * \returns 1 if successfully resolved, -1 if resolution failed, 0 if still
 *          resolving (this function timed out without resolution); if -1,
 *          call SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread, and several
 *               threads can block on the same address simultaneously.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_GetAddressStatus
 */
extern DECLSPEC int SDLCALL SDLNet_WaitUntilResolved(SDLNet_Address *address, Sint32 timeout);

/**
 * Check if an address is resolved, without blocking.
 *
 * The SDLNet_Address objects returned by SDLNet_ResolveHostname take time to
 * do their work, so it is does so _asynchronously_ instead of making your
 * program wait an indefinite amount of time.
 *
 * This function allows you to check the progress of that work without
 * blocking.
 *
 * Resolution can fail after some time (DNS server took awhile to reply that
 * the hostname isn't recognized, etc), so be sure to check the result of this
 * function instead of assuming it worked because it's non-zero!
 *
 * Once an address is successfully resolved, it can be used to connect to the
 * host represented by the address.
 *
 * \param address The SDLNet_Address to query.
 * \returns 1 if successfully resolved, -1 if resolution failed, 0 if still
 *          resolving; if -1, call SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WaitUntilResolved
 */
extern DECLSPEC int SDLCALL SDLNet_GetAddressStatus(SDLNet_Address *address);

/**
 * Get a human-readable string from a resolved address.
 *
 * This returns a string that's "human-readable", in that it's probably a
 * string of numbers and symbols, like "159.203.69.7" or
 * "2604:a880:800:a1::71f:3001". It won't be the original hostname (like
 * "icculus.org"), but it's suitable for writing to a log file, etc.
 *
 * Do not free or modify the returned string; it belongs to the SDLNet_Address
 * that was queried, and is valid as long as the object lives. Either make
 * sure the address has a reference as long as you need this or make a copy of
 * the string.
 *
 * This will return NULL if resolution is still in progress, or if resolution
 * failed. You can use SDLNet_GetAddressStatus() or SDLNet_WaitUntilResolved()
 * to make sure resolution has successfully completed before calling this.
 *
 * \param address The SDLNet_Address to query.
 * \returns a string, or NULL on error; call SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_GetAddressStatus
 * \sa SDLNet_WaitUntilResolved
 */
extern DECLSPEC const char * SDLCALL SDLNet_GetAddressString(SDLNet_Address *address);

/**
 * Add a reference to an SDLNet_Address.
 *
 * Since several pieces of the library might share a single SDLNet_Address,
 * including a background thread that's working on resolving, these objects
 * are referenced counted. This allows everything that's using it to declare
 * they still want it, and drop their reference to the address when they are
 * done with it. The object's resources are freed when the last reference is
 * dropped.
 *
 * This function adds a reference to an SDLNet_Address, increasing its
 * reference count by one.
 *
 * The documentation will tell you when the app has to explicitly unref an
 * address. For example, SDLNet_ResolveHostname() creates addresses that are
 * already referenced, so the caller needs to unref it when done.
 *
 * Generally you only have to explicit ref an address when you have different
 * parts of your own app that will be sharing an address. In normal usage, you
 * only have to unref things you've created once (like you might free()
 * something), but you are free to add extra refs if it makes sense.
 *
 * This returns the same address passed as a parameter, which makes it easy to
 * ref and assign in one step:
 *
 * ```c
 * myAddr = SDLNet_RefAddress(yourAddr);
 * ```
 *
 * \param address The SDLNet_Address to add a reference to.
 * \returns the same address that was passed as a parameter.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC SDLNet_Address *SDLCALL SDLNet_RefAddress(SDLNet_Address *address);

/**
 * Drop a reference to an SDLNet_Address.
 *
 * Since several pieces of the library might share a single SDLNet_Address,
 * including a background thread that's working on resolving, these objects
 * are referenced counted. This allows everything that's using it to declare
 * they still want it, and drop their reference to the address when they are
 * done with it. The object's resources are freed when the last reference is
 * dropped.
 *
 * This function drops a reference to an SDLNet_Address, decreasing its
 * reference count by one.
 *
 * The documentation will tell you when the app has to explicitly unref an
 * address. For example, SDLNet_ResolveHostname() creates addresses that are
 * already referenced, so the caller needs to unref it when done.
 *
 * \param address The SDLNet_Address to drop a reference to.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC void SDLCALL SDLNet_UnrefAddress(SDLNet_Address *address);

/**
 * Enable simulated address resolution failures.
 *
 * Often times, testing a networked app on your development machine--which
 * might have a wired connection to a fast, reliable network service--won't
 * expose bugs that happen when networks intermittently fail in the real
 * world, when the wifi is flakey and firewalls get in the way.
 *
 * This function allows you to tell the library to pretend that some
 * percentage of address resolutions will fail.
 *
 * The higher the percentage, the more resolutions will fail and/or take
 * longer for resolution to complete.
 *
 * Setting this to zero (the default) will disable the simulation. Setting to
 * 100 means _everything_ fails unconditionally. At what percent the system
 * merely borders on unusable is left as an exercise to the app developer.
 *
 * This is intended for debugging purposes, to simulate real-world conditions
 * that are various degrees of terrible. You probably should _not_ call this
 * in production code, where you'll likely see real failures anyhow.
 *
 * \param percent_loss A number between 0 and 100. Higher means more failures.
 *                     Zero to disable.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC void SDLCALL SDLNet_SimulateAddressResolutionLoss(int percent_loss);

/**
 * Compare two SDLNet_Address objects.
 *
 * This compares two addresses, returning a value that is useful for qsort (or
 * SDL_qsort).
 *
 * \param a first address to compare.
 * \param b second address to compare.
 * \returns -1 if `a` is "less than" `b`, 1 if "greater than", 0 if equal.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC int SDLCALL SDLNet_CompareAddresses(const SDLNet_Address *a, const SDLNet_Address *b);

/**
 * Obtain a list of local addresses on the system.
 *
 * This returns addresses that you can theoretically bind a socket to, to
 * accept connections from other machines at that address.
 *
 * You almost never need this function; first, it's hard to tell _what_ is a
 * good address to bind to, without asking the user (who will likely find it
 * equally hard to decide). Second, most machines will have lots of _private_
 * addresses that are accessible on the same LAN, but not public ones that are
 * accessible from the outside Internet.
 *
 * Usually it's better to use SDLNet_CreateServer() or
 * SDLNet_CreateDatagramSocket() with a NULL address, to say "bind to all
 * interfaces."
 *
 * The array of addresses returned from this is guaranteed to be
 * NULL-terminated. You can also pass a pointer to an int, which will return
 * the final count, not counting the NULL at the end of the array.
 *
 * Pass the returned array to SDLNet_FreeLocalAddresses when you are done with
 * it. It is safe to keep any addresses you want from this array even after
 * calling that function, as long as you called SDLNet_RefAddress() on them.
 *
 * \param num_addresses on exit, will be set to the number of addresses
 *                      returned. Can be NULL.
 * \returns A NULL-terminated array of SDLNet_Address pointers, one for each
 *          bindable address on the system, or NULL on error; call
 *          SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC SDLNet_Address **SDLCALL SDLNet_GetLocalAddresses(int *num_addresses);

/**
 * Free the results from SDLNet_GetLocalAddresses.
 *
 * This will unref all addresses in the array and free the array itself.
 *
 * Since addresses are reference counted, it is safe to keep any addresses you
 * want from this array even after calling this function, as long as you
 * called SDLNet_RefAddress() on them first.
 *
 * It is safe to pass a NULL in here, it will be ignored.
 *
 * \param addresses A pointer returned by SDLNet_GetLocalAddresses().
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC void SDLCALL SDLNet_FreeLocalAddresses(SDLNet_Address **addresses);


/* Streaming (TCP) API... */

typedef struct SDLNet_StreamSocket SDLNet_StreamSocket;  /**< a TCP socket. Reliable transmission, with the usual pros/cons. */

/**
 * Begin connecting a socket as a client to a remote server.
 *
 * Each SDLNet_StreamSocket represents a single connection between systems.
 * Usually, a client app will have one connection to a server app on a
 * different computer, and the server app might have many connections from
 * different clients. Each of these connections communicate over a separate
 * stream socket.
 *
 * Connecting is an asynchronous operation; this function does not block, and
 * will return before the connection is complete. One has to then use
 * SDLNet_WaitUntilConnected() or SDLNet_GetConnectionStatus() to see when the
 * operation has completed, and if it was successful.
 *
 * Once connected, you can read and write data to the returned socket. Stream
 * sockets are a mode of _reliable_ transmission, which means data will be
 * received as a stream of bytes in the order you sent it. If there are
 * problems in transmission, the system will deal with protocol negotiation
 * and retransmission as necessary, transparent to your app, but this means
 * until data is available in the order sent, the remote side will not get any
 * new data. This is the tradeoff vs datagram sockets, where data can arrive
 * in any order, or not arrive at all, without waiting, but the sender will
 * not know.
 *
 * Stream sockets don't employ any protocol (above the TCP level), so they can
 * connect to servers that aren't using SDL_net, but if you want to speak any
 * protocol beyond an abritrary stream of bytes, such as HTTP, you'll have to
 * implement that yourself on top of the stream socket.
 *
 * This function will fail if `address` is not finished resolving.
 *
 * When you are done with this connection (whether it failed to connect or
 * not), you must dispose of it with SDLNet_DestroyStreamSocket().
 *
 * Unlike BSD sockets or WinSock, you specify the port as a normal integer;
 * you do not have to byteswap it into "network order," as the library will
 * handle that for you.
 *
 * \param address the address of the remote server to connect to
 * \param the port on the remote server to connect to
 * \returns a new SDLNet_StreamSocket, pending connection, or NULL on error;
 *          call SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WaitUntilConnected
 * \sa SDLNet_GetConnectionStatus
 * \sa SDLNet_DestroyStreamSocket
 */
extern DECLSPEC SDLNet_StreamSocket * SDLCALL SDLNet_CreateClient(SDLNet_Address *address, Uint16 port);

/**
 * Block until a stream socket has connected to a server.
 *
 * The SDLNet_StreamSocket objects returned by SDLNet_CreateClient take time
 * to do their work, so it is does so _asynchronously_ instead of making your
 * program wait an indefinite amount of time.
 *
 * However, if you want your program to sleep until the connection is
 * complete, you can call this function.
 *
 * This function takes a timeout value, represented in milliseconds, of how
 * long to wait for resolution to complete. Specifying a timeout of -1
 * instructs the library to wait indefinitely, and a timeout of 0 just checks
 * the current status and returns immediately (and is functionally equivalent
 * to calling SDLNet_GetConnectionStatus).
 *
 * Connections can fail after some time (server took awhile to respond at all,
 * and then refused the connection outright), so be sure to check the result
 * of this function instead of assuming it worked!
 *
 * Once a connection is successfully made, the socket may read data from, or
 * write data to, the connected server.
 *
 * If you don't want your program to block, you can call
 * SDLNet_GetConnectionStatus() from time to time until you get a non-zero
 * result.
 *
 * \param address The SDLNet_Address object to wait on.
 * \param timeout Number of milliseconds to wait for resolution to complete.
 *                -1 to wait indefinitely, 0 to check once without waiting.
 * \returns 1 if successfully connected, -1 if connection failed, 0 if still
 *          connecting (this function timed out without resolution); if -1,
 *          call SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               socket at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_GetConnectionStatus
 */
extern DECLSPEC int SDLCALL SDLNet_WaitUntilConnected(SDLNet_StreamSocket *sock, Sint32 timeout);

typedef struct SDLNet_Server SDLNet_Server;   /**< a listen socket, internally. Binds to a port, accepts connections. */

/**
 * Create a server, which listens for connections to accept.
 *
 * An app that initiates connection to a remote computer is called a "client,"
 * and the thing the client connects to is called a "server."
 *
 * Servers listen for and accept connections from clients, which spawns a new
 * stream socket on the server's end, which it can then send/receive data on.
 *
 * Use this function to create a server that will accept connections from
 * other systems.
 *
 * This function does not block, and is not asynchronous, as the system can
 * decide immediately if it can create a server or not. If this returns
 * success, you can immediately start accepting connections.
 *
 * You can specify an address to listen for connections on; this address must
 * be local to the system, and probably one returned by
 * SDLNet_GetLocalAddresses(), but almost always you just want to specify NULL
 * here, to listen on any address available to the app.
 *
 * After creating a server, you get stream sockets to talk to incoming client
 * connections by calling SDLNet_AcceptClient().
 *
 * Stream sockets don't employ any protocol (above the TCP level), so they can
 * accept connections from clients that aren't using SDL_net, but if you want
 * to speak any protocol beyond an abritrary stream of bytes, such as HTTP,
 * you'll have to implement that yourself on top of the stream socket.
 *
 * Unlike BSD sockets or WinSock, you specify the port as a normal integer;
 * you do not have to byteswap it into "network order," as the library will
 * handle that for you.
 *
 * \param address the _local_ address to listen for connections on, or NULL.
 * \param the port on the local address to listen for connections on
 * \returns a new SDLNet_StreamSocket, pending connection, or NULL on error;
 *          call SDL_GetError() for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_GetLocalAddresses
 * \sa SDLNet_AcceptClient
 * \sa SDLNet_DestroyServer
 */
extern DECLSPEC SDLNet_Server * SDLCALL SDLNet_CreateServer(SDLNet_Address *addr, Uint16 port);

/**
 * Create a stream socket for the next pending client connection.
 *
 * When a client connects to a server, their connection will be pending until
 * the server _accepts_ the connection. Once accepted, the server will be
 * given a stream socket to communicate with the client, and they can send
 * data to, and receive data from, each other.
 *
 * Unlike SDLNet_CreateClient, stream sockets returned from this function are
 * already connected and do not have to wait for the connection to complete,
 * as server acceptance is the final step of connecting.
 *
 * This function does not block. If there are no new connections pending, this
 * function will return 0 (for success, but `*client_stream` will be set to
 * NULL. This is not an error and a common condition the app should expect. In
 * fact, this function should be called in a loop until this condition occurs,
 * so all pending connections are accepted in a single batch.
 *
 * If you want the server to sleep until there's a new connection, you can use
 * SDLNet_WaitUntilInputAvailable().
 *
 * When done with the newly-accepted client, you can disconnect and dispose of
 * the stream socket by calling SDL_DestroyStreamSocket().
 *
 * \param server the server object to check for pending connections
 * \param client_stream Will be set to a new stream socket if a connection was
 *                      pending, NULL otherwise.
 * \returns 0 on success (even if no new connections were pending), -1 on
 *          error; call SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same server from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               servers at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WaitUntilInputAvailable
 * \sa SDLNet_DestroyStreamSocket
 */
extern DECLSPEC int SDLCALL SDLNet_AcceptClient(SDLNet_Server *server, SDLNet_StreamSocket **client_stream);

/**
 * Dispose of a previously-created server.
 *
 * This will immediately disconnect any pending client connections that had
 * not yet been accepted, but will not disconnect any existing accepted
 * connections (which can still be used and must be destroyed separately).
 * Further attempts to make new connections to this server will fail on the
 * client side.
 *
 * \param server server to destroy
 *
 * \threadsafety You should not operate on the same server from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               servers at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_CreateServer
 */
extern DECLSPEC void SDLCALL SDLNet_DestroyServer(SDLNet_Server *server);

/**
 * Get the remote address of a stream socket.
 *
 * This reports the address of the remote side of a stream socket, which might
 * still be pending connnection.
 *
 * This adds a reference to the address; the caller _must_ call
 * SDLNet_UnrefAddress() when done with it.
 *
 * \param sock the stream socket to query
 * \returns the socket's remote address, or NULL on error; call SDL_GetError()
 *          for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC SDLNet_Address * SDLCALL SDLNet_GetStreamSocketAddress(SDLNet_StreamSocket *sock);

/**
 * Check if a stream socket is connected, without blocking.
 *
 * The SDLNet_StreamSocket objects returned by SDLNet_CreateClient take time
 * to do negotiate a connection to a server, so it is does so _asynchronously_
 * instead of making your program wait an indefinite amount of time.
 *
 * This function allows you to check the progress of that work without
 * blocking.
 *
 * Connection can fail after some time (server took a while to respond, and
 * then rejected the connection), so be sure to check the result of this
 * function instead of assuming it worked because it's non-zero!
 *
 * Once a connection is successfully made, the stream socket can be used to
 * send and receive data with the server.
 *
 * Note that if the connection succeeds, but later the connection is dropped,
 * this will still report the connection as successful, as it only deals with
 * the initial asynchronous work of getting connected; you'll know the
 * connection dropped later when your reads and writes report failures.
 *
 * \param sock the stream socket to query.
 * \returns 1 if successfully connected, -1 if connection failed, 0 if still
 *          connecting; if -1, call SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WaitUntilConnected
 */
extern DECLSPEC int SDLCALL SDLNet_GetConnectionStatus(SDLNet_StreamSocket *sock);

/**
 * Send bytes over a stream socket to a remote system.
 *
 * Stream sockets are _reliable_, which means data sent over them will arrive
 * in the order it was transmitted, and the system will retransmit data as
 * necessary to ensure its delivery. Which is to say, short of catastrophic
 * failure, data will arrive, possibly with severe delays. Also, "catastrophic
 * failure" isn't an uncommon event.
 *
 * (This is opposed to Datagram sockets, which send chunks of data that might
 * arrive in any order, or not arrive at all, but you never wait for missing
 * chunks to show up.)
 *
 * Stream sockets are _bidirectional_; you can read and write from the same
 * stream, and the other end of the connection can, too.
 *
 * This call never blocks; if it can't send the data immediately, the library
 * will queue it for later transmission. You can use
 * SDLNet_GetStreamSocketPendingWrites() to see how much is still queued for
 * later transmission, or SDLNet_WaitUntilStreamSocketDrained() to block until
 * all pending data has been sent.
 *
 * If the connection has failed (remote side dropped us, or one of a million
 * other networking failures occurred), this function will report failure by
 * returning -1. Stream sockets only report failure for unrecoverable
 * conditions; once a stream socket fails, you should assume it is no longer
 * usable and should destroy it with SDL_DestroyStreamSocket().
 *
 * \param sock the stream socket to send data through
 * \param buf a pointer to the data to send.
 * \param buflen the size of the data to send, in bytes.
 * \returns 0 if data sent or queued for transmission, -1 on failure; call
 *          SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_GetStreamSocketPendingWrites
 * \sa SDLNet_WaitUntilStreamSocketDrained
 * \sa SDLNet_ReadFromStreamSocket
 */
extern DECLSPEC int SDLCALL SDLNet_WriteToStreamSocket(SDLNet_StreamSocket *sock, const void *buf, int buflen);

/**
 * Query bytes still pending transmission on a stream socket.
 *
 * If SDLNet_WriteToStreamSocket() couldn't send all its data immediately, it
 * will queue it to be sent later. This function lets the app see how much of
 * that queue is still pending to be sent.
 *
 * The library will try to send more queued data before reporting what's left,
 * but it will not block to do so.
 *
 * If the connection has failed (remote side dropped us, or one of a million
 * other networking failures occurred), this function will report failure by
 * returning -1. Stream sockets only report failure for unrecoverable
 * conditions; once a stream socket fails, you should assume it is no longer
 * usable and should destroy it with SDL_DestroyStreamSocket().
 *
 * \param sock the stream socket to query
 * \returns number of bytes still pending transmission, -1 on failure; call
 *          SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WriteToStreamSocket
 * \sa SDLNet_WaitUntilStreamSocketDrained
 */
extern DECLSPEC int SDLCALL SDLNet_GetStreamSocketPendingWrites(SDLNet_StreamSocket *sock);

/**
 * Block until all of a stream socket's pending data is sent.
 *
 * If SDLNet_WriteToStreamSocket() couldn't send all its data immediately, it
 * will queue it to be sent later. This function lets the app sleep until all
 * the data is transmitted.
 *
 * This function takes a timeout value, represented in milliseconds, of how
 * long to wait for transmission to complete. Specifying a timeout of -1
 * instructs the library to wait indefinitely, and a timeout of 0 just checks
 * the current status and returns immediately (and is functionally equivalent
 * to calling SDLNet_GetStreamSocketPendingWrites).
 *
 * If you don't want your program to block, you can call
 * SDLNet_GetStreamSocketPendingWrites from time to time until you get a
 * result <= 0.
 *
 * If the connection has failed (remote side dropped us, or one of a million
 * other networking failures occurred), this function will report failure by
 * returning -1. Stream sockets only report failure for unrecoverable
 * conditions; once a stream socket fails, you should assume it is no longer
 * usable and should destroy it with SDL_DestroyStreamSocket().
 *
 * \param sock the stream socket to wait on
 * \param timeout Number of milliseconds to wait for draining to complete. -1
 *                to wait indefinitely, 0 to check once without waiting.
 * \returns number of bytes still pending transmission, -1 on failure; call
 *          SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WriteToStreamSocket
 * \sa SDLNet_GetStreamSocketPendingWrites
 */
extern DECLSPEC int SDLCALL SDLNet_WaitUntilStreamSocketDrained(SDLNet_StreamSocket *sock, Sint32 timeout);


/**
 * Receive bytes that a remote system sent to a stream socket.
 *
 * Stream sockets are _reliable_, which means data sent over them will arrive
 * in the order it was transmitted, and the system will retransmit data as
 * necessary to ensure its delivery. Which is to say, short of catastrophic
 * failure, data will arrive, possibly with severe delays. Also, "catastrophic
 * failure" isn't an uncommon event.
 *
 * (This is opposed to Datagram sockets, which send chunks of data that might
 * arrive in any order, or not arrive at all, but you never wait for missing
 * chunks to show up.)
 *
 * Stream sockets are _bidirectional_; you can read and write from the same
 * stream, and the other end of the connection can, too.
 *
 * This function returns data that has arrived for the stream socket that
 * hasn't been read yet. Data is provided in the order it was sent on the
 * remote side. This function may return less data than requested, depending
 * on what is available at the time, and also the app isn't required to read
 * all available data at once.
 *
 * This call never blocks; if no new data isn't available at the time of the
 * call, it returns 0 immediately. The caller can try again later.
 *
 * If the connection has failed (remote side dropped us, or one of a million
 * other networking failures occurred), this function will report failure by
 * returning -1. Stream sockets only report failure for unrecoverable
 * conditions; once a stream socket fails, you should assume it is no longer
 * usable and should destroy it with SDL_DestroyStreamSocket().
 *
 * \param sock the stream socket to receive data from
 * \param buf a pointer to a buffer where received data will be collected.
 * \param buflen the size of the buffer pointed to by `buf`, in bytes. This is
 *               the maximum that will be read from the stream socket.
 * \returns number of bytes read from the stream socket (which can be less
 *          than `buflen` or zero if none available), -1 on failure; call
 *          SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_WriteToStreamSocket
 */
extern DECLSPEC int SDLCALL SDLNet_ReadFromStreamSocket(SDLNet_StreamSocket *sock, void *buf, int buflen);

/**
 * Enable simulated stream socket failures.
 *
 * Often times, testing a networked app on your development machine--which
 * might have a wired connection to a fast, reliable network service--won't
 * expose bugs that happen when networks intermittently fail in the real
 * world, when the wifi is flakey and firewalls get in the way.
 *
 * This function allows you to tell the library to pretend that some
 * percentage of stream socket data transmission will fail.
 *
 * Since stream sockets are reliable, failure in this case pretends that
 * packets are getting lost on the network, making the stream retransmit to
 * deal with it. To simulate this, the library will introduce some amount of
 * delay before it sends or receives data on the socket. The higher the
 * percentage, the more delay is introduced for bytes to make their way to
 * their final destination. The library may also decide to drop connections at
 * random, to simulate disasterous network conditions.
 *
 * Setting this to zero (the default) will disable the simulation. Setting to
 * 100 means _everything_ fails unconditionally and no further data will get
 * through (and perhaps your sockets eventually fail). At what percent the
 * system merely borders on unusable is left as an exercise to the app
 * developer.
 *
 * This is intended for debugging purposes, to simulate real-world conditions
 * that are various degrees of terrible. You probably should _not_ call this
 * in production code, where you'll likely see real failures anyhow.
 *
 * \param sock The socket to set a failure rate on.
 * \param percent_loss A number between 0 and 100. Higher means more failures.
 *                     Zero to disable.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC void SDLCALL SDLNet_SimulateStreamPacketLoss(SDLNet_StreamSocket *sock, int percent_loss);

/**
 * Dispose of a previously-created stream socket.
 *
 * This will immediately disconnect the other side of the connection, if
 * necessary. Further attempts to read or write the socket on the remote end
 * will fail.
 *
 * This will _abandon_ any data queued for sending that hasn't made it to the
 * socket. If you need this data to arrive, you should wait for it to transmit
 * before destroying the socket with SDLNet_GetStreamSocketPendingWrites() or
 * SDLNet_WaitUntilStreamSocketDrained(). Any data that has arrived from the
 * remote end of the connection that hasn't been read yet is lost.
 *
 * \param sock stream socket to destroy
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_CreateClient
 * \sa SDLNet_AcceptClient
 * \sa SDLNet_GetStreamSocketPendingWrites
 * \sa SDLNet_WaitUntilStreamSocketDrained
 */
extern DECLSPEC void SDLCALL SDLNet_DestroyStreamSocket(SDLNet_StreamSocket *sock);  /* Destroy your sockets when finished with them. Does not block, handles shutdown internally. */


/* Datagram (UDP) API... */

typedef struct SDLNet_DatagramSocket SDLNet_DatagramSocket;  /**< a UDP socket. Unreliable, packet-based transmission, with the usual pros/cons */

typedef struct SDLNet_Datagram
{
    SDLNet_Address *addr;  /**< this is unref'd by SDLNet_DestroyDatagram. You only need to ref it if you want to keep it. */
    Uint16 port;  /**< these do not have to come from the same port the receiver is bound to. */
    Uint8 *buf;
    int buflen;
} SDLNet_Datagram;

/**
 * Create and bind a new datagram socket.
 *
 * Datagram sockets follow different rules than stream sockets. They are not a
 * reliable stream of bytes but rather packets, they are not limited to
 * talking to a single other remote system, they do not maintain a single
 * "connection" that can be dropped, and they more nimble about network
 * failures at the expense of being more complex to use. What makes sense for
 * your app depends entirely on what your app is trying to accomplish.
 *
 * Generally the idea of a datagram socket is that you send data one chunk
 * ("packet") at a time to any address you want, and it arrives whenever it
 * gets there, even if later packets get there first, and maybe it doesn't get
 * there at all, and you don't know when anything of this happens by default.
 *
 * This function creates a new datagram socket.
 *
 * This function does not block, and is not asynchronous, as the system can
 * decide immediately if it can create a socket or not. If this returns
 * success, you can immediately start talking to the network.
 *
 * You can specify an address to listen for connections on; this address must
 * be local to the system, and probably one returned by
 * SDLNet_GetLocalAddresses(), but almost always you just want to specify NULL
 * here, to listen on any address available to the app.
 *
 * If you need to bind to a specific port (like a server), you should specify
 * it in the `port` argument; datagram servers should do this, so they can be
 * reached at a well-known port. If you only plan to initiate communications
 * (like a client), you should specify 0 and let the system pick an unused
 * port. Only one process can bind to a specific port at a time, so if you
 * aren't acting as a server, you should choose 0. Datagram sockets can send
 * individual packets to any port, so this just declares where data will
 * arrive for your socket.
 *
 * Datagram sockets don't employ any protocol (above the UDP level), so they
 * can talk to apps that aren't using SDL_net, but if you want to speak any
 * protocol beyond arbitrary packets of bytes, such as WebRTC, you'll have to
 * implement that yourself on top of the stream socket.
 *
 * Unlike BSD sockets or WinSock, you specify the port as a normal integer;
 * you do not have to byteswap it into "network order," as the library will
 * handle that for you.
 *
 * \param address the _local_ address to listen for connections on, or NULL.
 * \param the port on the local address to listen for connections on, or zero
 *            for the system to decide.
 * \returns a new SDLNet_DatagramSocket, or NULL on error; call SDL_GetError()
 *          for details.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_GetLocalAddresses
 * \sa SDLNet_DestroyDatagramSocket
 */
extern DECLSPEC SDLNet_DatagramSocket * SDLCALL SDLNet_CreateDatagramSocket(SDLNet_Address *addr, Uint16 port);

/**
 * Send a new packet over a datagram socket to a remote system.
 *
 * Datagram sockets send packets of data. They either arrive as complete
 * packets or they don't arrive at all, as opposed to stream sockets, where
 * individual bytes might trickle in as they attempt to reliably deliver a
 * stream of data.
 *
 * Datagram packets might arrive in a different order than you sent them, or
 * they may just be lost while travelling across the network. You have to plan
 * for this.
 *
 * You can send to any address and port on the network, but there has to be a
 * datagram socket waiting for the data on the other side for the packet not
 * to be lost.
 *
 * General wisdom is that you shouldn't send a packet larger than 1500 bytes
 * over the Internet, as bad routers might fragment or lose larger ones, but
 * this limit is not hardcoded into SDL_net and in good conditions you might
 * be able to send significantly more.
 *
 * This call never blocks; if it can't send the data immediately, the library
 * will queue it for later transmission. There is no query to see what is
 * still queued, as datagram transmission is unreliable, so you should never
 * assume anything about queued data.
 *
 * If there's a fatal error, this function will return -1. Datagram sockets
 * generally won't report failures, because there is no state like a
 * "connection" to fail at this level, but may report failure for
 * unrecoverable system-level conditions; once a datagram socket fails, you
 * should assume it is no longer usable and should destroy it with
 * SDL_DestroyDatagramSocket().
 *
 * \param sock the datagram socket to send data through
 * \param buf a pointer to the data to send as a single packet.
 * \param buflen the size of the data to send, in bytes.
 * \returns 0 if data sent or queued for transmission, -1 on failure; call
 *          SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_ReceiveDatagram
 */
extern DECLSPEC int SDLCALL SDLNet_SendDatagram(SDLNet_DatagramSocket *sock, SDLNet_Address *address, Uint16 port, const void *buf, int buflen);


/**
 * Receive a new packet that a remote system sent to a datagram socket.
 *
 * Datagram sockets send packets of data. They either arrive as complete
 * packets or they don't arrive at all, so you'll never receive half a packet.
 *
 * This call never blocks; if no new data isn't available at the time of the
 * call, it returns 0 immediately. The caller can try again later.
 *
 * On a successful call to this function, it returns zero, even if no new
 * packets are available, so you should check for a successful return and a
 * non-NULL value in `*dgram` to decide if a new packet is available.
 *
 * You must pass received packets to SDLNet_DestroyDatagram when you are done
 * with them. If you want to save the sender's address past this time, it is
 * safe to call SDLNet_RefAddress() on the address and hold onto the pointer,
 * so long as you call SDLNet_UnrefAddress() on it when you are done with it.
 *
 * Since datagrams can arrive from any address or port on the network without
 * prior warning, this information is available in the SDLNet_Datagram object
 * that is provided by this function, and this is the only way to know who to
 * reply to. Even if you aren't acting as a "server," packets can still arrive
 * at your socket if someone sends one.
 *
 * If there's a fatal error, this function will return -1. Datagram sockets
 * generally won't report failures, because there is no state like a
 * "connection" to fail at this level, but may report failure for
 * unrecoverable system-level conditions; once a datagram socket fails, you
 * should assume it is no longer usable and should destroy it with
 * SDL_DestroyDatagramSocket().
 *
 * \param sock the datagram socket to send data through
 * \param buf a pointer to the data to send as a single packet.
 * \param buflen the size of the data to send, in bytes.
 * \returns 0 if data sent or queued for transmission, -1 on failure; call
 *          SDL_GetError() for details.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_SendDatagram
 * \sa SDLNet_DestroyDatagram
 */
extern DECLSPEC int SDLCALL SDLNet_ReceiveDatagram(SDLNet_DatagramSocket *sock, SDLNet_Datagram **dgram);

/**
 * Dispose of a datagram packet previously received.
 *
 * You must pass packets received through SDLNet_ReceiveDatagram to this
 * function when you are done with them. This will free resources used by this
 * packet and unref its SDLNet_Address.
 *
 * If you want to save the sender's address from the packet past this time, it
 * is safe to call SDLNet_RefAddress() on the address and hold onto its
 * pointer, so long as you call SDLNet_UnrefAddress() on it when you are done
 * with it.
 *
 * Once you call this function, the datagram pointer becomes invalid and
 * should not be used again by the app.
 *
 * \param dgram the datagram packet to destroy.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC void SDLCALL SDLNet_DestroyDatagram(SDLNet_Datagram *dgram);


/**
 * Enable simulated datagram socket failures.
 *
 * Often times, testing a networked app on your development machine--which
 * might have a wired connection to a fast, reliable network service--won't
 * expose bugs that happen when networks intermittently fail in the real
 * world, when the wifi is flakey and firewalls get in the way.
 *
 * This function allows you to tell the library to pretend that some
 * percentage of datagram socket data transmission will fail.
 *
 * The library will randomly lose packets (both incoming and outgoing) at an
 * average matching `percent_loss`. Setting this to zero (the default) will
 * disable the simulation. Setting to 100 means _everything_ fails
 * unconditionally and no further data will get through. At what percent the
 * system merely borders on unusable is left as an exercise to the app
 * developer.
 *
 * This is intended for debugging purposes, to simulate real-world conditions
 * that are various degrees of terrible. You probably should _not_ call this
 * in production code, where you'll likely see real failures anyhow.
 *
 * \param sock The socket to set a failure rate on.
 * \param percent_loss A number between 0 and 100. Higher means more failures.
 *                     Zero to disable.
 *
 * \threadsafety It is safe to call this function from any thread.
 *
 * \since This function is available since SDL_Net 3.0.0.
 */
extern DECLSPEC void SDLCALL SDLNet_SimulateDatagramPacketLoss(SDLNet_DatagramSocket *sock, int percent_loss);


/**
 * Dispose of a previously-created datagram socket.
 *
 * This will _abandon_ any data queued for sending that hasn't made it to the
 * socket. If you need this data to arrive, you should wait for confirmation
 * from the remote computer in some form that you devise yourself. Queued data
 * is not guaranteed to arrive even if the library made efforts to transmit it
 * here.
 *
 * Any data that has arrived from the remote end of the connection that hasn't
 * been read yet is lost.
 *
 * \param sock datagram socket to destroy
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_CreateDatagramSocket
 * \sa SDLNet_SendDatagram
 * \sa SDLNet_ReceiveDatagram
 */
extern DECLSPEC void SDLCALL SDLNet_DestroyDatagramSocket(SDLNet_DatagramSocket *sock);

/* multi-socket polling ... */

/**
 * Block on multiple sockets until at least one has data available.
 *
 * This is a complex function that most apps won't need, but it could be used
 * to implement a more efficient server or i/o thread in some cases.
 *
 * This allows you to give it a list of objects and wait for new input to
 * become available on any of them. The calling thread is put to sleep until
 * such a time.
 *
 * The following things can be specified in the `vsockets` array, cast to
 * `void *`:
 *
 * - SDLNet_Server (reports new input when a connection is ready to be
 *   accepted with SDLNet_AcceptClient())
 * - SDLNet_StreamSocket (reports new input when the remote end has sent more
 *   bytes of data to be read with SDLNet_ReadFromStreamSocket).
 * - SDLNet_DatagramSocket (reports new input when a new packet arrives that
 *   can be read with SDLNet_ReceiveDatagram).
 *
 * This function takes a timeout value, represented in milliseconds, of how
 * long to wait for resolution to complete. Specifying a timeout of -1
 * instructs the library to wait indefinitely, and a timeout of 0 just checks
 * the current status and returns immediately.
 *
 * This returns the number of items that have new input, but it does not tell
 * you which ones; since access to them is non-blocking, you can just try to
 * read from each of them and see which are ready. If nothing is ready and the
 * timeout is reached, this returns zero. On error, this returns -1.
 *
 * \param vsockets an array of pointers to various objects that can be waited
 *                 on, each cast to a void pointer.
 * \param numsockets the number of pointers in the `vsockets` array.
 * \param timeout Number of milliseconds to wait for new input to become
 *                available. -1 to wait indefinitely, 0 to check once without
 *                waiting.
 *
 * \threadsafety You should not operate on the same socket from multiple
 *               threads at the same time without supplying a serialization
 *               mechanism. However, different threads may access different
 *               sockets at the same time without problems.
 *
 * \since This function is available since SDL_Net 3.0.0.
 *
 * \sa SDLNet_CreateDatagramSocket
 * \sa SDLNet_SendDatagram
 * \sa SDLNet_ReceiveDatagram
 */
extern DECLSPEC int SDLCALL SDLNet_WaitUntilInputAvailable(void **vsockets, int numsockets, Sint32 timeout);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_NET_H_ */

