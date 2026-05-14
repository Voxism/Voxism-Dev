#include "SoundtrackPlayer.h"

// miniaudio is a single-header library. Define the implementation in exactly
// one .cpp file (this one). Disable subsystems we don't use to shrink the
// build and avoid pulling in extra OS deps.
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_FLAC    // we play MP3 only
#define MA_NO_WAV
#include <miniaudio/miniaudio.h>

#include <iostream>

SoundtrackPlayer::SoundtrackPlayer() = default;

SoundtrackPlayer::~SoundtrackPlayer()
{
    if (soundLoaded_ && sound_) {
        ma_sound_uninit(sound_.get());
        soundLoaded_ = false;
    }
    if (engine_) {
        ma_engine_uninit(engine_.get());
    }
}

bool SoundtrackPlayer::init()
{
    if (engine_) {
        return true;
    }
    engine_ = std::unique_ptr<ma_engine>(new ma_engine());
    ma_result result = ma_engine_init(nullptr, engine_.get());
    if (result != MA_SUCCESS) {
        std::cerr << "SoundtrackPlayer: ma_engine_init failed (code " << result
                  << ") — audio disabled" << std::endl;
        engine_.reset();
        return false;
    }
    return true;
}

bool SoundtrackPlayer::playLoop(const std::string& path)
{
    if (!engine_) {
        std::cerr << "SoundtrackPlayer: engine not initialized" << std::endl;
        return false;
    }

    // Stop and unload any current track before starting a new one.
    stop();

    sound_ = std::unique_ptr<ma_sound>(new ma_sound());

    // MA_SOUND_FLAG_STREAM avoids loading the entire MP3 into memory — it
    // decodes in chunks as the file plays. For a several-minute track this
    // is the right default.
    ma_result result = ma_sound_init_from_file(
        engine_.get(),
        path.c_str(),
        MA_SOUND_FLAG_STREAM,
        nullptr,  // no sound group
        nullptr,  // no fence
        sound_.get());
    if (result != MA_SUCCESS) {
        std::cerr << "SoundtrackPlayer: failed to load '" << path << "' (code "
                  << result << ")" << std::endl;
        sound_.reset();
        return false;
    }

    soundLoaded_ = true;
    ma_sound_set_looping(sound_.get(), MA_TRUE);
    ma_sound_set_volume(sound_.get(), volume_);

    result = ma_sound_start(sound_.get());
    if (result != MA_SUCCESS) {
        std::cerr << "SoundtrackPlayer: ma_sound_start failed (code " << result
                  << ")" << std::endl;
        ma_sound_uninit(sound_.get());
        sound_.reset();
        soundLoaded_ = false;
        return false;
    }
    return true;
}

void SoundtrackPlayer::stop()
{
    if (soundLoaded_ && sound_) {
        ma_sound_stop(sound_.get());
        ma_sound_uninit(sound_.get());
        soundLoaded_ = false;
    }
    sound_.reset();
}

void SoundtrackPlayer::setVolume(float volume)
{
    volume_ = volume;
    if (soundLoaded_ && sound_) {
        ma_sound_set_volume(sound_.get(), volume_);
    }
}
