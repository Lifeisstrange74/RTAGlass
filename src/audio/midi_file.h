// Standard MIDI File loader (format 0 or 1). All tracks are merged into one time-ordered list of channel messages
// and tempo changes. Port prefix meta events (FF 21) give channels 16-63, so a file can address more than 16
// channels; "loopStart"/"loopEnd" markers (FF 06) set a loop. tools/sndconv writes files this way.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

namespace audio {

struct MidiEvent {
    uint32_t tick;
    uint8_t status;   // 0x80-0xEF channel message, with the channel in `channel`; 0xFF = tempo
    uint8_t channel;  // 0-63: port * 16 + MIDI channel
    uint8_t a, b;
    uint32_t tempo;   // microseconds per quarter note, for status 0xFF
};

class MidiFile {
public:
    bool Load(const std::string& path);
    const std::string& Error() const { return error_; }

    const std::vector<MidiEvent>& Events() const { return events_; }
    int Division() const { return division_; }     // ticks per quarter note
    uint32_t Length() const { return length_; }    // last tick
    bool Loops() const { return loopEnd_ > loopStart_; }
    uint32_t LoopStart() const { return loopStart_; }
    uint32_t LoopEnd() const { return loopEnd_; }

private:
    std::vector<MidiEvent> events_;
    int division_ = 96;
    uint32_t length_ = 0, loopStart_ = 0, loopEnd_ = 0;
    std::string error_;
};

}  // namespace audio
