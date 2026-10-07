#include "audio_engine.h"
#include <android/log.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>

#define LOG_TAG "BreachVR_Audio"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace breachvr {

AudioEngine& AudioEngine::Instance() {
    static AudioEngine instance;
    return instance;
}

void AudioEngine::BqCallback(SLAndroidSimpleBufferQueueItf bq, void* context) {
    auto* engine = static_cast<AudioEngine*>(context);
    int nextBuf = 1 - engine->currentBufferIndex_;
    engine->currentBufferIndex_ = nextBuf;

    int16_t* targetBuf = engine->pingPongBuffers_[nextBuf];
    engine->ProcessAudio(targetBuf, BUFFER_FRAMES);

    (*bq)->Enqueue(bq, targetBuf, BUFFER_FRAMES * 2 * sizeof(int16_t));
}

bool AudioEngine::Init() {
    if (initialized_) return true;

    SLresult res;

    // 1. Create OpenSL ES engine
    res = slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
    if (res != SL_RESULT_SUCCESS) {
        LOGE("slCreateEngine failed: %d", res);
        return false;
    }

    res = (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) return false;

    res = (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineInterface_);
    if (res != SL_RESULT_SUCCESS) return false;

    // 2. Create Output Mix
    res = (*engineInterface_)->CreateOutputMix(engineInterface_, &outputMixObject_, 0, nullptr, nullptr);
    if (res != SL_RESULT_SUCCESS) return false;

    res = (*outputMixObject_)->Realize(outputMixObject_, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) return false;

    // 3. Audio Sink and Source configuration
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };

    SLDataFormat_PCM format_pcm = {
        SL_DATAFORMAT_PCM,
        2, // stereo
        SL_SAMPLINGRATE_44_1,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
        SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = { &loc_bufq, &format_pcm };

    SLDataLocator_OutputMix loc_outmix = { SL_DATALOCATOR_OUTPUTMIX, outputMixObject_ };
    SLDataSink audioSnk = { &loc_outmix, nullptr };

    const SLInterfaceID ids[2] = { SL_IID_BUFFERQUEUE, SL_IID_PLAY };
    const SLboolean req[2] = { SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE };

    res = (*engineInterface_)->CreateAudioPlayer(engineInterface_, &playerObject_, &audioSrc, &audioSnk, 2, ids, req);
    if (res != SL_RESULT_SUCCESS) {
        LOGE("CreateAudioPlayer failed: %d", res);
        return false;
    }

    res = (*playerObject_)->Realize(playerObject_, SL_BOOLEAN_FALSE);
    if (res != SL_RESULT_SUCCESS) return false;

    res = (*playerObject_)->GetInterface(playerObject_, SL_IID_PLAY, &playerPlay_);
    if (res != SL_RESULT_SUCCESS) return false;

    res = (*playerObject_)->GetInterface(playerObject_, SL_IID_BUFFERQUEUE, &bufferQueue_);
    if (res != SL_RESULT_SUCCESS) return false;

    res = (*bufferQueue_)->RegisterCallback(bufferQueue_, BqCallback, this);
    if (res != SL_RESULT_SUCCESS) return false;

    // Prime the buffers
    std::fill_n(pingPongBuffers_[0], BUFFER_FRAMES * 2, 0);
    std::fill_n(pingPongBuffers_[1], BUFFER_FRAMES * 2, 0);

    (*bufferQueue_)->Enqueue(bufferQueue_, pingPongBuffers_[0], BUFFER_FRAMES * 2 * sizeof(int16_t));
    (*bufferQueue_)->Enqueue(bufferQueue_, pingPongBuffers_[1], BUFFER_FRAMES * 2 * sizeof(int16_t));

    res = (*playerPlay_)->SetPlayState(playerPlay_, SL_PLAYSTATE_PLAYING);
    if (res != SL_RESULT_SUCCESS) return false;

    initialized_ = true;
    LOGI("AudioEngine initialized successfully (OpenSL ES 44.1kHz stereo)");
    return true;
}

void AudioEngine::PlaySound(SoundType type, float volume, float pan) {
    std::lock_guard<std::mutex> lock(voiceMutex_);

    ActiveVoice voice;
    voice.type = type;
    voice.playbackPos = 0.0f;
    voice.volume = volume;
    voice.pan = std::clamp(pan, -1.0f, 1.0f);

    switch (type) {
        case SoundType::CarbineShot:       voice.duration = 0.45f; break;
        case SoundType::PistolSuppressed:  voice.duration = 0.20f; break;
        case SoundType::BreachExplosion:   voice.duration = 1.60f; break;
        case SoundType::FlashbangBlast:    voice.duration = 3.50f; break;
        case SoundType::ReloadMagazine:    voice.duration = 0.40f; break;
        case SoundType::DoorKick:          voice.duration = 0.35f; break;
        case SoundType::HitConfirmation:   voice.duration = 0.12f; break;
        case SoundType::RadioChirp:        voice.duration = 0.15f; break;
        case SoundType::DryFire:           voice.duration = 0.08f; break;
    }

    activeVoices_.push_back(voice);
}

