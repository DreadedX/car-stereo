#include "esp_log.h"

#include "helper.h"

const char* addr_to_str(esp_bd_addr_t bda) {
    static char bda_str[18];
    sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return (const char*)bda_str;
}

const char* connection_state_to_str(esp_a2d_connection_state_t state) {
	const char* states[4] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
	return states[state];
}

MultiPurposeButton::MultiPurposeButton(void(*short_press)(), void(*long_press)(), uint8_t threshold) : short_press(short_press), long_press(long_press), threshold(threshold) {}

void MultiPurposeButton::tick(bool current) {
	if (current && !acted) {
		if (counter >= threshold) {
			// Long press
			if (long_press) {
				ESP_LOGI("MPB", "Long press!");
				long_press();
			}
			acted = true;
		}

		counter++;
	} else if (previous != current && !current) {
		// The button just got released

		// Short press
		if (counter < threshold) {
			if (short_press) {
				ESP_LOGI("MPB", "Short press!");
				short_press();
			}
		}

		counter = 0;
		acted = false;
	}

	previous = current;
}
