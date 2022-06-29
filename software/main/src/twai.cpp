#include <cmath>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"
#include "esp_log.h"
#include "hal/twai_types.h"

#include "twai.h"
#include "avrcp.h"
#include "can_data.h"
#include "volume.h"
#include "helper.h"

#define TWAI_TAG "APP_TWAI"

static bool enabled = false;

// @TODO Make sure that the other buttons are set to match the current state
// That way way the scroll wheel and long pressing will not have unintented effects
void twai::change_volume(bool up) {
	// Make sure we only change the volume if we are enabled
	if (enabled) {
		can::Buttons buttons;
		memset(&buttons, 0, sizeof(buttons));

		if (up) {
			buttons.volume_up = true;
		} else {
			buttons.volume_down = true;
		}

		twai_message_t message;
		memset(&message, 0, sizeof(message));

		message.identifier = BUTTONS_ID;
		message.data_length_code = 3;
		message.data[0] = ((uint8_t*)&buttons)[0];
		message.data[1] = ((uint8_t*)&buttons)[1];
		message.data[2] = ((uint8_t*)&buttons)[2];


		for (int i = 0; i < message.data_length_code; i++) {
			ESP_LOGI(TWAI_TAG, "%i: 0x%X", i, message.data[i]);
		}

		if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
			ESP_LOGI(TWAI_TAG, "Message queued for transmission");
		} else {
			ESP_LOGI(TWAI_TAG, "Failed tp queue message for transmission");
		}

		vTaskDelay(pdMS_TO_TICKS(50));

		buttons.volume_up = false;
		buttons.volume_down = false;
		message.data[0] = ((uint8_t*)&buttons)[0];

		for (int i = 0; i < message.data_length_code; i++) {
			ESP_LOGI(TWAI_TAG, "%i: 0x%X", i, message.data[i]);
		}

		if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
			ESP_LOGI(TWAI_TAG, "Message queued for transmission");
		} else {
			ESP_LOGI(TWAI_TAG, "Failed tp queue message for transmission");
		}
	}
}

static void listen(void*) {
	for (;;) {
		twai_message_t message;
		if (twai_receive(&message, portMAX_DELAY) != ESP_OK) {
			ESP_LOGI(TWAI_TAG, "Failed to receive message");
			continue;
		}

		if (message.extd) {
			ESP_LOGI(TWAI_TAG, "Message is in Extended Format");
		}

		switch (message.identifier) {
			case BUTTONS_ID:
				if (enabled) {
					can::Buttons buttons = can::convert<can::Buttons>(message.data, message.data_length_code);

					static MultiPurposeButton button_forward(avrcp::play_pause, avrcp::forward);
					button_forward.update(buttons.forward);

					static MultiPurposeButton button_backward(nullptr, avrcp::backward);
					button_backward.update(buttons.backward);

					// @TODO Figure out what we want to do with the scroll button
					// Fast foward only appears to work in jellyfin and only skips 5 seconds
					// The scrolling also seems very unresponsive
					// So yeah...
					static uint8_t scroll = 0;
					if (scroll != buttons.scroll) {
						scroll = buttons.scroll;
						ESP_LOGI(TWAI_TAG, "Scroll changed: 0x%X", buttons.scroll);
					}

					// If the volume is syncing make sure we can cancel it if we press the volume buttons in the car
					if (buttons.volume_up || buttons.volume_down) {
						volume_controller::cancel_sync();
					}
				}
				break;

			case VOLUME_ID:
				if (enabled) {
					can::Volume volume = can::convert<can::Volume>(message.data, message.data_length_code);
					// Only update the volume if the volume has actually changed
					volume_controller::set_from_radio(volume.volume);
				}
				break;

			case RADIO_ID:
				{
					can::Radio radio = can::convert<can::Radio>(message.data, message.data_length_code);
					bool previous = enabled;
					enabled = (radio.source == can::Source::AUX2) && (radio.enabled);

					// If we just changed into the disabled state => pause
					if (!enabled && previous) {
						avrcp::pause();
					}

					static bool muted = false;
					static bool was_playing = false;
					// If we just muted => pause
					if (!muted && radio.muted) {
						was_playing = avrcp::is_playing();
						avrcp::pause();
					}

					// If we just unmuted and were playing before muting => unpause
					if (muted && !radio.muted && was_playing) {
						avrcp::play();
					}
					muted = radio.muted;

					// @TODO Figure out how all of this works when we receive a call
					// If I remember correctly when receiving a call, the radio muted the input
					// In which case it should auto resume playing after finishing the call
					// However the phone probably automatically pauses and unpauses the music during a call.
					// So we probably don't really have to do anything here.
				}
				break;


			default:
				break;
		}
	}
}

void twai::init() {
	twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_19, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
	twai_filter_config_t f_config = {
		.acceptance_code = (0b100100101 << 5) + (0b1000011111 << 21),
		.acceptance_mask = (0b011000000 << 5) + 0b11111 + (0b11111 << 16),
		.single_filter = false
	};

	if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
		ESP_LOGI(TWAI_TAG, "Driver installed");
	} else {
		ESP_LOGI(TWAI_TAG, "Failed to install the driver");
		return;
	}

	if (twai_start() == ESP_OK) {
		ESP_LOGI(TWAI_TAG, "Driver started");
	} else {
		ESP_LOGI(TWAI_TAG, "Failed to start driver");
	}

	xTaskCreatePinnedToCore(listen, "TWAI Listener", 2048, nullptr, 0, nullptr, 1);
}
