# Makefile.am for the SDL sample image loading library and viewer

lib_LTLIBRARIES = libSDL_net.la

libSDL_netincludedir = $(includedir)/SDL
libSDL_netinclude_HEADERS =		\
	SDL_net.h

libSDL_net_la_SOURCES =		\
	SDLnet.c		\
	SDLnetTCP.c		\
	SDLnetUDP.c		\
	SDLnetselect.c		\
	SDLnetsys.h

EXTRA_DIST =			\
	CHANGES			\
	autogen.sh		\
	VisualC.zip		\
	MPWmake.sea.bin		\
	quit.bmp		\
	scroll_dn.bmp		\
	scroll_up.bmp		\
	SDL_net.spec.in

libSDL_net_la_LIBADD = @INETLIB@
libSDL_net_la_LDFLAGS = 		\
        -release $(LT_RELEASE)	\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

if HAVE_GUI_LIB
EXAMPLES = chat chatd
else
EXAMPLES =
endif
noinst_PROGRAMS = $(EXAMPLES)

chat_SOURCES = chat.cpp chat.h
chat_LDADD = libSDL_net.la -lGUI
chatd_SOURCES = chatd.c
chatd_LDADD = libSDL_net.la
