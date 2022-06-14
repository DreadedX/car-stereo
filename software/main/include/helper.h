#pragma once

#include "esp_bt_device.h"
#include "esp_a2dp_api.h"

const char* addr_to_str(esp_bd_addr_t bda);
const char* connection_state_to_str(esp_a2d_connection_state_t state);

class MultiPurposeButton {
	public:
		MultiPurposeButton(void(*short_press)(), void(*long_press)(), uint8_t threshold = 5);

		void tick(bool current);

	private:
		void(*short_press)();
		void(*long_press)();
		uint8_t threshold = 5;
		bool previous = false;
		uint8_t counter = 0;
		bool acted = false;
};
