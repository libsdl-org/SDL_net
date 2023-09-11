/*
 * This is just for demonstration purposes! This doesn't
 * do anything as complicated as, say, the `dig` utility.
 *
 * All this to say: don't use this for anything serious!
 */

#include <stdio.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "SDL_net.h"

int main(int argc, char **argv)
{
    if (SDLNet_Init() < 0) {
        SDL_Log("SDLNet_Init() failed: %s", SDL_GetError());
        return 1;
    }

    //SDLNet_SimulateAddressResolutionLoss(3000, 30);

    SDLNet_Address **addrs = (SDLNet_Address **) SDL_calloc(argc, sizeof (SDLNet_Address *));
    for (int i = 1; i < argc; i++) {
        addrs[i] = SDLNet_ResolveHostname(argv[i]);
    }

    for (int i = 1; i < argc; i++) {
        const char *str;
        SDLNet_WaitUntilResolved(addrs[i], -1);
        str = SDLNet_GetAddressString(addrs[i]);
        SDL_Log("%s: %s", argv[i], str ? str : "[FAILED TO RESOLVE]");
        SDLNet_UnrefAddress(addrs[i]);
    }

    SDLNet_Quit();

    return 0;
}
