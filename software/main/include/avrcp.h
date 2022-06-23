#pragma once

#include <cstdint>

namespace avrcp {
	void init();
	bool is_playing();

	void play();
	void pause();
	void play_pause();
	void forward();
	void backward();
	void seek_forward();
	void seek_backward();

	void set_volume(uint8_t volume);
}
