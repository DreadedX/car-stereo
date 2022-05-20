#pragma once

#include <cstdint>

#define I2S_PORT I2S_NUM_0

namespace i2s {
	void init();

	uint32_t get_sample_rate();
	void set_sample_rate(uint32_t sample_rate);
}
