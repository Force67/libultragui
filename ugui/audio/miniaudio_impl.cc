// Single translation unit compiling the miniaudio implementation.
//
// Kept separate from the swappable audio backend (ULTRAGUI_AUDIO_SOURCE) so
// the video player's audio bridge still links when a custom backend replaces
// the bundled AudioEngine. Compiled when the default backend is used or when
// video is enabled (see CMakeLists.txt).
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