static inline float WhiteNoise() {
    return (float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f;
}

void AudioEngine::ProcessAudio(int16_t* buffer, size_t numFrames) {
    float dt = 1.0f / (float)SAMPLE_RATE;

    std::vector<float> leftOut(numFrames, 0.0f);
    std::vector<float> rightOut(numFrames, 0.0f);

    {
        std::lock_guard<std::mutex> lock(voiceMutex_);

        for (auto& v : activeVoices_) {
            if (v.finished) continue;

            float panL = std::cos((v.pan + 1.0f) * 0.25f * 3.14159f);
            float panR = std::sin((v.pan + 1.0f) * 0.25f * 3.14159f);

            for (size_t f = 0; f < numFrames; ++f) {
                float t = v.playbackPos;
                if (t >= v.duration) {
                    v.finished = true;
                    break;
                }

                float sample = 0.0f;

                switch (v.type) {
                    case SoundType::CarbineShot: {
                        // Sharp noise crack + low punch + decaying rumble
                        float noise = WhiteNoise() * std::exp(-t * 45.0f);
                        float punch = std::sin(2.0f * 3.14159f * (160.0f - t * 120.0f) * t) * std::exp(-t * 22.0f);
                        float rumble = WhiteNoise() * 0.25f * std::exp(-t * 8.0f);
                        sample = (noise * 0.7f + punch * 0.5f + rumble) * 0.9f;
                        break;
                    }
                    case SoundType::PistolSuppressed: {
                        // Suppressed "pfft" whisper + light click
                        float puff = WhiteNoise() * std::exp(-t * 80.0f);
                        float sub = std::sin(2.0f * 3.14159f * 95.0f * t) * std::exp(-t * 35.0f);
                        sample = (puff * 0.6f + sub * 0.4f) * 0.6f;
                        break;
                    }
                    case SoundType::BreachExplosion: {
                        // Massive C4 breach explosion (low rumble + crack + debris)
                        float shock = WhiteNoise() * std::exp(-t * 15.0f);
                        float subBass = std::sin(2.0f * 3.14159f * (65.0f - t * 25.0f) * t) * std::exp(-t * 2.5f);
                        float debris = WhiteNoise() * 0.35f * std::exp(-t * 4.0f) * (t > 0.08f ? 1.0f : 0.0f);
                        sample = (shock * 0.7f + subBass * 0.9f + debris) * 1.0f;
                        break;
                    }
                    case SoundType::FlashbangBlast: {
                        // Initial concussion blast (0-0.2s) followed by high tinnitus ear ringing (3800 Hz)
                        if (t < 0.2f) {
                            float blast = WhiteNoise() * std::exp(-t * 20.0f);
                            float sub = std::sin(2.0f * 3.14159f * 120.0f * t) * std::exp(-t * 15.0f);
                            sample = (blast * 0.8f + sub * 0.5f);
                        } else {
                            float ringT = t - 0.2f;
                            float tinnitus = std::sin(2.0f * 3.14159f * 3700.0f * t) * std::exp(-ringT * 0.85f);
                            sample = tinnitus * 0.45f;
                        }
                        break;
                    }
                    case SoundType::ReloadMagazine: {
                        // Two crisp metallic clicks (mag insert, slide rack)
                        float click1 = (t >= 0.02f && t < 0.10f) ? (WhiteNoise() * std::exp(-(t - 0.02f) * 60.0f)) : 0.0f;
                        float click2 = (t >= 0.22f && t < 0.35f) ? (WhiteNoise() * std::exp(-(t - 0.22f) * 50.0f)) : 0.0f;
                        sample = (click1 + click2) * 0.7f;
                        break;
                    }
                    case SoundType::DoorKick: {
                        // Heavy door breach impact
                        float impact = std::sin(2.0f * 3.14159f * 85.0f * t) * std::exp(-t * 18.0f);
                        float splinter = WhiteNoise() * std::exp(-t * 30.0f);
                        sample = (impact * 0.7f + splinter * 0.4f) * 0.85f;
                        break;
                    }
                    case SoundType::HitConfirmation: {
                        // Tactical audio confirmation beep
                        float tone = std::sin(2.0f * 3.14159f * 2200.0f * t) * std::exp(-t * 25.0f);
                        sample = tone * 0.5f;
                        break;
                    }
                    case SoundType::RadioChirp: {
                        // Radio squelch / tactical beep
                        float beep = std::sin(2.0f * 3.14159f * 1100.0f * t) * (t < 0.06f ? 1.0f : 0.0f);
                        float staticNoise = WhiteNoise() * 0.15f * std::exp(-t * 12.0f);
                        sample = (beep + staticNoise) * 0.5f;
                        break;
                    }
                    case SoundType::DryFire: {
                        float click = WhiteNoise() * std::exp(-t * 120.0f);
                        sample = click * 0.4f;
                        break;
                    }
                }

                leftOut[f]  += sample * v.volume * panL;
                rightOut[f] += sample * v.volume * panR;

                v.playbackPos += dt;
            }
        }

        // Remove finished voices
        activeVoices_.erase(
            std::remove_if(activeVoices_.begin(), activeVoices_.end(), [](const ActiveVoice& v) {
                return v.finished;
            }),
            activeVoices_.end()
        );
    }

    // Interleave and clamp to 16-bit PCM
    for (size_t f = 0; f < numFrames; ++f) {
        float l = std::clamp(leftOut[f],  -1.0f, 1.0f);
        float r = std::clamp(rightOut[f], -1.0f, 1.0f);
        buffer[f * 2 + 0] = static_cast<int16_t>(l * 32767.0f);
        buffer[f * 2 + 1] = static_cast<int16_t>(r * 32767.0f);
    }
}

void AudioEngine::Teardown() {
    if (playerPlay_) {
        (*playerPlay_)->SetPlayState(playerPlay_, SL_PLAYSTATE_STOPPED);
    }
    if (playerObject_) {
        (*playerObject_)->Destroy(playerObject_);
        playerObject_ = nullptr;
    }
    if (outputMixObject_) {
        (*outputMixObject_)->Destroy(outputMixObject_);
        outputMixObject_ = nullptr;
    }
    if (engineObject_) {
        (*engineObject_)->Destroy(engineObject_);
        engineObject_ = nullptr;
    }
    initialized_ = false;
    LOGI("AudioEngine torn down");
}

} // namespace breachvr
