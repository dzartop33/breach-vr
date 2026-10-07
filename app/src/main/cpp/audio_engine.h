#pragma once
// Breach VR - Procedural Spatial CQB Audio Synthesizer (OpenSL ES)

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <vector>
#include <mutex>
#include <cstdint>

namespace breachvr {

enum class SoundType {
    CarbineShot,
    PistolSuppressed,
    BreachExplosion,
    FlashbangBlast,
    ReloadMagazine,
    DoorKick,
    HitConfirmation,
    RadioChirp,
    DryFire
};

struct ActiveVoice {
    SoundType type;
    float playbackPos = 0.0f; // in seconds
    float duration = 0.5f;
    float volume = 1.0f;
    float pan = 0.0f; // -1.0 (left) to 1.0 (right)
    bool finished = false;
};

class AudioEngine {
public:
    static AudioEngine& Instance();

    bool Init();
    void Teardown();

    void PlaySound(SoundType type, float volume = 1.0f, float pan = 0.0f);
    void PlayCarbineShot()       { PlaySound(SoundType::CarbineShot, 1.0f); }
    void PlayPistolSuppressed()  { PlaySound(SoundType::PistolSuppressed, 0.7f); }
    void PlayBreachExplosion()   { PlaySound(SoundType::BreachExplosion, 1.2f); }
    void PlayFlashbang()         { PlaySound(SoundType::FlashbangBlast, 1.0f); }
    void PlayReload()            { PlaySound(SoundType::ReloadMagazine, 0.8f); }
    void PlayDoorKick()          { PlaySound(SoundType::DoorKick, 1.0f); }
    void PlayHitMarker()         { PlaySound(SoundType::HitConfirmation, 0.7f); }
    void PlayRadioChirp()        { PlaySound(SoundType::RadioChirp, 0.6f); }
    void PlayDryFire()           { PlaySound(SoundType::DryFire, 0.5f); }

    // Audio callback for OpenSL ES buffer queue
    void ProcessAudio(int16_t* buffer, size_t numFrames);

private:
    AudioEngine() = default;

    SLObjectItf engineObject_ = nullptr;
    SLEngineItf engineInterface_ = nullptr;
    SLObjectItf outputMixObject_ = nullptr;
    SLObjectItf playerObject_ = nullptr;
    SLPlayItf   playerPlay_ = nullptr;
    SLAndroidSimpleBufferQueueItf bufferQueue_ = nullptr;

    std::mutex voiceMutex_;
    std::vector<ActiveVoice> activeVoices_;
    bool initialized_ = false;

    static constexpr int SAMPLE_RATE = 44100;
    static constexpr int BUFFER_FRAMES = 512;
    int16_t pingPongBuffers_[2][BUFFER_FRAMES * 2] = {};
    int currentBufferIndex_ = 0;

    static void BqCallback(SLAndroidSimpleBufferQueueItf bq, void* context);
};

} // namespace breachvr
