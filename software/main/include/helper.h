#pragma once

#include "esp_bt_device.h"
#include "esp_a2dp_api.h"

const char* addr_to_str(esp_bd_addr_t bda);
const char* connection_state_to_str(esp_a2d_connection_state_t state);

class MultiPurposeButton {
	public:
		MultiPurposeButton(void(*short_press)(), void(*long_press)(), uint16_t threshold = 500);

		void update(bool current);

	private:
		void(* const short_press)();
		void(* const long_press)();
		const int16_t threshold = 500;
		uint64_t start = 0;
		bool previous = false;
		bool acted = false;
};
