// PC port: tracing of nw4r::snd for NEWSCHANNEL_AX_LOG (docs/pc_port.md,
// "Audio"). Called from nw4r::snd under TARGET_PC; does nothing unless the log
// is switched on. Implemented in src/pc/audio/snd_trace.cpp.

#ifndef PC_SND_TRACE_H
#define PC_SND_TRACE_H

#include <types.h>

namespace nw4r {
namespace snd {
class SoundStartable;
}
} // namespace nw4r

// A sound was started (or refused): `result` is a SoundStartable::StartResult.
void PCSndTraceStartSound(const nw4r::snd::SoundStartable* startable, u32 id, int result);

#endif
