/*
    NETLIB:  An example cross-platform network library for use with SDL
    Copyright (C) 1997-1999  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    5635-34 Springhouse Dr.
    Pleasanton, CA 94588 (USA)
    slouken@devolution.com
    
    
    HISTORY:
    
    2000/3/24 - rrwood - MacOS-related changes to SDLNet_TCP_Open() to correctly get returned IP:port info
    2000/3/25 - rrwood - rename "address" field of struct _TCPsocket to "remoteAddress"
    2000/3/25 - rrwood - add "localAddress" field to struct _TCPsocket
*/

#include "SDLnetsys.h"
#include "SDL_net.h"

/* The network API for TCP sockets */

/* Since the UNIX/Win32/BeOS code is so different from MacOS,
   we'll just have two completely different sections here.
*/

#ifdef MACOS_OPENTRANSPORT

#include <Threads.h>
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#include <OTDebug.h>

struct _TCPsocket {
	int ready;
	SOCKET channel;
	IPaddress remoteAddress;
	IPaddress localAddress;
	int sflag;

	int rcvdPassConn;
};

#if TARGET_API_MAC_CARBON
/* for Carbon */
OTNotifyUPP notifier;
#endif

/* Input: ep - endpointref on which to negotiate the option
			enableReuseIPMode - desired option setting - true/false
   Return: kOTNoError indicates that the option was successfully negotiated
   			OSStatus is an error if < 0, otherwise, the status field is
   			returned and is > 0.
   	
   	IMPORTANT NOTE: The endpoint is assumed to be in synchronous more, otherwise
   			this code will not function as desired
*/


OSStatus DoNegotiateIPReuseAddrOption(EndpointRef ep, Boolean enableReuseIPMode)

{
	UInt8		buf[kOTFourByteOptionSize];	// define buffer for fourByte Option size
	TOption*	opt;						// option ptr to make items easier to access
	TOptMgmt	req;
	TOptMgmt	ret;
	OSStatus	err;
	
	if (!OTIsSynchronous(ep))
	{
		return (-1);
	}
	opt = (TOption*)buf;					// set option ptr to buffer
	req.opt.buf	= buf;
	req.opt.len	= sizeof(buf);
	req.flags	= T_NEGOTIATE;				// negotiate for option

	ret.opt.buf = buf;
	ret.opt.maxlen = kOTFourByteOptionSize;

	opt->level	= INET_IP;					// dealing with an IP Level function
	opt->name	= IP_REUSEADDR;
	opt->len	= kOTFourByteOptionSize;
	opt->status = 0;
	*(UInt32*)opt->value = enableReuseIPMode;		// set the desired option level, true or false

	err = OTOptionManagement(ep, &req, &ret);
	
		// if no error then return the option status value
	if (err == kOTNoError)
	{
		if (opt->status != T_SUCCESS)
			err = opt->status;
		else
			err = kOTNoError;
	}
				
	return err;
}

static pascal void YieldingNotifier(void* contextPtr, OTEventCode code, 
									   OTResult result, void* cookie)
{
#pragma unused(contextPtr)
#pragma unused(result)
#pragma unused(cookie)
	
	switch (code) {
	    case kOTSyncIdleEvent:
		/* Yield the CPU to OS events */
#if ! TARGET_API_MAC_CARBON
		WaitNextEvent(~0, nil, 0, nil);
#else
		YieldToAnyThread();
#endif
		break;
	    default:
		break;
	}
}

/* Open a TCP network socket
   If 'remote' is NULL, this creates a local server socket on the given port,
   otherwise a TCP connection to the remote host and port is attempted.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Open(IPaddress *ip)
{
	TCPsocket sock;
	extern Uint32 OTlocalhost;
	EndpointRef	dummyEP;
	
	// Open a dummy end-point.
	// Not sure if this is really necessary....	
#if ! TARGET_API_MAC_CARBON
	dummyEP = OTOpenEndpoint( OTCreateConfiguration("tcp"), 0, nil, nil );
#else
	dummyEP = OTOpenEndpointInContext( OTCreateConfiguration("tcp"), 0, nil, nil, nil );
	/* create notifier for carbon */
	notifier = NewOTNotifyUPP( YieldingNotifier );
	if( notifier == NULL )
	{
		SDLNet_SetError("Out of memory");
		goto error_return;
	}
