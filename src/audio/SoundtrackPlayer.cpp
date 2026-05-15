// stb_vorbis is compiled as a separate C translation unit (stb_vorbis_impl.c).
// On MinGW, including stb_vorbis.c directly into a C++ TU triggers an
// upstream parser bug on its channel-routing macros. Compiling it as plain C
// avoids that and we just declare the one decode function we use here.
extern "C" {
    int stb_vorbis_decode_filename(const char* filename, int* channels,
                                   int* sample_rate, short** output);
}

// Now safe to bring in miniaudio's full implementation. We keep WAV enabled
// (some "sfx/*.ogg" assets are actually WAV containers — the file extension
// is misleading — so the built-in dr_wav decoder handles them).
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_FLAC
#include <miniaudio/miniaudio.h>

#include "SoundtrackPlayer.h"

#include <cstdlib>
#include <iostream>

// ---------------------------------------------------------------------------
// PIMPL: keep all miniaudio types out of the public header.
// ---------------------------------------------------------------------------
struct SoundtrackPlayer::Impl {
    struct SfxClip {
        std::vector<short>            pcm;             // interleaved s16
        ma_uint64                     frameCount = 0;
        ma_uint32                     channels   = 0;
        ma_uint32                     sampleRate = 0;
        std::vector<ma_audio_buffer*> buffers;         // one per voice
        std::vector<ma_sound*>        voices;
        size_t                        nextVoice = 0;   // round-robin fallback
    };

    ma_engine engine{};
    bool      engineInitialized = false;

    ma_sound  music{};
    bool      musicLoaded = false;
    float     musicVolume = 1.0f;

    std::unordered_map<std::string, SfxClip> sfx;
};

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

SoundtrackPlayer::SoundtrackPlayer() : impl_(new Impl()) {}

SoundtrackPlayer::~SoundtrackPlayer()
{
    if (!impl_) return;

    // Tear down SFX voices first (they reference the engine).
    for (auto& kv : impl_->sfx) {
        Impl::SfxClip& clip = kv.second;
        for (ma_sound* s : clip.voices) {
            if (s) {
                ma_sound_uninit(s);
                delete s;
            }
        }
        for (ma_audio_buffer* b : clip.buffers) {
            if (b) {
                ma_audio_buffer_uninit(b);
                delete b;
            }
        }
    }
    impl_->sfx.clear();

    if (impl_->musicLoaded) {
        ma_sound_uninit(&impl_->music);
        impl_->musicLoaded = false;
    }
    if (impl_->engineInitialized) {
        ma_engine_uninit(&impl_->engine);
        impl_->engineInitialized = false;
    }

    delete impl_;
    impl_ = nullptr;
}

bool SoundtrackPlayer::isInitialized() const
{
    return impl_ && impl_->engineInitialized;
}

// ---------------------------------------------------------------------------
// Engine init / music loop
// ---------------------------------------------------------------------------

bool SoundtrackPlayer::init()
{
    if (!impl_) return false;
    if (impl_->engineInitialized) return true;

    ma_result result = ma_engine_init(nullptr, &impl_->engine);
    if (result != MA_SUCCESS) {
        std::cerr << "SoundtrackPlayer: ma_engine_init failed (code " << result
                  << ") — audio disabled" << std::endl;
        return false;
    }
    impl_->engineInitialized = true;
    return true;
}

bool SoundtrackPlayer::playLoop(const std::string& path)
{
    if (!isInitialized()) {
        std::cerr << "SoundtrackPlayer: engine not initialized" << std::endl;
        return false;
    }

    stop();

    // Stream from disk so we don't load the whole MP3 into memory.
    ma_result result = ma_sound_init_from_file(
        &impl_->engine,
        path.c_str(),
        MA_SOUND_FLAG_STREAM,
        nullptr,
        nullptr,
        &impl_->music);
    if (result != MA_SUCCESS) {
        std::cerr << "SoundtrackPlayer: failed to load music '" << path
                  << "' (code " << result << ")" << std::endl;
        return false;
    }

    impl_->musicLoaded = true;
    ma_sound_set_looping(&impl_->music, MA_TRUE);
    ma_sound_set_volume(&impl_->music, impl_->musicVolume);

    result = ma_sound_start(&impl_->music);
    if (result != MA_SUCCESS) {
        std::cerr << "SoundtrackPlayer: ma_sound_start failed (code " << result
                  << ")" << std::endl;
        ma_sound_uninit(&impl_->music);
        impl_->musicLoaded = false;
        return false;
    }
    return true;
}

void SoundtrackPlayer::stop()
{
    if (impl_ && impl_->musicLoaded) {
        ma_sound_stop(&impl_->music);
        ma_sound_uninit(&impl_->music);
        impl_->musicLoaded = false;
    }
}

void SoundtrackPlayer::setVolume(float volume)
{
    if (!impl_) return;
    impl_->musicVolume = volume;
    if (impl_->musicLoaded) {
        ma_sound_set_volume(&impl_->music, impl_->musicVolume);
    }
}

// ---------------------------------------------------------------------------
// SFX: pre-decode OGG with stb_vorbis, play through a voice pool.
// ---------------------------------------------------------------------------

