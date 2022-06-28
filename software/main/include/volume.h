#pragma once

#include <cstdint>
#include <sys/lock.h>

namespace volume_controller {
	void init();
	void set_from_radio(int volume);
	void set_from_remote(int volume);

	uint8_t current();
}