#endif
	
	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		SDLNet_SetError("Out of memory");
		goto error_return;
	}

	/* Connect to remote, or bind locally, as appropriate */
	if ( (ip->host != INADDR_NONE) && (ip->host != INADDR_ANY) ) {

	// #########  Connecting to remote
		OSStatus status;
		TCall sndTCall;
		InetAddress sndInetAddress;
		TCall rcvTCall;
		InetAddress rcvInetAddress;
		TBind assignedAddressTBind;
		InetAddress assignedInetAddress;
#if ! TARGET_API_MAC_CARBON
		sock->channel = OTOpenEndpoint(OTCreateConfiguration(kTCPName),
							0, nil, &status);
#else
		sock->channel = OTOpenEndpointInContext(OTCreateConfiguration(kTCPName),
							0, nil, &status, nil);
#endif
		if ( sock->channel == INVALID_SOCKET ) {
			SDLNet_SetError("Couldn't create socket");
			goto error_return;
		}
			
		DoNegotiateIPReuseAddrOption( sock->channel, true );
			
#if ! TARGET_API_MAC_CARBON
		status = OTInstallNotifier(sock->channel, YieldingNotifier, nil);
#else
		status = OTInstallNotifier(sock->channel, notifier, nil);
#endif
			
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't install socket OT notifier");
			goto error_return;
		}

		status = OTSetBlocking( sock->channel );
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
			goto error_return;
		}
		status = OTSetSynchronous( sock->channel );
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
			goto error_return;
		}
		status = OTUseSyncIdleEvents(sock->channel, true);
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
			goto error_return;
		}

		/* I used to use OTAlloc() to set up TCall/TBind structures, 
		  	 but was getting bogus results in some cases-- weird */
		memset(&assignedAddressTBind, 0, sizeof(assignedAddressTBind));
		assignedAddressTBind.addr.maxlen = sizeof(assignedInetAddress);
		assignedAddressTBind.addr.len = sizeof(assignedInetAddress);
		assignedAddressTBind.addr.buf = (UInt8 *) &assignedInetAddress;
		  
		status = OTBind(sock->channel, nil, &assignedAddressTBind);
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't bind to local port");
			goto error_return;
		}
		  
		/* If the assigned IP is 0, then it's the default interface, so look up *that* IP */
		if (assignedInetAddress.fHost == 0) {
		  	InetInterfaceInfo theInetInterfaceInfo;
		  	
			if (OTInetGetInterfaceInfo(&theInetInterfaceInfo,kDefaultInetInterface) == noErr)
			{
				assignedInetAddress.fHost = theInetInterfaceInfo.fAddress;
			}
		}

		sock->localAddress.host = assignedInetAddress.fHost;
		sock->localAddress.port = assignedInetAddress.fPort;

		memset(&sndTCall, 0, sizeof(sndTCall));
		sndTCall.addr.maxlen = sizeof(sndInetAddress);
		sndTCall.addr.len = sizeof(sndInetAddress);
		sndTCall.addr.buf = (UInt8 *) &sndInetAddress;
		  
		OTInitInetAddress(&sndInetAddress, ip->port, ip->host);

		memset(&rcvTCall, 0, sizeof(rcvTCall));
		rcvTCall.addr.maxlen = sizeof(rcvInetAddress);
		rcvTCall.addr.len = sizeof(rcvInetAddress);
		rcvTCall.addr.buf = (UInt8 *) &rcvInetAddress;
		  
		status = OTConnect(sock->channel, &sndTCall, &rcvTCall);
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't connect to remote host");
			goto error_return;
		}
		  
		sock->remoteAddress.host = rcvInetAddress.fHost;
		sock->remoteAddress.port = rcvInetAddress.fPort;
		sock->sflag = 0;
	} else {

	// ##########  Binding locally
	
		OSStatus status;
		TBind bindReq;
		InetInterfaceInfo	theInetInterfaceInfo;
		InetAddress	ipAddress;

	// First, get InetInterfaceInfo.
	// I don't search for all of them.
	// Does that matter ?
			
		if( OTInetGetInterfaceInfo( &theInetInterfaceInfo, 0 ) != noErr )
		{
			SDLNet_SetError( "Could not get InetInterfaceInfo");
			goto error_return;
		}
			
	// Second, open the endpoint
#if ! TARGET_API_MAC_CARBON
		sock->channel = OTOpenEndpoint(OTCreateConfiguration("tilisten, tcp"),
							0, nil, &status);
#else
		sock->channel = OTOpenEndpointInContext(OTCreateConfiguration("tilisten, tcp"),
							0, nil, &status, nil);
#endif

		if ( sock->channel == INVALID_SOCKET ) {
			SDLNet_SetError("Couldn't create socket");
			goto error_return;
		}
			
		DoNegotiateIPReuseAddrOption( sock->channel, true );

	// And more options.
	// 		Synchronous-Blocking
#if ! TARGET_API_MAC_CARBON
		status = OTInstallNotifier(sock->channel, YieldingNotifier, nil);
#else
		status = OTInstallNotifier(sock->channel, notifier, nil);
#endif
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't install socket OT notifier");
			goto error_return;
		}
		status = OTSetBlocking( sock->channel );
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
			goto error_return;
		}
		status = OTSetSynchronous( sock->channel );
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
			goto error_return;
		}
		status = OTUseSyncIdleEvents(sock->channel, true);
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
			goto error_return;
		}
			
	// Now is the time for Binding.
			
		OTInitInetAddress( &ipAddress, ip->port, theInetInterfaceInfo.fAddress );

		/* For the moment, go with qlen = 1, thought if we allocate the endpoint as a tilisten
		 endpoint, then handling qlen > 1 is trivial */
		bindReq.addr.buf = (UInt8*) &ipAddress;
		bindReq.addr.len = sizeof( ipAddress );
		bindReq.qlen = 1;

		status = OTBind(sock->channel, &bindReq, nil);
		if ( status != noErr ) {
			SDLNet_SetError("Couldn't bind to local port");
			goto error_return;
		}
	
	// All right, everything is ready now.
	
		sock->localAddress.host = ipAddress.fHost;
		sock->localAddress.port = ipAddress.fPort;
		sock->sflag = 1;
	}
	sock->ready = 0;

	if( dummyEP != nil )
		OTCloseProvider( dummyEP );

	/* The socket is ready */
	return(sock);

