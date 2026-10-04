// Thunks for the names under which d_scene.cpp calls SoundResource
// (src/news/Resource.cpp).
// See <pc/thunk.h> and docs/pc_port.md, rule R7.

#include <news/Resource.h>
#include <pc/thunk.h>

// The scene's sound archive (d_scene.cpp).
extern SoundResource* gSoundPlayer;

// SoundResource::~SoundResource(); d_scene.cpp declares the object as void*.
extern "C" void __dt__13SoundResourceFv(void* sound, s32 flags) {
    SoundResource* self = static_cast<SoundResource*>(sound);
    if (self != NULL) {
        if (flags > 0) {
            delete self;
        } else {
            self->~SoundResource();
        }
    }
}

// SoundResource::Calc() and SoundResource::Update(). d_scene.cpp calls both
// without an object: neither function uses `this`, and the original leaves r3
// as it is. Both calls come after a check of gSoundPlayer != NULL, so the
// thunks call them on that object.
extern "C" void Calc__13SoundResourceFv(void) {
    gSoundPlayer->Calc();
}

extern "C" void Update__13SoundResourceFv(void) {
    gSoundPlayer->Update();
}
