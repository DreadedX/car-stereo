#pragma once

#include <cstdint>

#define WAV_PLAY(NAME) {\
	extern const uint8_t _binary_ ## NAME ## _wav_start[]; \
	extern const uint8_t _binary_ ## NAME ## _wav_end[]; \
	wav::play(_binary_ ## NAME ## _wav_start, _binary_ ## NAME ## _wav_end); \
}

namespace wav {
	void play(const uint8_t* start, const uint8_t* end);
}
