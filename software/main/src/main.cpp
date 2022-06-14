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
		ESP_LOGI("TEST", "Pressing");

		static uint8_t cmd = ESP_AVRC_PT_CMD_PLAY;
		esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
		ESP_ERROR_CHECK(ret);
		ret = esp_avrc_ct_send_passthrough_cmd(1, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);
		ESP_ERROR_CHECK(ret);

		vTaskDelay(5000 / portTICK_PERIOD_MS);

		if (cmd == ESP_AVRC_PT_CMD_PLAY) {
			cmd = ESP_AVRC_PT_CMD_PAUSE;
		} else {
			cmd = ESP_AVRC_PT_CMD_PLAY;
		}
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

	can::init();
}

