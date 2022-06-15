#include <cmath>

#include "esp_avrc_api.h"
#include "esp_log.h"

#include "esp_system.h"

#include "freertos/portmacro.h"
#include "storage.h"
#include "i2s.h"
#include "bluetooth.h"
#include "avrcp.h"
#include "a2dp.h"
#include "can.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define APP_TAG "APP"

void task(void*) {
	for (;;) {
		vTaskDelay(5000 / portTICK_PERIOD_MS);

		static uint8_t volume = 0;
		volume = (volume + 1) % 31;

		ESP_LOGI(APP_TAG, "Emulation radio with volume: %i", volume);
		avrcp::set_volume(ceil(volume * 4.2f));
	}
}

extern "C" void app_main() {
	ESP_LOGI(APP_TAG, "Starting Car Stereo");
	ESP_LOGI(APP_TAG, "Available Heap: %u", esp_get_free_heap_size());

	nvs::init();
	i2s::init();
	bluetooth::init();

	avrcp::init();
	a2dp::init();

	a2dp::connect_to_last();

	/* xTaskCreate(task, "Task", 2048, nullptr, 0, nullptr); */

	/* can::init(); */
}

