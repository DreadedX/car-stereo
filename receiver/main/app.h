#pragma once
#include <cstdint>

namespace app {
	typedef void (*Callback) (uint16_t event, void *param);
	struct Message {
		uint16_t event;
		Callback callback;
		void* param;
	};

	void start();
	/* void stop(); */
	void dispatch(Callback callback, uint16_t event, void* param, int param_len);

	uint32_t get_sample_rate();
}