error_return:
	if( dummyEP != nil )
		OTCloseProvider( dummyEP );
	SDLNet_TCP_Close(sock);
	return(NULL);
}

/* Accept an incoming connection on the given server socket.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Accept(TCPsocket server)
{
	TCPsocket sock;

	/* Only server sockets can accept */
	if ( ! server->sflag ) {
		SDLNet_SetError("Only server sockets can accept()");
		return(NULL);
	}
	server->ready = 0;

	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		SDLNet_SetError("Out of memory");
		goto error_return;
	}

	/* Accept a new TCP connection on a server socket */
	{ InetAddress peer;
	  TCall peerinfo;
	  OSStatus status;


/* Hmm-- what about the select() on this puppy?  Should check for incoing connection request, right? */
 
	  memset(&peerinfo, 0, (sizeof peerinfo));
	  peerinfo.addr.buf = (UInt8 *) &peer;
	  peerinfo.addr.maxlen = sizeof(peer);
	  status = OTListen(server->channel, &peerinfo);
	  if ( status != noErr ) {
		/*SDLNet_SetError("OT: listen() failed");
		goto error_return;*/
		return NULL;
	  }
	  #if ! TARGET_API_MAC_CARBON
	  sock->channel = OTOpenEndpoint(OTCreateConfiguration(kTCPName),
							0, nil, &status);
	  #else
	  sock->channel = OTOpenEndpointInContext(OTCreateConfiguration(kTCPName),
							0, nil, &status, nil);
	  #endif

	  if ( sock->channel == INVALID_SOCKET ) {
		SDLNet_SetError("accept() failed");
		goto error_return;
	  }
  #if ! TARGET_API_MAC_CARBON
	  OTInstallNotifier( sock->channel, YieldingNotifier, nil );
  #else
	  OTInstallNotifier(sock->channel, notifier, nil);
  #endif
	  OTSetBlocking( sock->channel );
	  OTSetSynchronous( sock->channel );
	  OTUseSyncIdleEvents( sock->channel, true );
	  
	  status = OTAccept(server->channel, sock->channel, &peerinfo);
	  if ( status != noErr ) {
		SDLNet_SetError("OT: accept() failed");
		goto error_return;
	  }
	  sock->remoteAddress.host = peer.fHost;
	  sock->remoteAddress.port = peer.fPort;
	}

	sock->sflag = 0;
	sock->ready = 0;

	/* The socket is ready */
	return(sock);

error_return:
	SDLNet_TCP_Close(sock);
	return(NULL);
}

