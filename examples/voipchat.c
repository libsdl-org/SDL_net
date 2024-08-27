#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_net/SDL_net.h>

/*
 * This is just for demonstration purposes! A real VoIP solution would
 * definitely compress audio with a speech codec of some sort, it would
 * deal with packet loss better, it would have encryption, NAT punching,
 * and it wouldn't just allow _anyone_ to talk to you without some sort
 * of authorization step.
 *
 * All this to say: don't use this for anything serious!
 */

typedef struct Voice
{
    SDL_AudioStream *stream;
    SDLNet_Address *addr;
    Uint16 port;
    Uint64 idnum;
    Uint64 last_seen;
    Uint64 last_packetnum;
    struct Voice *prev;
    struct Voice *next;
} Voice;

static SDLNet_DatagramSocket *sock = NULL;  /* you talk over this, client or server. */
static SDLNet_Address *server_addr = NULL;  /* address of the server you're talking to, NULL if you _are_ the server. */
static Uint16 server_port = 3025;
static int max_datagram = 0;
static Voice *voices = NULL;
static Uint64 next_idnum = 0;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_AudioDeviceID audio_device = 0;
static SDL_AudioDeviceID capture_device = 0;
static SDL_AudioStream *capture_stream = NULL;
static const SDL_AudioSpec audio_spec = { SDL_AUDIO_S16LE, 1, 8000 };
static Uint8 scratch_area[4096];

static Voice *FindVoiceByAddr(const SDLNet_Address *addr, const Uint16 port)
{
    Voice *i;
    for (i = voices; i != NULL; i = i->next) {
        if ((i->port == port) && (SDLNet_CompareAddresses(i->addr, addr) == 0)) {
            return i;
        }
    }
    return NULL;
}

static Voice *FindVoiceByIdNum(const Uint64 idnum)
{
    Voice *i;
    for (i = voices; i != NULL; i = i->next) {
        if (i->idnum == idnum) {
            return i;
        }
    }
    return NULL;
}

static void ClearOldVoices(const Uint64 now)
{
    Voice *i;
    Voice *next;
    for (i = voices; i != NULL; i = next) {
        next = i->next;
        if (!now || ((now - i->last_seen) > 60000)) {  /* nothing new in 60+ seconds? (or shutting down?) */
            if (!i->stream || (SDL_GetAudioStreamAvailable(i->stream) == 0)) {  /* they'll get a reprieve if data is still playing out */
                SDL_Log("Destroying voice #%" SDL_PRIu64, i->idnum);
                SDL_DestroyAudioStream(i->stream);
                SDLNet_UnrefAddress(i->addr);
                if (i->prev) {
                    i->prev->next = next;
                } else {
                    voices = next;
                }
                if (next) {
                    next->prev = i->prev;
                }
                SDL_free(i);
            }
        }
    }
}

static const int extra = (int) (sizeof (Uint64) * 2);
static void SendClientAudioToServer(void)
{
    const int br = SDL_GetAudioStreamData(capture_stream, scratch_area + extra, max_datagram - extra);
    if (br > 0) {
        ((Uint64 *) scratch_area)[0] = SDL_Swap64LE(0);  /* just being nice and leaving space in the buffer for the server to replace. */
        ((Uint64 *) scratch_area)[1] = SDL_Swap64LE(++next_idnum);
        SDL_Log("CLIENT: Sending %d new bytes to server at %s:%d...", br + extra, SDLNet_GetAddressString(server_addr), (int) server_port);
        SDLNet_SendDatagram(sock, server_addr, server_port, scratch_area, br + extra);
    }
}

