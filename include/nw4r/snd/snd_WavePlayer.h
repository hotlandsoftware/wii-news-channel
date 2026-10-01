#ifndef NW4R_SND_WAVE_PLAYER_H
#define NW4R_SND_WAVE_PLAYER_H
#include <nw4r/types_nw4r.h>

#include <nw4r/snd/snd_Types.h>

#include <nw4r/snd/snd_ut.h>

namespace nw4r {
namespace snd {

// Added for snd part 3 (Task 13): present in this NW4R revision
// (cf. TP's nw4hbm WavePlayer)
class WavePlayer {
public:
    static void detail_UpdateAllPlayers();
    static void detail_UpdateBufferAllPlayers();
    static void detail_StopAllPlayers();
};

} // namespace snd
} // namespace nw4r

#endif