/* Get the IP address of the remote system associated with the socket.
   If the socket is a server socket, this function returns NULL.
*/
IPaddress *SDLNet_TCP_GetPeerAddress(TCPsocket sock)
{
	if ( sock->sflag ) {
		return(NULL);
	}
	return(&sock->remoteAddress);
}

/* Send 'len' bytes of 'data' over the non-server socket 'sock'
   This function returns the actual amount of data sent.  If the return value
   is less than the amount of data sent, then either the remote connection was
   closed, or an unknown socket error occurred.
*/
int SDLNet_TCP_Send(TCPsocket sock, void *datap, int len)
{
	Uint8 *data = (Uint8 *)datap;	/* For pointer arithmetic */
	int sent, left;

	/* Server sockets are for accepting connections only */
	if ( sock->sflag ) {
		SDLNet_SetError("Server sockets cannot send");
		return(-1);
	}

	/* Keep sending data until it's sent or an error occurs */
	left = len;
	sent = 0;
	errno = 0;
	do {
		len = OTSnd(sock->channel, data, left, 0);
		if (len == kOTFlowErr)
			len = 0;
		if ( len > 0 ) {
			sent += len;
			left -= len;
			data += len;
		}
	} while ( (left > 0) && (len > 0) );

	return(sent);
}

/* Receive up to 'maxlen' bytes of data over the non-server socket 'sock',
   and store them in the buffer pointed to by 'data'.
   This function returns the actual amount of data received.  If the return
   value is less than or equal to zero, then either the remote connection was
   closed, or an unknown socket error occurred.
*/
int SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen)
{
	int len;

	/* Server sockets are for accepting connections only */
	if ( sock->sflag ) {
		SDLNet_SetError("Server sockets cannot receive");
		return(-1);
	}

	len = OTRcv(sock->channel, data, maxlen, 0);
	if (len == kOTNoDataErr) 
		len = 0;
	sock->ready = 0;
	return(len);
}

/* Close a TCP network socket */
void SDLNet_TCP_Close(TCPsocket sock)
{
	if ( sock != NULL ) {
		if ( sock->channel != INVALID_SOCKET ) {
			closesocket(sock->channel);
		}
		free(sock);
	}
}

#else /* !MACOS_OPENTRANSPORT */

struct _TCPsocket {
	int ready;
	SOCKET channel;
	IPaddress remoteAddress;
	IPaddress localAddress;
	int sflag;
};

/* Open a TCP network socket
   If 'remote' is NULL, this creates a local server socket on the given port,
   otherwise a TCP connection to the remote host and port is attempted.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Open(IPaddress *ip)
{
	TCPsocket sock;
	struct sockaddr_in sock_addr;

	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		SDLNet_SetError("Out of memory");
		goto error_return;
	}

	/* Open the socket */
	sock->channel = socket(AF_INET, SOCK_STREAM, 0);
	if ( sock->channel == INVALID_SOCKET ) {
		SDLNet_SetError("Couldn't create socket");
		goto error_return;
	}

	/* Connect to remote, or bind locally, as appropriate */
	if ( (ip->host != INADDR_NONE) && (ip->host != INADDR_ANY) ) {

	// #########  Connecting to remote
	
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = ip->host;
		sock_addr.sin_port = ip->port;

		/* Connect to the remote host */
		if ( connect(sock->channel, (struct sockaddr *)&sock_addr,
				sizeof(sock_addr)) == SOCKET_ERROR ) {
			SDLNet_SetError("Couldn't connect to remote host");
			goto error_return;
		}
		sock->sflag = 0;
	} else {

	// ##########  Binding locally
	
		memset(&sock_addr, 0, sizeof(sock_addr));
		sock_addr.sin_family = AF_INET;
		sock_addr.sin_addr.s_addr = INADDR_ANY;
		sock_addr.sin_port = ip->port;

		/* Bind the socket for listening */
		if ( bind(sock->channel, (struct sockaddr *)&sock_addr,
				sizeof(sock_addr)) == SOCKET_ERROR ) {
			SDLNet_SetError("Couldn't bind to local port");
			goto error_return;
		}
		if ( listen(sock->channel, 5) == SOCKET_ERROR ) {
			SDLNet_SetError("Couldn't listen to local port");
			goto error_return;
		}
#ifdef O_NONBLOCK
		/* Set the socket to non-blocking mode for accept() */
		fcntl(sock->channel, F_SETFL, O_NONBLOCK);
#else
#ifdef WIN32
		{
		  /* passing a non-zero value, socket mode set non-blocking */
		  unsigned long mode = 1;
		  ioctlsocket (sock->channel, FIONBIO, &mode);
		}
#else
#warning How do we set non-blocking mode on other operating systems?
#endif /* WIN32 */
#endif /* O_NONBLOCK */

		sock->sflag = 1;
	}
	sock->ready = 0;

