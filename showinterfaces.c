
#include "SDL_net.h"

#define MAX_ADDRESSES	10

int main(int argc, char *argv[])
{
	IPaddress addresses[MAX_ADDRESSES];
	int i, count;

	count = SDLNet_GetLocalAddresses(addresses, MAX_ADDRESSES);
	printf("Found %d local addresses\n", count);
	for ( i = 0; i < count; ++i ) {
		printf("%d: %s\n", i+1, SDLNet_ResolveIP(&addresses[i]));
	}
	return 0;
}
