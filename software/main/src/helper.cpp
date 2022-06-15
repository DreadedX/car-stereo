#include "esp_log.h"
#include "esp_timer.h"

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

MultiPurposeButton::MultiPurposeButton(void(*short_press)(), void(*long_press)(), uint16_t threshold) : short_press(short_press), long_press(long_press), threshold(threshold) {}

// @TOOD Use a timer instead of amount of updates as this can be inconsistent (e.g. when in eco mode)
void MultiPurposeButton::update(bool current) {
	if (current != previous && current) {
		// Button just got presset
		start = esp_timer_get_time();
	} else if (previous != current && !current) {
		// The button just got released
		// Check for short press
		if (esp_timer_get_time() - start < threshold) {
			if (short_press) {
				ESP_LOGI("MPB", "Short press!");
				short_press();
			}
		}

		acted = false;
	} else if (current && !acted) {
		// Button is still being pressed
		// Check if we exceed the timer for a long press
		if (esp_timer_get_time() >= threshold) {
			if (long_press) {
				ESP_LOGI("MPB", "Long press!");
				long_press();
			}
			
			// Make sure we only execute the long press once
			acted = true;
		}
	}

	previous = current;
}
