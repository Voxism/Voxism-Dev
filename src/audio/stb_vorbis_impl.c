/*
 * Translation unit that compiles stb_vorbis as a standalone C source.
 *
 * Why a separate file: when stb_vorbis.c is included from a C++ TU on MinGW,
 * the C++ front-end fails on the channel-routing macro definitions around
 * line 5128 (STBV_PLAYBACK_LEFT/RIGHT/MONO). Compiling it as a regular C
 * source sidesteps that and just produces an object the C++ code can link
 * against. The decode API (stb_vorbis_decode_filename) is C-linkage compatible.
 */

#include <miniaudio/stb_vorbis.c>
