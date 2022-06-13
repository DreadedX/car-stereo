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

void play_task(void*) {
	for (;;) {
		esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_PRESSED);
		ESP_ERROR_CHECK(ret);

		ret = esp_avrc_ct_send_passthrough_cmd(1, ESP_AVRC_PT_CMD_PAUSE, ESP_AVRC_PT_CMD_STATE_RELEASED);
		ESP_ERROR_CHECK(ret);

		vTaskDelay(2000 / portTICK_PERIOD_MS);

		ret = esp_avrc_ct_send_passthrough_cmd(0, ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_PRESSED);
		ESP_ERROR_CHECK(ret);

		ret = esp_avrc_ct_send_passthrough_cmd(1, ESP_AVRC_PT_CMD_PLAY, ESP_AVRC_PT_CMD_STATE_RELEASED);
		ESP_ERROR_CHECK(ret);

		vTaskDelay(2000 / portTICK_PERIOD_MS);
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

	/* xTaskCreate(play_task, "Play task", 2048, nullptr, 0, nullptr); */

	can::init();

	/* uint8_t button_buffer[3] = { 0b01000110, 22, 0x00 }; */
	/* can::Buttons buttons = *(can::Buttons*)button_buffer; */
	/* print_buttons(buttons); */

	/* uint8_t radio_buffer[4] = { 0b10100000, 0b01000000, 0b00110000, 0x00 }; */
	/* can::Radio radio = *(can::Radio*)radio_buffer; */
	/* print_radio(radio); */
}

