#include <cmath>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "volume.h"
#include "avrcp.h"
#include "twai.h"

#define VOLUME_TAG "APP_VOLUME"

// 0-127
static uint8_t volume;
// 0-127
static uint8_t remote_volume;
// 0-30
static uint8_t radio_volume;

static bool synced = false;
static _lock_t lock;

void volume_controller::set_from_radio(int v) {
	ESP_LOGI(VOLUME_TAG, "Volume on radio updated: %i (0-30)", v);
	// Update the radio volume
	_lock_acquire(&lock);
	radio_volume = v;
	_lock_release(&lock);

	if (!synced) {
		ESP_LOGI(VOLUME_TAG, "Not updating internal and remote (SYNCING)");
		// In this case we are still adjusting the volume of the car to match the remote/internal volume
		// So we do not want to update these values based on the radio
		return;
	}

	// Convert the 0 - 30 range of the radio to 0 - 127
	uint8_t full_range = ceil(v * 4.2f);
	ESP_LOGI(VOLUME_TAG, "Updating internal and remote to: %i (0-127)", full_range);

	// Update the remote volume
	avrcp::set_volume(full_range);

	// @TODO Somehow make sure we actually the remote volume is actually set before updating the value
	_lock_acquire(&lock);
	volume = full_range;
	remote_volume = full_range;
	_lock_release(&lock);
}

void volume_controller::set_from_remote(int v) {
	ESP_LOGI(VOLUME_TAG, "Volume on remote updated: %i (0-127)", v);

	_lock_acquire(&lock);
	remote_volume = v;
	volume = v;

	synced = false;
	_lock_release(&lock);
}

uint8_t volume_controller::current() {
	return volume;
}

static void correct_volume(void*) {
	for (;;) {
		if (!synced) {
			uint8_t target = floor(volume / 4.2f);

			if (radio_volume == target) {
				ESP_LOGI(VOLUME_TAG, "SYNCED!");
				_lock_acquire(&lock);
				synced = true;
				_lock_release(&lock);
			} else {
				ESP_LOGI(VOLUME_TAG, "Adjusting volume: %i", radio_volume < target);
				twai::change_volume(radio_volume < target);
			}
		}

		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void volume_controller::init() {
	xTaskCreatePinnedToCore(correct_volume, "Correct volume", 2048, nullptr, 0, nullptr, 1);
}