static void mainloop(void)
{
    const SDL_bool is_client = (server_addr != NULL) ? SDL_TRUE : SDL_FALSE;
    SDL_bool done = SDL_FALSE;
    Uint64 last_send_ticks = 0;

    if (is_client) {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  /* red by default (green when recording) */
    }

    while (!done) {
        SDL_bool activity = SDL_FALSE;
        const Uint64 now = SDL_GetTicks();
        SDL_Event event;
        SDLNet_Datagram *dgram = NULL;
        int rc;

        while (((rc = SDLNet_ReceiveDatagram(sock, &dgram)) == 0) && (dgram != NULL)) {
            SDL_Log("%s: got %d-byte datagram from %s:%d", is_client ? "CLIENT" : "SERVER", (int) dgram->buflen, SDLNet_GetAddressString(dgram->addr), (int) dgram->port);
            activity = SDL_TRUE;
            if (!is_client) {  /* we're the server? */
                Voice *voice = FindVoiceByAddr(dgram->addr, dgram->port);
                Voice *i;

                if (!voice) {
                    SDL_Log("SERVER: Creating voice idnum=%" SDL_PRIu64 " from %s:%d", next_idnum + 1, SDLNet_GetAddressString(dgram->addr), (int) dgram->port);
                    voice = (Voice *) SDL_calloc(1, sizeof (Voice));
                    voice->addr = SDLNet_RefAddress(dgram->addr);
                    voice->port = dgram->port;
                    voice->idnum = ++next_idnum;
                    if (voices) {
                        voice->next = voices;
                        voices->prev = voice;
                    }
                    voices = voice;
                }

                voice->last_seen = now;

                /* send this new voice data to all recent speakers. */
                if (dgram->buflen > extra) {  /* ignore it if too small, might just be a keepalive packet. */
                    *((Uint64 *) dgram->buf) = SDL_Swap64LE(voice->idnum);  /* the client leaves space to fill this in for convenience. */
                    for (i = voices; i != NULL; i = i->next) {
                        if ((voice->port != i->port) || (SDLNet_CompareAddresses(voice->addr, i->addr) != 0)) {  /* don't send client's own voice back to them. */
                            SDL_Log("SERVER: sending %d-byte datagram to %s:%d", (int) dgram->buflen, SDLNet_GetAddressString(i->addr), (int) i->port);
                            SDLNet_SendDatagram(sock, i->addr, i->port, dgram->buf, dgram->buflen);
                        }
                    }
                }
            } else {  /* we're the client. */
                if ((dgram->port != server_port) || (SDLNet_CompareAddresses(dgram->addr, server_addr) != 0)) {
                    SDL_Log("CLIENT: Got packet from non-server address %s:%d. Ignoring.", SDLNet_GetAddressString(dgram->addr), (int) dgram->port);
                } else if (dgram->buflen < extra) {
                    SDL_Log("CLIENT: Got bogus packet from the server. Ignoring.");
                } else {
                    const Uint64 idnum = SDL_Swap64LE(((const Uint64 *) dgram->buf)[0]);
                    const Uint64 packetnum = SDL_Swap64LE(((const Uint64 *) dgram->buf)[1]);
                    Voice *voice = FindVoiceByIdNum(idnum);
                    if (!voice) {
                        SDL_Log("CLIENT: Creating voice idnum=#%" SDL_PRIu64, idnum);
                        voice = (Voice *) SDL_calloc(1, sizeof (Voice));
                        if (audio_device) {
                            voice->stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
                            if (voice->stream) {
                                SDL_BindAudioStream(audio_device, voice->stream);
                            }
                        }
                        voice->idnum = idnum;
                        if (voices) {
                            voice->next = voices;
                            voices->prev = voice;
                        }
                        voices = voice;
                    }

                    voice->last_seen = now;

                    if (packetnum > voice->last_packetnum) {   /* if packet arrived out of order, don't queue it for playing. */
                        voice->last_packetnum = packetnum;
                        SDL_PutAudioStreamData(voice->stream, dgram->buf + extra, dgram->buflen - extra);
                        SDL_FlushAudioStream(voice->stream);  /* flush right away so we have all data if the stream dries up. We can still safely add more data later. */
                    }
                }
            }

            SDLNet_DestroyDatagram(dgram);
        }

        while (SDL_PollEvent(&event)) {
            activity = SDL_TRUE;
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    done = 1;
                    break;

                case SDL_EVENT_MOUSE_BUTTON_DOWN:
                    if (is_client && capture_stream && (event.button.button == SDL_BUTTON_LEFT)) {
                        if (SDL_BindAudioStream(capture_device, capture_stream)) {
                            SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);  /* green when recording */
                        }
                    }
                    break;

                case SDL_EVENT_MOUSE_BUTTON_UP:
                    if (is_client && capture_stream && (event.button.button == SDL_BUTTON_LEFT)) {
                        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);  /* red when not recording */
                        SDL_UnbindAudioStream(capture_stream);
                        SDL_FlushAudioStream(capture_stream);
                        while (SDL_GetAudioStreamAvailable(capture_stream) > 0) {
                            SendClientAudioToServer();
                            last_send_ticks = now;
                        }
                    }
                    break;
            }
        }

        if (is_client) {
            if (capture_stream) {
                while (SDL_GetAudioStreamAvailable(capture_stream) > max_datagram) {
                    SendClientAudioToServer();
                    last_send_ticks = now;
                    activity = SDL_TRUE;
                }
            }

            if (!last_send_ticks || ((now - last_send_ticks) > 5000)) {  /* send a keepalive packet if we haven't transmitted for a bit. */
                ((Uint64 *) scratch_area)[0] = SDL_Swap64LE(0);
                ((Uint64 *) scratch_area)[1] = SDL_Swap64LE(++next_idnum);
                SDL_Log("CLIENT: Sending %d keepalive bytes to server at %s:%d...", extra, SDLNet_GetAddressString(server_addr), (int) server_port);
                SDLNet_SendDatagram(sock, server_addr, server_port, scratch_area, extra);
                last_send_ticks = now;
            }
        }

        ClearOldVoices(now);

        if (!activity) {
            SDL_Delay(10);
        }

        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
    }
}

