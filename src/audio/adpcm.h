// PS-ADPCM, the SPU2's sample format, and the radio file (.adp) built from it. The game decodes the radio stations
// with it (audio/radio.cpp); sndconv writes the stations' files and tools/adpconv encodes and decodes them for mods.
//
// PS-ADPCM: 16-byte blocks of 28 samples. Byte 0 is the filter (high nibble, 0-4) and shift (low nibble, 0-12),
// byte 1 the flags (loop start, loop, end: the radio ignores them), then 14 bytes of signed 4-bit values, the first
// sample in the low nibble. A sample is (value << 12 >> shift) plus a prediction from the two before it, so a stream
// decodes as one piece from its start.
//
// The radio file, little-endian:
//   0x00 "RADP", u32 version (1), u32 rate, u32 channels (1 or 2), u32 blocks (a side), u32 seekBlocks,
//        u32 seekCount, u32 dataOffset
//   0x20 seek points [seekCount][channels]: s16 s1, s16 s2, the two samples before block k x seekBlocks
//   dataOffset: the blocks, the sides' block b one after the other
// The seek points let a player start anywhere by decoding from the one before.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace audio {

const int kAdpcmBlockSamples = 28;
const int kAdpcmBlockBytes = 16;
const int kRadioHeaderSize = 0x20;
const int kRadioSeekBlocks = 512;   // between seek points: about 1.2 s at the stations' 12 kHz

// One block's 28 samples, given and updating the two samples before it. Writes every `stride`th sample of `out`.
void Adpcm_DecodeBlock(const uint8_t* block, int& s1, int& s2, int16_t* out, int stride = 1);

// Encodes 28 samples (every `stride`th of `in`) as one block: every filter and shift is tried, each decoded as
// Adpcm_DecodeBlock does (so the error doesn't build up from block to block), and the closest kept. Updates s1/s2
// to the block's decoded samples, as a decoder will have them.
void Adpcm_EncodeBlock(const int16_t* in, int stride, int& s1, int& s2, uint8_t* block);

struct RadioHeader {
    int rate = 0, channels = 0, seekBlocks = 0;
    int64_t blocks = 0;
    uint32_t seekCount = 0, dataOffset = 0;
};

// Reads a radio file's header (its first kRadioHeaderSize bytes), checked against the file's size. False if it
// isn't one.
bool Radio_ParseHeader(const uint8_t* p, int64_t fileSize, RadioHeader& h);

// A whole radio file for the sides' blocks (equal lengths, 16 bytes a block), with its seek points.
std::vector<uint8_t> Radio_BuildFile(const std::vector<std::vector<uint8_t>>& sides, int rate);

// Interleaved 16-bit samples (1 or 2 channels) as a side's blocks each (the last block padded with silence);
// `snr` gets the decoded result's signal-to-noise ratio in dB.
std::vector<std::vector<uint8_t>> Radio_Encode(const std::vector<int16_t>& pcm, int channels, double* snr);

// A whole radio file decoded to interleaved 16-bit samples. False if it isn't one.
bool Radio_Decode(const std::vector<uint8_t>& file, std::vector<int16_t>& pcm, int& channels, int& rate);

}  // namespace audio
