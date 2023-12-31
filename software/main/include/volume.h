#pragma once

#include <cstdint>

namespace volume_controller {
	void init();
	void set_from_radio(int volume);
	void set_from_remote(int volume);
	void cancel_sync();

	uint8_t current();
}
