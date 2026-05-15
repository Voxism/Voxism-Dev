#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/**
 * SoundtrackPlayer — audio system for music + SFX.
 *
 * - The "music" channel streams a single MP3 from disk on a continuous loop.
 * - The "SFX" channel pre-decodes short OGG clips into memory at load time and
 *   plays them through a small per-clip voice pool so rapid triggers layer
 *   instead of cutting each other off.
 *
 * Why both: miniaudio's built-in resource manager doesn't decode OGG/Vorbis
 * (it's not bundled for licensing reasons). We use stb_vorbis to pre-decode
 * SFX into raw PCM, then play through an ma_audio_buffer data source. MP3 is
 * decoded by miniaudio's built-in dr_mp3 backend so the music stream works
 * out of the box.
 *
 * The miniaudio types are kept entirely inside the .cpp via an opaque PIMPL
 * struct. The header doesn't pull in miniaudio.h (which on Windows
 * transitively includes mmsystem.h and breaks stb_vorbis with macro
 * collisions).
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

    /** Stop music playback and unload the current track. Engine remains initialized. */
    void stop();

    /** Music gain multiplier in [0, 1+]. Default 1.0. */
    void setVolume(float volume);

    /**
     * Pre-load an OGG/Vorbis SFX from `path`, registering it under `id` for
     * later playback. Allocates a small voice pool per clip (default 4) so
     * rapid triggers can overlap rather than cut. Returns false on failure.
     */
    bool loadSfx(const std::string& id, const std::string& path, int voices = 4);

    /**
     * Play a previously-loaded SFX by id. Picks the next free voice from the
     * pool (round-robin if all are busy). No-op if id is unknown.
     */
    bool playSfx(const std::string& id);

    bool isInitialized() const;

private:
    struct Impl;
    Impl* impl_ = nullptr;
};
