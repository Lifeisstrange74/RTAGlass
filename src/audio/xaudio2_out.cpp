// XAudio2 2.7 output: one 16-bit stereo source voice fed from a thread, four buffers of 1024 frames
// (about 85 ms of latency at 48 kHz). Everything XAudio2 runs on that thread, which owns its COM apartment.
#include <windows.h>
#include <XAudio2.h>

#include <vector>

#include "audio_out.h"

namespace audio {

namespace {

const int kBuffers = 4;
const int kFrames = 1024;

class XAudio2Out : public AudioOut, public IXAudio2VoiceCallback {
public:
    ~XAudio2Out() override { Stop(); }

    bool Start(int rate, Fill fill, void* user) override {
        rate_ = rate;
        fill_ = fill;
        user_ = user;
        // xaudio2_7.dll stays loaded for the life of the process: unloading it on exit can crash (a known 2.7 bug).
        if (!LoadLibraryA("xaudio2_7.dll")) return false;
        quit_ = false;
        bufferDone_ = CreateEventA(nullptr, FALSE, FALSE, nullptr);
        started_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        thread_ = CreateThread(nullptr, 0, ThreadMain, this, 0, nullptr);
        if (!thread_) return false;
        WaitForSingleObject(started_, INFINITE);
        if (!ok_) {
            WaitForSingleObject(thread_, INFINITE);
            Close();
        }
        return ok_;
    }

    void Stop() override {
        if (!thread_) return;
        quit_ = true;
        SetEvent(bufferDone_);
        WaitForSingleObject(thread_, INFINITE);
        Close();
    }

    // IXAudio2VoiceCallback
    void STDMETHODCALLTYPE OnBufferEnd(void*) override { SetEvent(bufferDone_); }
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnStreamEnd() override {}
    void STDMETHODCALLTYPE OnBufferStart(void*) override {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) override {}

private:
    static DWORD WINAPI ThreadMain(void* self) {
        static_cast<XAudio2Out*>(self)->Run();
        return 0;
    }

    void Run() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        IXAudio2* xa = nullptr;
        IXAudio2MasteringVoice* master = nullptr;
        IXAudio2SourceVoice* source = nullptr;
        WAVEFORMATEX fmt = {};
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 2;
        fmt.nSamplesPerSec = rate_;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4;
        fmt.nAvgBytesPerSec = rate_ * 4;
        ok_ = SUCCEEDED(XAudio2Create(&xa, 0, XAUDIO2_DEFAULT_PROCESSOR)) &&
              SUCCEEDED(xa->CreateMasteringVoice(&master, 2, rate_)) &&
              SUCCEEDED(xa->CreateSourceVoice(&source, &fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, this)) &&
              SUCCEEDED(source->Start(0));
        SetEvent(started_);
        if (ok_) {
            std::vector<int16_t> buffers(kBuffers * kFrames * 2);
            int next = 0;
            while (!quit_) {
                XAUDIO2_VOICE_STATE state;
                source->GetState(&state);
                if (state.BuffersQueued < (UINT32)kBuffers) {
                    int16_t* b = &buffers[next * kFrames * 2];
                    fill_(user_, b, kFrames);
                    XAUDIO2_BUFFER xb = {};
                    xb.AudioBytes = kFrames * 4;
                    xb.pAudioData = (const BYTE*)b;
                    source->SubmitSourceBuffer(&xb);
                    next = (next + 1) % kBuffers;
                    continue;
                }
                WaitForSingleObject(bufferDone_, 100);
            }
            source->Stop(0);
        }
        if (source) source->DestroyVoice();
        if (master) master->DestroyVoice();
        if (xa) xa->Release();
        CoUninitialize();
    }

    void Close() {
        CloseHandle(thread_);
        CloseHandle(bufferDone_);
        CloseHandle(started_);
        thread_ = bufferDone_ = started_ = nullptr;
    }

    int rate_ = 48000;
    Fill fill_ = nullptr;
    void* user_ = nullptr;
    HANDLE thread_ = nullptr, bufferDone_ = nullptr, started_ = nullptr;
    volatile bool quit_ = false, ok_ = false;
};

}  // namespace

AudioOut* CreateXAudio2Out() { return new XAudio2Out; }

}  // namespace audio
