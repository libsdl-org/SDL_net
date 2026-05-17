# Migrating to SDL_net 3.0

SDL_net 3.0 (aka "SDL3_net") is a dramatically different library than
previous versions. The API has been completely redesigned. There is no
compatibility layer. If you want to use it, you have to migrate to it.

SDL3_net requires SDL3. It relies on many features that are new to SDL3,
both internally and in the public API, so if your project is on SDL 1.2 or
SDL2, you'll have to move your project to SDL3 at the same time.

That being said, we think SDL3_net and SDL3 are both great pieces of
software--significant improvements over their previous versions--and we
think that once you move to them, you'll be quite happy you did.

There are some things that don't have simple replacements that can be changed
mechanically to migrate to SDL3_net. The new API is in many ways more
powerful, but also much simpler.

This migration guide will attempt to walk through the important details but
it's possible that some things can't be done the same way. Feel free to open
bug reports if you're totally stuck.

The end of this document provides a "tl;dr" section that lists each SDL2_net
function name and a brief explanation about what to do with it.


## Things that are totally gone

- Building SDL_net without SDL at all (the `WITHOUT_SDL` define in SDL2_net)
  is no longer an option. SDL3 is required, both in including the SDL_net
  public headers and in the library's implementation code.
- Network addresses used to be in a public struct (IPaddress). The new object
  (NET_Address) is an opaque type. If your app was setting or reading IPv4
  addresses directly in the struct, this will have to change. SDL3_net offers
  functions to create and query NET_Address objects.
- UDP transmission no longer deals with "channels." This was a confusing
  interface in general. One could build a channel system on top of SDL3_net
  easily with an array of NET_Address objects, but NET_DatagramSocket only
  deals with sending and receiving from addresses now.
- UDP packets had several API functions for memory management. In SDL3_net,
  you just provide a pointer to data you want to send, or a buffer to receive
  into, and that's it. Internal memory management is done with SDL_malloc().
- Socket sets are gone. The entire API has collapsed down into
  NET_WaitUntilInputAvailable(), which just takes an array of
  sockets to check.


## Including SDL3_net

The proper way to include SDL3_net's header is:

```c
#include <SDL3_net/SDL_net.h>
```

Like SDL3, the new convention is to use `<>` brackets and a subdirectory.


## Symbol names

In SDL2_net, all functions started with `SDLNet_` and macros started with
`SDLNET_`. In SDL3_net, everything starts with `NET_`.


## Versioning

Versions are now bits packed into a single int instead of a struct, using
SDL3's usual magic for unpacking these.


## Return values

Most things that returned an int in SDL2_net would return a -1 or 0 to report
error or success, respectively. In SDL3_net, these sorts of functions return
bools, with true for success and false for failure. Be careful in migrating,
as your compiler might catch this and warn you...

```c
if (NET_Init() < 0) { /* failure! */ }
```

...but it won't catch this...

```c
if (!NET_Init()) { /* Success in SDL2_net, but failure in SDL3_net! */ }
```


## Multiple protocols

SDL2_net's public API was hardcoded to offer IPv4 networking. SDL3_net has
replaced its IPaddress struct with the opaque NET_Address. This can handle
addresses from different network protocols, including future ones that haven't
been conceived of yet. But most notably: it means that your app can abstractly
handle both IPv4 and IPv6.

