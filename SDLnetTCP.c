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

struct _TCPsocket {
	int ready;
	SOCKET channel;
	IPaddress remoteAddress;
	IPaddress localAddress;
	int sflag;

#ifdef macintosh
	int rcvdPassConn;
#endif
};

#ifdef macintosh
static pascal void YieldingNotifier(void* contextPtr, OTEventCode code, 
									   OTResult result, void* cookie)
{
#pragma unused(result)
#pragma unused(cookie)
	
	switch (code) {
		case kOTSyncIdleEvent:
		{
			/* Yield the CPU to OS events */
			
			/* Hmm-- shouldn't we be calling YieldToAnyThread() here? */
			
			WaitNextEvent(~0, nil, 0, nil);
			
			break;
		}
		
		case T_PASSCON:
		{
		TCPsocket sock = (TCPsocket) contextPtr;
			
			/* Good thing this is never called at interrupt time or
			   I'd have a race condition to deal with in the accept() code */
			sock->rcvdPassConn = true;
		}
			
		default:
			break;
	}
}
#endif /* macintosh */

/* Open a TCP network socket
   If 'remote' is NULL, this creates a local server socket on the given port,
   otherwise a TCP connection to the remote host and port is attempted.
   The newly created socket is returned, or NULL if there was an error.
*/
TCPsocket SDLNet_TCP_Open(IPaddress *ip)
{
	TCPsocket sock;
#ifdef macintosh
	extern Uint32 OTlocalhost;
	OSStatus status;
#else
	struct sockaddr_in sock_addr;
#endif

	/* Allocate a TCP socket structure */
	sock = (TCPsocket)malloc(sizeof(*sock));
	if ( sock == NULL ) {
		SDLNet_SetError("Out of memory");
		goto error_return;
	}

	/* Open the socket */
#ifdef macintosh
	{ 
	  sock->channel = OTOpenEndpoint(OTCreateConfiguration(kTCPName),
							0, nil, &status);
	}
#else
	sock->channel = socket(AF_INET, SOCK_STREAM, 0);
#endif
	if ( sock->channel == INVALID_SOCKET ) {
		SDLNet_SetError("Couldn't create socket");
		goto error_return;
	}


#ifdef macintosh
	status = OTInstallNotifier(sock->channel, YieldingNotifier, nil);
	if ( status != noErr ) {
		SDLNet_SetError("Couldn't install socket OT notifier");
		goto error_return;
	}

	status = OTUseSyncIdleEvents(sock->channel, true);
	if ( status != noErr ) {
		SDLNet_SetError("Couldn't configure socket to issue OT syncIdleEvents");
		goto error_return;
	}
#endif


	/* Connect to remote, or bind locally, as appropriate */
	if ( (ip->host != INADDR_NONE) && (ip->host != INADDR_ANY)
#ifdef macintosh
	&&    (ip->host != OTlocalhost)
#endif
	) {
#ifdef macintosh
		{
		  OSStatus status;
		  TCall sndTCall;
		  InetAddress sndInetAddress;
		  TCall rcvTCall;
		  InetAddress rcvInetAddress;
		  TBind assignedAddressTBind;
		  InetAddress assignedInetAddress;


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
		}
#else 
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
#endif /* macintosh */
		sock->sflag = 0;
	} else {
#ifdef macintosh
		{
#if 0
		  OSStatus status;
//	TBind *wantedAddressTBind = nil;
//	TBind *assignedAddressTBind = nil;


/* Big problem here-- we must call OTListen() before we'll notice any incoming requests, and that means we should be asynch..... */

		  wantedAddressTBind = (TBind *) OTAlloc(sock->channel,T_BIND,T_ADDR,&status);
		  if ( status != noErr || wantedAddressTBind == nil) {
			SDLNet_SetError("Couldn't allocate TBind structure prior to binding to local port");
			goto error_return;
		  }

		  InetAddressPtr = (InetAddress *) wantedAddressTBind->addr.buf;
		  OTInitInetAddress(InetAddressPtr, ip->port, INADDR_ANY);
		  
		  /* For the moment, go with qlen = 1, thought if we allocate the endpoint as a tilisten
		     endpoint, then handling qlen > 1 is trivial */
		  wantedAddressTBind->qlen = 1;

		  assignedAddressTBind = (TBind *) OTAlloc(sock->channel,T_BIND,T_ADDR,&status);
		  if ( status != noErr || assignedAddressTBind == nil) {
			SDLNet_SetError("Couldn't allocate TBind structure prior to binding to local port");
			goto error_return;
		  }

		  status = OTBind(sock->channel, wantedAddressTBind, assignedAddressTBind);
		  if ( status != noErr ) {
			SDLNet_SetError("Couldn't bind to local port");
			goto error_return;
		  }
		  
		  InetAddressPtr = (InetAddress *) assignedAddressTBind->addr.buf;
		  if (InetAddressPtr->fHost == 0) {
		  	InetInterfaceInfo theInetInterfaceInfo;
		  	
			if (OTInetGetInterfaceInfo(&theInetInterfaceInfo,kDefaultInetInterface) == noErr)
			{
				InetAddressPtr->fHost = theInetInterfaceInfo.fAddress;
			}
		  }
		  
		  sock->localAddress.host = InetAddressPtr->fHost;
		  sock->localAddress.port = InetAddressPtr->fPort;
#endif
		}
#else
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

#endif /* macintosh */
		sock->sflag = 1;
	}
	sock->ready = 0;

#ifdef TCP_NODELAY
	/* Set the nodelay TCP option for real-time games */
	{ int yes = 1;
	setsockopt(sock->channel, IPPROTO_TCP, TCP_NODELAY, (char*)&yes, sizeof(yes));
	}
#endif /* TCP_NODELAY */

#ifndef macintosh
	/* Fill in the channel host address */
	sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
	sock->remoteAddress.port = sock_addr.sin_port;
#endif

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
#ifndef macintosh
	struct sockaddr_in sock_addr;
	int sock_alen;
#endif

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
#ifdef macintosh
	{ InetAddress peer;
	  TCall peerinfo;
	  OSStatus status;


/* Hmm-- what about the select() on this puppy?  Should check for incoing connection request, right? */
 
	  memset(&peerinfo, 0, (sizeof peerinfo));
	  peerinfo.addr.buf = (UInt8 *) &peer;
	  peerinfo.addr.maxlen = sizeof(peer);
	  status = OTListen(server->channel, &peerinfo);
	  if ( status != noErr ) {
		SDLNet_SetError("OT: listen() failed");
		goto error_return;
	  }
	  sock->channel = OTOpenEndpoint(OTCreateConfiguration(kTCPName),
							0, nil, &status);
	  if ( sock->channel == INVALID_SOCKET ) {
		SDLNet_SetError("accept() failed");
		goto error_return;
	  }
	  status = OTAccept(server->channel, sock->channel, &peerinfo);
	  if ( status != noErr ) {
		SDLNet_SetError("OT: accept() failed");
		goto error_return;
	  }
	  sock->remoteAddress.host = peer.fHost;
	  sock->remoteAddress.port = peer.fPort;
	}
#else
	sock_alen = sizeof(sock_addr);
	sock->channel = accept(server->channel, (struct sockaddr *)&sock_addr,
								&sock_alen);
	if ( sock->channel == SOCKET_ERROR ) {
		SDLNet_SetError("accept() failed");
		goto error_return;
	}
	sock->remoteAddress.host = sock_addr.sin_addr.s_addr;
	sock->remoteAddress.port = sock_addr.sin_port;
#endif /* macintosh */

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
#ifdef macintosh
		len = OTSnd(sock->channel, data, left, 0);
		
		if (len == kOTFlowErr)
			len = 0;
#else
		len = send(sock->channel, (const char *) data, left, 0);
#endif
		if ( len > 0 ) {
			sent += len;
			left -= len;
			data += len;
		}
#ifdef macintosh
	} while ( (left > 0) && (len > 0) );
#else
	} while ( (left > 0) && ((len > 0) || (errno == EINTR)) );
#endif

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

#ifdef macintosh
	len = OTRcv(sock->channel, data, maxlen, 0);
	
	if (len == kOTNoDataErr) 
		len = 0;
#else
	errno = 0;
	do {
		len = recv(sock->channel, (char *) data, maxlen, 0);
	} while ( errno == EINTR );
#endif /* macintosh */

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