#ifdef TCP_NODELAY
	/* Set the nodelay TCP option for real-time games */
	{ int yes = 1;
	setsockopt(sock->channel, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
	}
#endif /* TCP_NODELAY */

	/* Fill in the channel host address */
	sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
	sock->remoteAddress.port = sock_addr.sin_port;

	/* The socket is ready */
	return(sock);

error_return:
	SDLNet_TCP_Close(sock);
	return(NULL);
}

/* Accept an incoming connection on the given server socket.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Accept(TCPsocket server)
{
	TCPsocket sock;
	struct sockaddr_in sock_addr;
	int sock_alen;

	/* Only server sockets can accept */
	if ( ! server->sflag ) {
		SDLNet_SetError("Only server sockets can accept()");
		return(NULL);
	}
	server->ready = 0;

	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		SDLNet_SetError("Out of memory");
		goto error_return;
	}

	/* Accept a new TCP connection on a server socket */
	sock_alen = sizeof(sock_addr);
	sock->channel = accept(server->channel, (struct sockaddr *)&sock_addr,
#ifdef USE_GUSI_SOCKETS
						(unsigned int *)&sock_alen);
#else
								&sock_alen);
#endif
	if ( sock->channel == SOCKET_ERROR ) {
		SDLNet_SetError("accept() failed");
		goto error_return;
	}
	sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
	sock->remoteAddress.port = sock_addr.sin_port;

	sock->sflag = 0;
	sock->ready = 0;

	/* The socket is ready */
	return(sock);

error_return:
	SDLNet_TCP_Close(sock);
	return(NULL);
}

/* Get the IP address of the remote system associated with the socket.
   If the socket is a server socket, this function returns NULL.
*/
IPaddress *SDLNet_TCP_GetPeerAddress(TCPsocket sock)
{
	if ( sock->sflag ) {
		return(NULL);
	}
	return(&sock->remoteAddress);
}

/* Send 'len' bytes of 'data' over the non-server socket 'sock'
   This function returns the actual amount of data sent.  If the return value
   is less than the amount of data sent, then either the remote connection was
   closed, or an unknown socket error occurred.
*/
int SDLNet_TCP_Send(TCPsocket sock, void *datap, int len)
{
	Uint8 *data = (Uint8 *)datap;	/* For pointer arithmetic */
	int sent, left;

	/* Server sockets are for accepting connections only */
	if ( sock->sflag ) {
		SDLNet_SetError("Server sockets cannot send");
		return(-1);
	}

	/* Keep sending data until it's sent or an error occurs */
	left = len;
	sent = 0;
	errno = 0;
	do {
		len = send(sock->channel, (const char *) data, left, 0);
		if ( len > 0 ) {
			sent += len;
			left -= len;
			data += len;
		}
	} while ( (left > 0) && ((len > 0) || (errno == EINTR)) );

	return(sent);
}

/* Receive up to 'maxlen' bytes of data over the non-server socket 'sock',
   and store them in the buffer pointed to by 'data'.
   This function returns the actual amount of data received.  If the return
   value is less than or equal to zero, then either the remote connection was
   closed, or an unknown socket error occurred.
*/
int SDLNet_TCP_Recv(TCPsocket sock, void *data, int maxlen)
{
	int len;

	/* Server sockets are for accepting connections only */
	if ( sock->sflag ) {
		SDLNet_SetError("Server sockets cannot receive");
		return(-1);
	}

	errno = 0;
	do {
		len = recv(sock->channel, (char *) data, maxlen, 0);
	} while ( errno == EINTR );

	sock->ready = 0;
	return(len);
}

/* Close a TCP network socket */
void SDLNet_TCP_Close(TCPsocket sock)
{
	if ( sock != NULL ) {
		if ( sock->channel != INVALID_SOCKET ) {
			closesocket(sock->channel);
		}
		free(sock);
	}
}

#endif /* MACOS_OPENTRANSPORT */
