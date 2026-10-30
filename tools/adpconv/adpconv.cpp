// adpconv: converts between 16-bit WAV files and the game's radio files (.adp: PS-ADPCM, the PS2 sound chip's own
// format, with seek points; src/audio/adpcm.h). For mods: replace a radio station with your own recording at about
// a quarter of a WAV's size, or get a station out to listen to or edit. It goes in the player folder's tools.
//
// Usage:
//   adpconv encode IN.wav OUT.adp   any 16-bit PCM WAV, mono or stereo, any rate (PS-ADPCM is lossy: it prints
//                                   the result's signal-to-noise ratio)
//   adpconv decode IN.adp OUT.wav   back to a 16-bit WAV, exactly the samples the game plays
//   adpconv info IN.adp
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include <string>
#include <vector>

#include "../../src/audio/adpcm.h"
#include "../../src/audio/wav_file.h"

namespace {

bool ReadAll(const std::string& path, std::vector<uint8_t>& out) {
    FILE* fp = fopen(path.c_str(), "rb");
    if (!fp) return false;
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    out.resize(size > 0 ? (size_t)size : 0);
    bool ok = size >= 0 && fread(out.data(), 1, out.size(), fp) == out.size();
    fclose(fp);
    return ok;
}

// Through a temporary file, so a failed write leaves an existing file as it was.
bool WriteAll(const std::string& path, const std::vector<uint8_t>& data) {
    std::string tmp = path + ".tmp";
    FILE* fp = fopen(tmp.c_str(), "wb");
    if (!fp) return false;
    bool ok = fwrite(data.data(), 1, data.size(), fp) == data.size();
    ok &= fclose(fp) == 0;
    if (!ok || !MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

std::string Length(size_t frames, int rate) {
    unsigned s = (unsigned)(frames / rate);
    char buf[32];
    snprintf(buf, sizeof buf, "%u:%02u:%02u", s / 3600, s / 60 % 60, s % 60);
    return buf;
}

int Encode(const std::string& in, const std::string& out) {
    std::vector<uint8_t> file;
    std::vector<int16_t> pcm;
    int channels = 0, rate = 0;
    if (!ReadAll(in, file)) {
        fprintf(stderr, "adpconv: can't read %s\n", in.c_str());
        return 1;
    }
    if (!audio::Wav_Parse(file, pcm, channels, rate) || pcm.empty()) {
        fprintf(stderr, "adpconv: %s isn't a 16-bit PCM WAV, mono or stereo\n", in.c_str());
        return 1;
    }
    DWORD t0 = GetTickCount();
    double snr = 0;
    std::vector<uint8_t> adp = audio::Radio_BuildFile(audio::Radio_Encode(pcm, channels, &snr), rate);
    if (!WriteAll(out, adp)) {
        fprintf(stderr, "adpconv: can't write %s\n", out.c_str());
        return 1;
    }
    printf("%s: %s, %s at %d Hz, %.1f MB -> %.1f MB, SNR %.1f dB (%lu ms)\n", out.c_str(),
           Length(pcm.size() / channels, rate).c_str(), channels == 2 ? "stereo" : "mono", rate, file.size() / 1e6,
           adp.size() / 1e6, snr, (unsigned long)(GetTickCount() - t0));
    return 0;
}

int Decode(const std::string& in, const std::string& out) {
    std::vector<uint8_t> file;
    std::vector<int16_t> pcm;
    int channels = 0, rate = 0;
    if (!ReadAll(in, file)) {
        fprintf(stderr, "adpconv: can't read %s\n", in.c_str());
        return 1;
    }
    if (!audio::Radio_Decode(file, pcm, channels, rate)) {
        fprintf(stderr, "adpconv: %s isn't a radio file\n", in.c_str());
        return 1;
    }
    if (!WriteAll(out, audio::Wav_Build(pcm, channels, rate))) {
        fprintf(stderr, "adpconv: can't write %s\n", out.c_str());
        return 1;
    }
    printf("%s: %s, %s at %d Hz\n", out.c_str(), Length(pcm.size() / channels, rate).c_str(),
           channels == 2 ? "stereo" : "mono", rate);
    return 0;
}

int Info(const std::string& in) {
    std::vector<uint8_t> file;
    audio::RadioHeader h;
    if (!ReadAll(in, file) || file.size() < (size_t)audio::kRadioHeaderSize ||
        !audio::Radio_ParseHeader(file.data(), (int64_t)file.size(), h)) {
        fprintf(stderr, "adpconv: %s isn't a radio file\n", in.c_str());
        return 1;
    }
    printf("%s: %s, %s at %d Hz, %lld blocks a side, a seek point every %d blocks (%u), %.1f MB\n", in.c_str(),
           Length((size_t)h.blocks * audio::kAdpcmBlockSamples, h.rate).c_str(), h.channels == 2 ? "stereo" : "mono",
           h.rate, (long long)h.blocks, h.seekBlocks, h.seekCount, file.size() / 1e6);
    return 0;
}

int Usage() {
    fprintf(stderr,
            "usage: adpconv encode IN.wav OUT.adp   a 16-bit WAV (mono or stereo, any rate) to a radio file\n"
            "       adpconv decode IN.adp OUT.wav   a radio file to a 16-bit WAV\n"
            "       adpconv info IN.adp\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) return Usage();
    std::string cmd = argv[1];
    if (cmd == "encode" && argc == 4) return Encode(argv[2], argv[3]);
    if (cmd == "decode" && argc == 4) return Decode(argv[2], argv[3]);
    if (cmd == "info" && argc == 3) return Info(argv[2]);
    return Usage();
}
