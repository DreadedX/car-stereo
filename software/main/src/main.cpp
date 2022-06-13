#include "esp_log.h"

#include "esp_system.h"

#include "storage.h"
#include "i2s.h"
#include "bluetooth.h"
#include "avrcp.h"
#include "a2dp.h"
#include "can.h"

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

	can::init();

	/* uint8_t button_buffer[3] = { 0b01000110, 22, 0x00 }; */
	/* can::Buttons buttons = *(can::Buttons*)button_buffer; */
	/* print_buttons(buttons); */

	/* uint8_t radio_buffer[4] = { 0b10100000, 0b01000000, 0b00110000, 0x00 }; */
	/* can::Radio radio = *(can::Radio*)radio_buffer; */
	/* print_radio(radio); */
}

