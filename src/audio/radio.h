// The car radio's stream: one station (data/sound/radio/<n>CH.adp, from SOUND/<n>CH_L/R.VAG) played from a point and
// mixed into the synth's output. SNDMOD plays the stations as two SPU2 voices a side, a chunk of 30 seconds at a
// time (load_radio, procRadio); this plays the same samples straight through. See docs/sound.md.
//
// Two kinds of file, either read through a small mapped window that moves along with the playback, so an hour of
// sound costs neither memory nor address space:
//   - A radio file (.adp, from sndconv or tools/adpconv; the layout is in audio/adpcm.h): the disc's PS-ADPCM,
//     decoded here, which gives the same samples as decoding the VAGs. A stream decodes as one sample from its
//     start, so playing from a point decodes from the seek point before it.
//   - A 16-bit PCM WAV, mono or stereo, at any rate (a replacement station, say).
#pragma once

#include <windows.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "synth.h"

namespace audio {

class RadioStream : public SynthStream {
public:
    RadioStream() = default;
    ~RadioStream() override { Close(); }

    // Opens `path`, a radio file or a WAV (closing what was open). False if it's missing or neither.
    bool Open(const std::string& path);
    void Close();
    bool IsOpen() const { return data_ != 0; }

    // Plays from frame `frame` (taken modulo `loop`, the frames it wraps after), both counted at `rate` and scaled
    // to the file's own rate; a file shorter than `loop` wraps at its end.
    void Start(int64_t frame, int64_t loop, int rate);
    void Stop() { playing_ = false; }
    void SetLevels(float left, float right) {
        left_ = left;
        right_ = right;
    }
    // Reads the file's pages around the playback point, so the audio thread doesn't wait on the disk for them.
    void Prefetch();

    void Mix(float* mix, int frames, int rate) override;

private:
    bool OpenWav(int64_t fileSize);
    bool OpenAdpcm(int64_t fileSize);
    // Sample frame `i` (wrapped) as left and right.
    void Frame(int64_t i, float* l, float* r);
    // The decoded segment (seekBlocks_ blocks from a seek point) holding sample frame `i`, or null.
    const int16_t* Segment(int64_t i);
    int64_t ByteOf(int64_t frame) const;
    const uint8_t* Window(int64_t byte, int size);

    HANDLE file_ = INVALID_HANDLE_VALUE, mapping_ = 0;
    int64_t data_ = 0, dataSize_ = 0;   // the samples' (or blocks') place in the file
    int channels_ = 0, rate_ = 0;
    bool adpcm_ = false;
    int64_t frames_ = 0, loop_ = 0;
    const uint8_t* view_ = nullptr;
    int64_t viewStart_ = 0, viewSize_ = 0;
    bool playing_ = false;
    int64_t pos_ = 0;      // the frame playing
    double frac_ = 0;      // and how far past it
    float left_ = 1, right_ = 1;

    // A radio file's seek points and two decoded segments (the interpolation reaches across a segment's end).
    int seekBlocks_ = 0;
    int64_t blocks_ = 0;
    std::vector<int16_t> seeks_;   // [seek][channel]: s1, s2
    struct Decoded {
        int64_t segment = -1;
        std::vector<int16_t> pcm;   // interleaved
    } decoded_[2];
    int lastDecoded_ = 0;
    const int16_t* segPcm_ = nullptr;   // the segment Frame read last: frames segStart_ to segStart_ + segFrames_
    int64_t segStart_ = 0, segFrames_ = 0;
};

}  // namespace audio
