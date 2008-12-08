/*
    CHAT:  A chat client/server using the SDL example network library
    Copyright (C) 1997-2009 Sam Lantinga

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
    slouken@libsdl.org
*/

/* $Id$ */

/* Convert four letters into a number */
#define MAKE_NUM(A, B, C, D)	(((A+B)<<8)|(C+D))

/* Defines for the chat client */
#define CHAT_SCROLLBACK	512		/* Save 512 lines in scrollback */
#define CHAT_PROMPT	"> "
#define CHAT_PACKETSIZE	256		/* Maximum length of a message */

/* Defines shared between the server and client */
#define CHAT_PORT	MAKE_NUM('C','H','A','T')

/* The protocol between the chat client and server */
#define CHAT_HELLO	0	/* 0+Port+len+name */
#define CHAT_HELLO_PORT		1
#define CHAT_HELLO_NLEN		CHAT_HELLO_PORT+2
#define CHAT_HELLO_NAME		CHAT_HELLO_NLEN+1
#define CHAT_ADD	1	/* 1+N+IP+Port+len+name */
#define CHAT_ADD_SLOT		1
#define CHAT_ADD_HOST		CHAT_ADD_SLOT+1
#define CHAT_ADD_PORT		CHAT_ADD_HOST+4
#define CHAT_ADD_NLEN		CHAT_ADD_PORT+2
#define CHAT_ADD_NAME		CHAT_ADD_NLEN+1
#define CHAT_DEL	2	/* 2+N */
#define CHAT_DEL_SLOT		1
#define CHAT_DEL_LEN		CHAT_DEL_SLOT+1
#define CHAT_BYE	255	/* 255 */
#define CHAT_BYE_LEN		1

/* The maximum number of people who can talk at once */
#define CHAT_MAXPEOPLE	10
