#pragma once

#include <memory>

namespace aimc {

// Small procedural sound engine on top of miniaudio. Device failures (e.g. a
// headless container without /dev/snd) are non-fatal: ready() returns false.
class Audio {
public:
    enum class Sound {
        Step = 0,
        Break,
        Place,
        Jump,
        Land,
        Count,
    };

    Audio();
    ~Audio();

    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init();
    void shutdown();
    bool ready() const;
    void play(Sound sound, float gain = 1.0f);
    void setVolume(float volume);
    float volume() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

}  // namespace aimc