static void run_voipchat(int argc, char **argv)
{
    const char *hostname = NULL;
    SDL_bool is_server = SDL_FALSE;
    int simulate_failure = 0;
    int i;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (SDL_strcmp(arg, "--server") == 0) {
            is_server = SDL_TRUE;
        } else if ((SDL_strcmp(arg, "--port") == 0) && (i < (argc-1))) {
            server_port = (Uint16) SDL_atoi(argv[++i]);
        } else if ((SDL_strcmp(arg, "--simulate-failure") == 0) && (i < (argc-1))) {
            simulate_failure = (int) SDL_atoi(argv[++i]);
        } else {
            hostname = arg;
        }
    }

    simulate_failure = SDL_min(SDL_max(simulate_failure, 0), 100);

    if (simulate_failure) {
        SDL_Log("Simulating failure at %d percent", simulate_failure);
    }

    if (is_server && hostname) {
        SDL_Log("WARNING: Specified --server and a hostname, ignoring the hostname");
    } else if (!is_server && !hostname) {
        SDL_Log("USAGE: %s <--server|hostname> [--port X] [--simulate-failure Y]", argv[0]);
        return;
    }

    if (is_server) {
        SDL_Log("SERVER: Listening on port %d", server_port);
    } else {
        SDL_Log("CLIENT: Resolving server hostname '%s' ...", hostname);
        server_addr = SDLNet_ResolveHostname(hostname);
        if (server_addr) {
            if (SDLNet_WaitUntilResolved(server_addr, -1) < 0) {
                SDLNet_UnrefAddress(server_addr);
                server_addr = NULL;
            }
        }

        if (!server_addr) {
            SDL_Log("CLIENT: Failed! %s", SDL_GetError());
            SDL_Log("CLIENT: Giving up.");
            return;
        }

        SDL_Log("CLIENT: Server is at %s:%d.", SDLNet_GetAddressString(server_addr), (int) server_port);

        audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec);
        if (!audio_device) {
            SDL_Log("CLIENT: Failed to open output audio device (%s), going on without sound playback!", SDL_GetError());
        }

        capture_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_RECORDING, &audio_spec);
        if (!capture_device) {
            SDL_Log("CLIENT: Failed to open capture audio device (%s), going on without sound recording!", SDL_GetError());
        } else {
            capture_stream = SDL_CreateAudioStream(&audio_spec, &audio_spec);
            if (!capture_stream) {
                SDL_Log("CLIENT: Failed to create capture audio stream (%s), going on without sound recording!", SDL_GetError());
                SDL_CloseAudioDevice(capture_device);
                capture_device = 0;
            }
        }
    }

    /* server _must_ be on the requested port. Clients can take anything available, server will respond to where it sees it come from. */
    sock = SDLNet_CreateDatagramSocket(NULL, is_server ? server_port : 0);
    if (!sock) {
        SDL_Log("Failed to create datagram socket: %s", SDL_GetError());
    } else {
        if (simulate_failure) {
            SDLNet_SimulateDatagramPacketLoss(sock, simulate_failure);
        }
        mainloop();
    }

    SDL_Log("Shutting down...");

    ClearOldVoices(0);

    SDL_DestroyAudioStream(capture_stream);
    SDL_CloseAudioDevice(audio_device);
    SDL_CloseAudioDevice(capture_device);
    audio_device = capture_device = 0;

    SDLNet_UnrefAddress(server_addr);
    server_addr = NULL;
    SDLNet_DestroyDatagramSocket(sock);
    sock = NULL;
}


int main(int argc, char **argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (SDLNet_Init() < 0) {
        SDL_Log("SDLNet_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    window = SDL_CreateWindow("SDL_Net3 voipchat example", 640, 480, 0);
    renderer = SDL_CreateRenderer(window, NULL);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

    max_datagram = SDL_min(1200, (int) sizeof (scratch_area));

    run_voipchat(argc, argv);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDLNet_Quit();
    SDL_Quit();
    return 0;
}
