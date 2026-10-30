// 16-bit PCM WAV files in memory, for the tools (sndconv, adpconv). The game streams its WAVs itself (audio/radio).
#pragma once

#include <stdint.h>

#include <vector>

namespace audio {

// A WAV of interleaved 16-bit samples, `channels` (1 or 2) at `rate`.
std::vector<uint8_t> Wav_Build(const std::vector<int16_t>& pcm, int channels, int rate);

// A WAV's samples, interleaved, if it is 16-bit PCM, mono or stereo. False otherwise.
bool Wav_Parse(const std::vector<uint8_t>& file, std::vector<int16_t>& pcm, int& channels, int& rate);

}  // namespace audio
