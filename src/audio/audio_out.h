// Audio output: a thread that pulls 16-bit stereo frames from a callback and streams them to the sound device.
// XAudio2 2.7 (DirectX June 2010 runtime) is the only backend so far; a DirectSound one can be added behind the
// same interface for machines without the DirectX runtime.
#pragma once

#include <stdint.h>

namespace audio {

class AudioOut {
public:
    // Called on the audio thread to fill `frames` stereo frames.
    typedef void (*Fill)(void* user, int16_t* out, int frames);

    virtual ~AudioOut() {}
    // Starts the thread and the device. Returns false (with nothing running) if the device can't be opened.
    virtual bool Start(int rate, Fill fill, void* user) = 0;
    virtual void Stop() = 0;
};

AudioOut* CreateXAudio2Out();

}  // namespace audio
