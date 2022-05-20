#include "esp_log.h"

#include "esp_system.h"

#include "storage.h"
#include "i2s.h"
#include "bluetooth.h"
#include "avrcp.h"
#include "a2dp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define APP_TAG "APP"

extern "C" void app_main() {
	ESP_LOGI(APP_TAG, "Starting Car Stereo");
	ESP_LOGI(APP_TAG, "Available Heap: %u", esp_get_free_heap_size());

	nvs::init();
	i2s::init();
	bluetooth::init();

	avrcp::init();
	a2dp::init();

	a2dp::connect_to_last();

	vTaskDelay(10000 / portTICK_PERIOD_MS);
	ESP_LOGI(APP_TAG, "Changing volume");
	avrcp::set_volume(127/2);
}

