#pragma once

#include <memory>
#include <string>

// Forward-declare miniaudio types so the header doesn't drag the 4MB single-header
// implementation into every translation unit. We hold them by std::unique_ptr in
// the impl wrapper.
struct ma_engine;
struct ma_sound;

/**
 * SoundtrackPlayer — plays an MP3 from disk on a continuous loop.
 *
 * Usage:
 *   SoundtrackPlayer player;
 *   player.init();                                  // start audio engine
 *   player.playLoop("resources/soundtrack/foo.mp3"); // begin looping playback
 *   ...
 *   // Destructor stops playback and shuts down the engine cleanly.
 *
 * Thread-safety: not thread-safe. Call from a single thread (typically main).
 */
class SoundtrackPlayer {
public:
    SoundtrackPlayer();
    ~SoundtrackPlayer();

    SoundtrackPlayer(const SoundtrackPlayer&) = delete;
    SoundtrackPlayer& operator=(const SoundtrackPlayer&) = delete;

    /** Initialize the audio engine. Returns false on failure. */
    bool init();

    /**
     * Begin looping playback of the file at `path`. Replaces any track already
     * playing. Returns false on failure (engine not initialized, file missing,
     * decode error, etc.). Failure is non-fatal — the game continues silently.
     */
    bool playLoop(const std::string& path);

    /** Stop playback and unload the current track. Engine remains initialized. */
    void stop();

    /** Optional: linear gain multiplier in [0, 1+]. Default 1.0. */
    void setVolume(float volume);

    bool isInitialized() const { return engine_ != nullptr; }

private:
    std::unique_ptr<ma_engine> engine_;
    std::unique_ptr<ma_sound>  sound_;
    bool soundLoaded_ = false;
    float volume_ = 1.0f;
};
