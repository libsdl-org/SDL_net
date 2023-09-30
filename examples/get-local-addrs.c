/*
 * This is just for demonstration purposes! This doesn't
 * do anything as complicated as, say, the `ifconfig` utility.
 *
 * All this to say: don't use this for anything serious!
 */

#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_net/SDL_net.h>

int main(int argc, char **argv)
{
    SDLNet_Address **addrs = NULL;
    int num_addrs = 0;
    int i;

    (void)argc;
    (void)argv;

    if (SDLNet_Init() < 0) {
        SDL_Log("SDLNet_Init() failed: %s", SDL_GetError());
        return 1;
    }

    addrs = SDLNet_GetLocalAddresses(&num_addrs);
    if (addrs == NULL) {
        SDL_Log("Failed to determine local addresses: %s", SDL_GetError());
        SDLNet_Quit();
        return 1;
    }

    SDL_Log("We saw %d local addresses:", num_addrs);
    for (i = 0; i < num_addrs; i++) {
        SDL_Log("  - %s", SDLNet_GetAddressString(addrs[i]));
    }

    SDLNet_FreeLocalAddresses(addrs);
    SDLNet_Quit();

    return 0;
}