(In theory, this could also work with, say, an IPX or AppleTalk network in the
wild, but it might require small fixes to SDL3_net's code. Send patches.)

Using NET_ResolveHostname() might give you any available protocol, letting
the OS decide the best option for the system.

SDL3_net goes even further: it offers "dual-stack" sockets. If you create a
server or datagram socket bound to a NULL address, it will try to create both
IPv4 and IPv6 OS-level sockets under the hood, and listen on both. This is
not just an IPv4-over-IPv6 bridge. This makes it possible that you'll get
traffic from both protocols at the same time, with clients coming from
addresses that look like 157.90.7.176 and 2a01:4f8:251:5583::2 simultaneously,
and your server can trivially bridge between them.


## Blocking vs Non-blocking

SDL2_net was (mostly) a blocking API. If it took time to complete a network
operation, you would have to wait. Some waits might be short, like passing a
datagram to the kernel's transmit buffers, but others might take many seconds,
depending on network conditions. It offered a wrapper over the select() call
with what it called "socket sets" to query if data was available, to avoid
blocking. A DNS lookup was going to take as long as it takes.

SDL3_net takes the opposite approach: everything is _non-blocking_ now. If
data is not available, read calls will return immediately, reporting this.
If data cannot be sent right away, it will queue internally until the library
can send it later. DNS queries happen on a pool of background threads and the
app can check in from time to time to see if there are results available.

The concept of "socket sets" has collapsed down into a single function:
NET_WaitUntilInputAvailable(), which takes an array of various SDL_net
objects and a timeout. A non-blocking query has a timeout of zero, an
indefinite wait until _something_ happens is a timeout of -1.

If you prefer blocking behavior, there are functions to wait for specific
things to happen (with an optional timeout). These will put the calling
thread to sleep for efficiency.

- NET_WaitUntilResolved: Wait for a specific DNS query to finish.
- NET_WaitUntilConnected: Wait for a specific stream socket to connect.
- NET_WaitUntilStreamSocketDrained: Wait for a stream socket to send all
  queued data.
- NET_WaitUntilInputAvailable: Like the other NET_WaitUntil* functions, but
  check multiple things at the same time.

There are simple non-blocking query functions, too: NET_GetAddressStatus,
NET_GetConnectionStatus, NET_GetStreamSocketPendingWrites.


## Thread safety

All functions in SDL3_net are thread safe, with one caveat: you can not
operate on the same socket from two threads at once. Two threads can work
with two separate sockets at the same time without problems.


## Network byte order

In SDL2_net, things like network addresses and port numbers might need to be
in "network byte order" (bigendian) in some cases. In SDL3_net, these values
are always in "host byte order" (the native byte order of the system), and
SDL3_net will manage byteswapping behind the scenes as necessary.

Of course any payload bytes sent over the internet might comes from any
computer with any byte order, so the app still needs to deal with that exactly
the same as they did with SDL2_net.


## Initialization

SDL2_net expected you to call SDL_Init() before SDLNet_Init(). Now you don't
have to.

```c
if (!NET_Init()) {
    SDL_Log("NET_Init failed: %s", SDL_GetError());
} else {
    SDL_Log("SDL_net is ready!");
}
```

NET_Init() is reference-counted; it's safe to call it more than once, and
only the first call will do actual initialization tasks. Likewise, actual
deinitialization with NET_Quit() will only happen when it has been called
as many times as NET_Init() has. This allows different parts of the program
to initialize and use SDL3_net without knowing about other parts using it
too.


## Stream sockets

SDL2_net represents server ("listen") sockets with the same type as connected
sockets: a TCPsocket object. In SDL3_net, these are split into two separate
objects: NET_Server and NET_StreamSocket.

NET_StreamSocket is the thing that actually transmits data over the network
between a client and server; both ends of the connection have one and both
can read or write to theirs.

NET_Server is the thing that will listen for new connections from clients, and
generate the server's end of the NET_StreamSocket for each one.

Creating NET_Server with a NULL address will possibly listen on both IPv4 and
IPv6 networks separately under the hood.


## Datagram sockets

SDLNet_UDP_Open() would let you specify a port but only binds to INADDR_ANY.
SDL3_net's NET_CreateDatagramSocket() will let you bind to specific interfaces
or specify a NULL address for the equivalent of INADDR_ANY (but it might
listen on both IPv4 and IPv6 interfaces under the hood).


## Broadcasting

In SDL2_net, you could send UDP packets to the subnet's broadcast address
(if you could figure it out), or 255.255.255.255 (which wouldn't work on
Windows).

In SDL3_net, you create a datagram socket with the
`NET_PROP_DATAGRAM_SOCKET_ALLOW_BROADCAST_BOOLEAN` property, and call
NET_SendDatagram() with a NULL address.

This abstracts out broadcasting for different protocols (including IPv6,
where SDL3_net fakes this under the hood with multicasting on your behalf).
SDL3_net will also make efforts to broadcast to all network interfaces if
appropriate, and broadcast to different network protocols. As such, it's
possible that a single broadcast packet will arrive for the same app on
multiple interfaces, and it might appear to come from different computers,
since it might have an IPv4 and IPv6 address. Plan accordingly, so you know
whether to drop duplicate packets.

Futher, if you broadcast from a client in order to locate servers, and that
server is listening on both IPv4 and IPv6, you might get two responses that
appear to be different servers. Plan to add something unique in the payload
so you can recognize they are the same machine, or separate out IPv4/IPv6
servers in your server browser, etc.

(And, of course, please limit the amount of broadcasting your app does in
general, to be a good citizen of your subnet!)


## Simulating failure

SDL2_net had SDLNet_UDP_SetPacketLoss(). SDL3_net extends this to other pieces
of the network stack: you can set up separate simulated failure percentages
for stream sockets (being reliable, they will introduce delays into the stream
and possibly just drop the connection outright sometimes), and DNS lookups
(some will take longer, and some will fail, as if packets aren't arriving from
the DNS server or domains are mysteriously missing).

These functions are NET_SimulateAddressResolutionLoss(),
NET_SimulateStreamPacketLoss(), and NET_SimulateDatagramPacketLoss().


## tl;dr

A very brief comment on what to do with each symbol in SDL2_net.

Some of these are listed as "no equivalent in SDL3_net" but could possibly
be added if there is a need. If you're stuck, please file an issue and we
can discuss it!

- SDL_NET_MAJOR_VERSION => SDL_NET_MAJOR_VERSION
- SDL_NET_MINOR_VERSION => SDL_NET_MAJOR_VERSION
- SDL_NET_PATCHLEVEL => SDL_NET_MICRO_VERSION
- SDL_NET_VERSION => SDL_NET_VERSION
- SDL_NET_VERSION_ATLEAST => SDL_NET_VERSION_ATLEAST
- SDLNet_Linked_Version => NET_Version
- SDL_NET_COMPILEDVERSION => SDL_NET_VERSION
- SDLNet_Init => NET_Init
- SDLNet_Quit => NET_Quit
- IPaddress => NET_Address
- SDLNet_ResolveHost => NET_ResolveHostname (the port number from SDL2_net is specified in other functions).
- SDLNet_ResolveIP => NET_GetAddressString (this does not attempt to figure out the hostname via DNS, though! File an issue if you need this!)
- SDLNet_GetLocalAddresses => NET_GetLocalAddresses
- TCPsocket => NET_StreamSocket
- SDLNet_TCP_OpenServer => NET_CreateServer
- SDLNet_TCP_OpenClient => NET_CreateClient
- SDLNet_TCP_Open => No direct equivalent, use NET_CreateServer or NET_CreateClient as appropriate.
- SDLNet_TCP_Accept => NET_AcceptClient
- SDLNet_TCP_GetPeerAddress => NET_GetStreamSocketAddress
- SDLNet_TCP_Send => NET_WriteToStreamSocket
- SDLNet_TCP_Recv => NET_ReadFromStreamSocket
- SDLNet_TCP_Close => NET_DestroyStreamSocket
- SDLNET_MAX_UDPCHANNELS => no equivalent in SDL3_net.
- SDLNET_MAX_UDPADDRESSES => no equivalent in SDL3_net.
- UDPpacket => NET_Datagram
- SDLNet_AllocPacket => no equivalent in SDL3_net.
- SDLNet_ResizePacket => no equivalent in SDL3_net.
- SDLNet_FreePacket => NET_DestroyDatagram
- SDLNet_AllocPacketV => no equivalent in SDL3_net.
- SDLNet_FreePacketV => NET_DestroyDatagram in a loop.
- UDPsocket => NET_DatagramSocket
- SDLNet_UDP_Open => NET_CreateDatagramSocket
- SDLNet_UDP_SetPacketLoss => NET_SimulateDatagramPacketLoss
- SDLNet_UDP_Bind => no equivalent in SDL3_net.
- SDLNet_UDP_Unbind => no equivalent in SDL3_net.
- SDLNet_UDP_GetPeerAddress => no equivalent in SDL3_net. NET_Datagram::addr has it in received packets.
- SDLNet_UDP_SendV => NET_SendDatagram in a loop.
- SDLNet_UDP_Send => NET_SendDatagram
- SDLNet_UDP_RecvV => NET_ReceiveDatagram in a loop.
- SDLNet_UDP_Recv => NET_ReceiveDatagram
- SDLNet_UDP_Close => NET_DestroyDatagramSocket
- SDLNet_GenericSocket => `void *`
- SDLNet_SocketSet => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_AllocSocketSet => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_AddSocket => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_TCP_AddSocket => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_UDP_AddSocket => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_DelSocket => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_TCP_DelSocket => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_UDP_DelSocket => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_CheckSockets => NET_WaitUntilInputAvailable
- SDLNet_SocketReady => no equivalent in SDL3_net. Just try to use them, they're all non-blocking.
- SDLNet_FreeSocketSet => no equivalent in SDL3_net. Just pass an array of things to NET_WaitUntilInputAvailable().
- SDLNet_SetError => SDL_SetError
- SDLNet_GetError => SDL_GetError
- SDLNet_Write16 => SDL_SwapBE16
- SDLNet_Write32 => SDL_SwapBE32
- SDLNet_Read16 => SDL_SwapBE16
- SDLNet_Read32 => SDL_SwapBE32

