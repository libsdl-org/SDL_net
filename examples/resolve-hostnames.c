/*
 * This is just for demonstration purposes! This doesn't
 * do anything as complicated as, say, the `dig` utility.
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

    //SDLNet_SimulateAddressResolutionLoss(3000, 30);

    SDLNet_Address **addrs = (SDLNet_Address **) SDL_calloc(argc, sizeof (SDLNet_Address *));
    for (int i = 1; i < argc; i++) {
        addrs[i] = SDLNet_ResolveHostname(argv[i]);
    }

    for (int i = 1; i < argc; i++) {
        SDLNet_WaitUntilResolved(addrs[i], -1);

        if (SDLNet_GetAddressStatus(addrs[i]) == -1) {
            SDL_Log("%s: [FAILED TO RESOLVE: %s]", argv[i], SDL_GetError());
        } else {
            SDL_Log("%s: %s", argv[i], SDLNet_GetAddressString(addrs[i]));
        }

        SDLNet_UnrefAddress(addrs[i]);
    }

    SDLNet_Quit();

    return 0;
}
