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
*/

/* Include normal system headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef macintosh
#ifndef USE_GUSI_SOCKETS
#define MACOS_OPENTRANSPORT
#error Open Transport driver is broken
#endif
#endif /* macintosh */

/* Include system network headers */
#ifdef MACOS_OPENTRANSPORT
#include <OpenTransport.h>
#include <OpenTptInternet.h>
#else
#if defined(__WIN32__) || defined(WIN32)
#define Win32_Winsock
#include <windows.h>
#else /* UNIX */
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#ifndef __BEOS__
#include <arpa/inet.h>
#endif
#include <netinet/in.h>
#ifdef linux /* FIXME: what other platforms have this? */
#include <netinet/tcp.h>
#endif
#include <netdb.h>
#include <sys/socket.h>
#endif /* WIN32 */
#endif /* Open Transport */

/* System-dependent definitions */
#ifdef MACOS_OPENTRANSPORT
#define closesocket	OTCloseProvider
#define SOCKET		EndpointRef
#define INVALID_SOCKET	kOTInvalidEndpointRef
#else
#ifndef Win32_Winsock
#define closesocket	close
#define SOCKET	int
#define INVALID_SOCKET	-1
#define SOCKET_ERROR	-1
#endif /* Win32_Winsock */
#endif /* Open Transport */
