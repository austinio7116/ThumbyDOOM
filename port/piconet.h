/* ThumbyDOOM — empty piconet stub. We don't use multiplayer.
 * Provides the symbols vendor sources expect under USE_PICO_NET. */
#ifndef _PICONET_H
#define _PICONET_H

#include "doomtype.h"
#include "net_defs.h"

extern boolean net_client_connected;
extern char player_name[MAXPLAYERNAME];

typedef struct {
    uint32_t client_id;
    char name[MAXPLAYERNAME];
} lobby_player_t;

typedef enum {
    lobby_no_connection,
    lobby_waiting_for_start,
    lobby_game_started,
    lobby_game_not_compatible,
} piconet_lobby_status_t;

typedef struct {
    uint32_t compat_hash;
    uint32_t seq;
    piconet_lobby_status_t status;
    uint8_t nplayers;
    int8_t deathmatch;
    int8_t epi;
    int8_t skill;
    lobby_player_t players[NET_MAXPLAYERS];
} lobby_state_t;

static inline void piconet_start_host(int8_t dm, int8_t epi, int8_t sk) { }
static inline void piconet_start_client(void) { }
static inline void piconet_start_game(void) { }
static inline int  piconet_get_lobby_state(lobby_state_t *s) { return 0; }

static inline void piconet_init(void) { }
static inline void piconet_stop(void) { }
static inline boolean piconet_client_check_for_dropped_connection(void) { return false; }
static inline void piconet_new_local_tic(int tic) { }
static inline int piconet_maybe_recv_tic(int fromtic) { return -1; }

#endif
