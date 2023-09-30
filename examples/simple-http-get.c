/*
 * This is just for demonstration purposes! A real HTTP solution would
 * be WAY more complicated, support HTTPS, cookies, etc. Use curl or
 * wget for real stuff, not this.
 *
 * All this to say: don't use this for anything serious!
 */

#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_net/SDL_net.h>

int main(int argc, char **argv)
{
    if (SDLNet_Init() < 0) {
        SDL_Log("SDLNet_Init() failed: %s", SDL_GetError());
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        SDL_Log("Looking up %s ...", argv[i]);
        SDLNet_Address *addr = SDLNet_ResolveHostname(argv[i]);
        if (SDLNet_WaitUntilResolved(addr, -1) == -1) {
            SDL_Log("Failed to lookup %s: %s", argv[i], SDL_GetError());
        } else {
            SDL_Log("%s is %s", argv[i], SDLNet_GetAddressString(addr));
            char *req = NULL;
            SDL_asprintf(&req, "GET / HTTP/1.0\r\nHost: %s\r\n\r\n", argv[i]);
            SDLNet_StreamSocket *sock = req ? SDLNet_CreateClient(addr, 80) : NULL;
            if (!req) {
                SDL_Log("Out of memory!");
            } else if (!sock) {
                SDL_Log("Failed to create stream socket to %s: %s\n", argv[i], SDL_GetError());
            } else if (SDLNet_WaitUntilConnected(sock, -1) < 0) {
                SDL_Log("Failed to connect to %s: %s", argv[i], SDL_GetError());
            } else if (SDLNet_WriteToStreamSocket(sock, req, SDL_strlen(req)) < 0) {
                SDL_Log("Failed to write to %s: %s", argv[i], SDL_GetError());
            } else if (SDLNet_WaitUntilStreamSocketDrained(sock, -1) < 0) {
                SDL_Log("Failed to finish write to %s: %s", argv[i], SDL_GetError());
            } else {
                char buf[512];
                int br;
                while ((br = SDLNet_ReadFromStreamSocket(sock, buf, sizeof (buf))) >= 0) {
                    fwrite(buf, 1, br, stdout);
                }

                printf("\n\n\n%s\n\n\n", SDL_GetError());
                fflush(stdout);
            }

            if (sock) {
                SDLNet_DestroyStreamSocket(sock);
            }

            SDL_free(req);
        }
    }


    SDLNet_Quit();

    return 0;
}