bool SoundtrackPlayer::loadSfx(const std::string& id, const std::string& path, int voices)
{
    if (!isInitialized()) {
        std::cerr << "SoundtrackPlayer::loadSfx: engine not initialized" << std::endl;
        return false;
    }
    if (voices < 1) voices = 1;

    // 1) Decode the file to interleaved s16 PCM. Try miniaudio's built-in
    //    decoders first (handles MP3 + WAV out of the box, including .ogg
    //    files that are actually RIFF/WAV containers); fall back to
    //    stb_vorbis for genuine OGG/Vorbis if that fails.
    Impl::SfxClip clip;

    {
        ma_decoder_config decoderCfg = ma_decoder_config_init(
            ma_format_s16,
            0,    // 0 channels = use the file's native channel count
            0);   // 0 sample rate = use the file's native sample rate

        ma_uint64 maFrameCount = 0;
        void*     maPcm        = nullptr;
        ma_result mr = ma_decode_file(path.c_str(), &decoderCfg, &maFrameCount, &maPcm);

        if (mr == MA_SUCCESS && maPcm != nullptr && maFrameCount > 0) {
            clip.frameCount = maFrameCount;
            clip.channels   = decoderCfg.channels;
            clip.sampleRate = decoderCfg.sampleRate;
            const size_t totalSamples =
                static_cast<size_t>(maFrameCount) *
                static_cast<size_t>(decoderCfg.channels);
            clip.pcm.assign(static_cast<short*>(maPcm),
                            static_cast<short*>(maPcm) + totalSamples);
            ma_free(maPcm, nullptr);
        } else {
            // miniaudio couldn't decode it — try stb_vorbis as a fallback.
            short* pcmRaw = nullptr;
            int channels = 0;
            int sampleRate = 0;
            int frameCount = stb_vorbis_decode_filename(
                path.c_str(), &channels, &sampleRate, &pcmRaw);
            if (frameCount <= 0 || pcmRaw == nullptr) {
                std::cerr << "SoundtrackPlayer::loadSfx: failed to decode '"
                          << path << "' (miniaudio code " << mr
                          << ", stb_vorbis frames=" << frameCount << ")" << std::endl;
                if (pcmRaw) std::free(pcmRaw);
                return false;
            }
            clip.frameCount = static_cast<ma_uint64>(frameCount);
            clip.channels   = static_cast<ma_uint32>(channels);
            clip.sampleRate = static_cast<ma_uint32>(sampleRate);
            const size_t totalSamples =
                static_cast<size_t>(frameCount) * static_cast<size_t>(channels);
            clip.pcm.assign(pcmRaw, pcmRaw + totalSamples);
            std::free(pcmRaw);
        }
    }

    // 2) Build N audio_buffer + sound voices that all reference the same PCM.
    //    Each ma_sound is independent — multiple can play simultaneously.
    for (int v = 0; v < voices; ++v) {
        ma_audio_buffer* buffer = new ma_audio_buffer();
        ma_audio_buffer_config bufCfg = ma_audio_buffer_config_init(
            ma_format_s16,
            clip.channels,
            clip.frameCount,
            clip.pcm.data(),
            nullptr);
        bufCfg.sampleRate = clip.sampleRate;  // tell the buffer its source rate
        if (ma_audio_buffer_init(&bufCfg, buffer) != MA_SUCCESS) {
            std::cerr << "SoundtrackPlayer::loadSfx: ma_audio_buffer_init failed for '"
                      << path << "' voice " << v << std::endl;
            delete buffer;
            for (ma_audio_buffer* b : clip.buffers) {
                ma_audio_buffer_uninit(b);
                delete b;
            }
            for (ma_sound* s : clip.voices) {
                ma_sound_uninit(s);
                delete s;
            }
            return false;
        }

        ma_sound* sound = new ma_sound();
        if (ma_sound_init_from_data_source(
                &impl_->engine,
                buffer,
                MA_SOUND_FLAG_NO_SPATIALIZATION,
                nullptr,
                sound) != MA_SUCCESS) {
            std::cerr << "SoundtrackPlayer::loadSfx: ma_sound_init_from_data_source failed for '"
                      << path << "' voice " << v << std::endl;
            ma_audio_buffer_uninit(buffer);
            delete buffer;
            delete sound;
            for (ma_audio_buffer* b : clip.buffers) {
                ma_audio_buffer_uninit(b);
                delete b;
            }
            for (ma_sound* s : clip.voices) {
                ma_sound_uninit(s);
                delete s;
            }
            return false;
        }

        clip.buffers.push_back(buffer);
        clip.voices.push_back(sound);
    }

    impl_->sfx[id] = std::move(clip);
    return true;
}

bool SoundtrackPlayer::playSfx(const std::string& id)
{
    if (!isInitialized()) return false;
    auto it = impl_->sfx.find(id);
    if (it == impl_->sfx.end()) return false;
    Impl::SfxClip& clip = it->second;
    if (clip.voices.empty()) return false;

    // Find an idle voice; if all are busy, fall back to round-robin restart.
    ma_sound* chosen = nullptr;
    size_t chosenIdx = 0;
    for (size_t i = 0; i < clip.voices.size(); ++i) {
        ma_sound* s = clip.voices[i];
        if (!ma_sound_is_playing(s)) {
            chosen = s;
            chosenIdx = i;
            break;
        }
    }
    if (!chosen) {
        chosenIdx = clip.nextVoice % clip.voices.size();
        clip.nextVoice = chosenIdx + 1;
        chosen = clip.voices[chosenIdx];
        ma_sound_stop(chosen);
    }

    // Rewind both the audio buffer (data source) and the sound (cursor) so
    // the clip plays from the beginning every trigger.
    ma_audio_buffer_seek_to_pcm_frame(clip.buffers[chosenIdx], 0);
    ma_sound_seek_to_pcm_frame(chosen, 0);

    if (ma_sound_start(chosen) != MA_SUCCESS) {
        return false;
    }
    return true;
}
