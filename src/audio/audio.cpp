#include "audio/audio.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

#include <miniaudio/miniaudio.h>
#include <spdlog/spdlog.h>

namespace aimc {
namespace {

constexpr int kSampleRate = 48000;
constexpr int kVoices = 24;

struct Rng {
    std::uint32_t state = 0x9E3779B9u;
    std::uint32_t next() {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }
    float unit() { return static_cast<float>(next() & 0xFFFFFFu) / 16777216.0f; }
    float bipolar() { return unit() * 2.0f - 1.0f; }
};

std::vector<float> makeNoise(float seconds, float lowpass, float decay, float gain) {
    const int samples = static_cast<int>(seconds * kSampleRate);
    std::vector<float> out(static_cast<std::size_t>(samples));
    Rng rng(0xC0FFEEu);
    float lp = 0.0f;
    for (int i = 0; i < samples; ++i) {
        lp += (rng.bipolar() - lp) * lowpass;
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        out[static_cast<std::size_t>(i)] = lp * std::exp(-decay * t) * gain;
    }
    return out;
}

std::vector<float> makeThump(float seconds, float startHz, float endHz, float decay, float gain) {
    const int samples = static_cast<int>(seconds * kSampleRate);
    std::vector<float> out(static_cast<std::size_t>(samples));
    float phase = 0.0f;
    for (int i = 0; i < samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(kSampleRate);
        const float hz = startHz + (endHz - startHz) * (t / seconds);
        phase += 2.0f * 3.14159265f * hz / static_cast<float>(kSampleRate);
        out[static_cast<std::size_t>(i)] = std::sin(phase) * std::exp(-decay * t) * gain;
    }
    return out;
}

}  // namespace

struct Audio::Impl {
    struct Voice {
        int clip = -1;
        float cursor = 0.0f;
        float gain = 1.0f;
        bool active = false;
    };

    ma_device device{};
    bool deviceReady = false;
    bool ready = false;
    ma_uint32 deviceRate = kSampleRate;

    std::array<std::vector<float>, static_cast<std::size_t>(Sound::Count)> clips;
    std::array<Voice, kVoices> voices{};
    std::mutex mutex;
    std::atomic<float> volume{0.7f};

    static void dataCallback(ma_device* device, void* output, const void* input,
                             ma_uint32 frameCount) {
        (void)input;
        auto* self = static_cast<Impl*>(device->pUserData);
        self->mix(static_cast<float*>(output), frameCount);
    }

    void buildClips() {
        clips[static_cast<std::size_t>(Sound::Step)] = makeNoise(0.09f, 0.16f, 42.0f, 0.35f);
        clips[static_cast<std::size_t>(Sound::Break)] = makeNoise(0.18f, 0.22f, 18.0f, 0.75f);
        clips[static_cast<std::size_t>(Sound::Place)] = makeNoise(0.09f, 0.45f, 48.0f, 0.50f);
        clips[static_cast<std::size_t>(Sound::Jump)] = makeThump(0.12f, 300.0f, 620.0f, 20.0f, 0.35f);
        clips[static_cast<std::size_t>(Sound::Land)] = makeThump(0.15f, 150.0f, 70.0f, 26.0f, 0.60f);
        clips[static_cast<std::size_t>(Sound::Ignite)] = makeNoise(0.34f, 0.07f, 7.5f, 0.65f);
        clips[static_cast<std::size_t>(Sound::Evaporate)] = makeNoise(0.30f, 0.45f, 15.0f, 0.35f);
    }

    void mix(float* output, ma_uint32 frameCount) {
        std::fill(output, output + static_cast<std::size_t>(frameCount) * 2, 0.0f);
        const float master = volume.load(std::memory_order_relaxed);
        const float rateScale = static_cast<float>(kSampleRate) / static_cast<float>(deviceRate);

        std::lock_guard<std::mutex> lock(mutex);
        for (Voice& voice : voices) {
            if (!voice.active) {
                continue;
            }
            const std::vector<float>& clip = clips[static_cast<std::size_t>(voice.clip)];
            if (clip.empty()) {
                voice.active = false;
                continue;
            }
            for (ma_uint32 i = 0; i < frameCount; ++i) {
                const auto index = static_cast<std::size_t>(voice.cursor);
                if (index + 1 >= clip.size()) {
                    voice.active = false;
                    break;
                }
                const float frac = voice.cursor - static_cast<float>(index);
                const float sample =
                    clip[index] + (clip[index + 1] - clip[index]) * frac;
                const float value = sample * voice.gain * master;
                output[static_cast<std::size_t>(i) * 2 + 0] += value;
                output[static_cast<std::size_t>(i) * 2 + 1] += value;
                voice.cursor += rateScale;
            }
        }

        for (std::size_t i = 0; i < static_cast<std::size_t>(frameCount) * 2; ++i) {
            output[i] = std::clamp(output[i], -1.0f, 1.0f);
        }
    }

    void addVoice(Sound sound, float gain) {
        const int clip = static_cast<int>(sound);
        for (Voice& voice : voices) {
            if (!voice.active) {
                voice.clip = clip;
                voice.cursor = 0.0f;
                voice.gain = gain;
                voice.active = true;
                return;
            }
        }
        // All voices busy: steal the first one.
        voices[0] = Voice{clip, 0.0f, gain, true};
    }
};

Audio::Audio() : m_impl(std::make_unique<Impl>()) {}
Audio::~Audio() { shutdown(); }

bool Audio::init() {
    m_impl->buildClips();

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 2;
    config.sampleRate = kSampleRate;
    config.dataCallback = &Impl::dataCallback;
    config.pUserData = m_impl.get();

    const ma_result result = ma_device_init(nullptr, &config, &m_impl->device);
    if (result != MA_SUCCESS) {
        spdlog::warn("audio device unavailable ({}), running silent",
                     ma_result_description(result));
        return false;
    }
    m_impl->deviceReady = true;
    m_impl->deviceRate = m_impl->device.sampleRate;

    if (ma_device_start(&m_impl->device) != MA_SUCCESS) {
        spdlog::warn("audio device failed to start, running silent");
        ma_device_uninit(&m_impl->device);
        m_impl->deviceReady = false;
        return false;
    }

    m_impl->ready = true;
    spdlog::info("audio ready: {} Hz", m_impl->deviceRate);
    return true;
}

void Audio::shutdown() {
    if (m_impl->deviceReady) {
        ma_device_stop(&m_impl->device);
        ma_device_uninit(&m_impl->device);
        m_impl->deviceReady = false;
    }
    m_impl->ready = false;
}

bool Audio::ready() const { return m_impl->ready; }

void Audio::play(Sound sound, float gain) {
    if (!m_impl->ready) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_impl->mutex);
    m_impl->addVoice(sound, gain);
}

void Audio::setVolume(float volume) {
    m_impl->volume.store(std::clamp(volume, 0.0f, 1.0f), std::memory_order_relaxed);
}

float Audio::volume() const { return m_impl->volume.load(std::memory_order_relaxed); }

}  // namespace aimc
