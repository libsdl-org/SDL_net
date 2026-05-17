/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* this creates a stream socket server, waits for connections, then
   echoes back anything they send. Connect to the server with telnet
   or netcat on port 7241 and type things! */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_net/SDL_net.h>

int main(int argc, char **argv)
{
    const int SERVER_PORT = 7241;

    if (!NET_Init()) {
        SDL_Log("NET_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    NET_Server *server = NET_CreateServer(NULL, SERVER_PORT, 0);
    if (!server) {
        SDL_Log("Failed to create server: %s", SDL_GetError());
    } else {
        SDL_Log("Server is ready! Connect to port %d and send text!", (int) SERVER_PORT);
        int num_vsockets = 1;
        void *vsockets[128];
        SDL_zeroa(vsockets);
        vsockets[0] = server;
        while (NET_WaitUntilInputAvailable(vsockets, num_vsockets, -1) >= 0) {
            NET_StreamSocket *streamsocket = NULL;
            if (!NET_AcceptClient(server, &streamsocket)) {
                SDL_Log("NET_AcceptClient failed: %s", SDL_GetError());
                break;
            } else if (streamsocket) { // new connection!
                SDL_Log("New connection from %s!", NET_GetAddressString(NET_GetStreamSocketAddress(streamsocket)));
                if (num_vsockets >= (int) (SDL_arraysize(vsockets) - 1)) {
                    SDL_Log("  (too many connections, though, so dropping immediately.)");
                    NET_DestroyStreamSocket(streamsocket);
                } else {
                    vsockets[num_vsockets++] = streamsocket;
                }
            }

            // see if anything has new stuff.
            char buffer[1024];
            for (int i = 1; i < num_vsockets; i++) {
                bool kill_socket = false;
                streamsocket = (NET_StreamSocket *) vsockets[i];
                const int br = NET_ReadFromStreamSocket(streamsocket, buffer, sizeof (buffer));
                if (br < 0) {  // uhoh, socket failed!
                    kill_socket = true;
                } else if (br > 0) {
                    const char *addrstr = NET_GetAddressString(NET_GetStreamSocketAddress(streamsocket));
                    SDL_Log("Got %d more bytes from '%s'", br, addrstr);
                    if (!NET_WriteToStreamSocket(streamsocket, buffer, br)) {
                        SDL_Log("Failed to echo data back to '%s': %s", addrstr, SDL_GetError());
                        kill_socket = true;
                    }
                }

                if (kill_socket) {
                    SDL_Log("Dropping connection to '%s'", NET_GetAddressString(NET_GetStreamSocketAddress(streamsocket)));
                    NET_DestroyStreamSocket(streamsocket);
                    vsockets[i] = NULL;
                    if (i < (num_vsockets - 1)) {
                        SDL_memmove(&vsockets[i], &vsockets[i+1], sizeof (vsockets[0]) * ((num_vsockets - i) - 1));
                    }
                    num_vsockets--;
                    i--;
                }
            }
        }

        SDL_Log("Destroying server...");
        NET_DestroyServer(server);
    }

    SDL_Log("Shutting down...");
    NET_Quit();
    SDL_Quit();
    return 0;
}

