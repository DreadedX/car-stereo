#include "esp_log.h"

#include "esp_system.h"

#include "storage.h"
#include "i2s.h"
#include "bluetooth.h"
#include "avrcp.h"
#include "a2dp.h"
#include "can.h"

#include "can_data.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define APP_TAG "APP"

static void print_buttons(can::Buttons buttons) {
	ESP_LOGI(APP_TAG, "BUTTONS");
	if (buttons.forward) {
		ESP_LOGI(APP_TAG, "Button: F");
	}
	if (buttons.backward) {
		ESP_LOGI(APP_TAG, "Button: B");
	}
	if (buttons.volume_up) {
		ESP_LOGI(APP_TAG, "Button: U");
	}
	if (buttons.volume_down) {
		ESP_LOGI(APP_TAG, "Button: D");
	}
	if (buttons.source) {
		ESP_LOGI(APP_TAG, "Button: S");
	}
	ESP_LOGI(APP_TAG, "Scroll: %i", buttons.scroll);
}

static void print_radio(can::Radio radio) {
	ESP_LOGI(APP_TAG, "RADIO");
	if (radio.enabled) {
		ESP_LOGI(APP_TAG, "Enabled: %i", radio.enabled);
	}
	if (radio.muted) {
		ESP_LOGI(APP_TAG, "Muted: %i", radio.muted);
	}
	if (radio.cd_changer_available) {
		ESP_LOGI(APP_TAG, "CD Changer: %i", radio.cd_changer_available);
	}
	switch (radio.disk_status) {
		case can::DiskStatus::Init:
			ESP_LOGI(APP_TAG, "CD: Init");
			break;
		case can::DiskStatus::Unavailable:
			ESP_LOGI(APP_TAG, "CD: Unavailable");
			break;
		case can::DiskStatus::Available:
			ESP_LOGI(APP_TAG, "CD: Available");
			break;
		default:
			ESP_LOGW(APP_TAG, "CD: Invalid");
			break;
	}

	switch (radio.source) {
		case can::Source::Bluetooth:
			ESP_LOGI(APP_TAG, "Source: Bluetooth");
			break;
		case can::Source::USB:
			ESP_LOGI(APP_TAG, "Source: USB");
			break;
		case can::Source::AUX2:
			ESP_LOGI(APP_TAG, "Source: AUX2");
			break;
		case can::Source::AUX1:
			ESP_LOGI(APP_TAG, "Source: AUX1");
			break;
		case can::Source::CD_Changer:
			ESP_LOGI(APP_TAG, "Source: CD Changer");
			break;
		case can::Source::CD:
			ESP_LOGI(APP_TAG, "Source: CD");
			break;
		case can::Source::Tuner:
			ESP_LOGI(APP_TAG, "Source: Tuner");
			break;
		default:
			ESP_LOGW(APP_TAG, "Source: Invalid");
			break;
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

	can::init();

	/* uint8_t button_buffer[3] = { 0b01000110, 22, 0x00 }; */
	/* can::Buttons buttons = *(can::Buttons*)button_buffer; */
	/* print_buttons(buttons); */

	/* uint8_t radio_buffer[4] = { 0b10100000, 0b01000000, 0b00110000, 0x00 }; */
	/* can::Radio radio = *(can::Radio*)radio_buffer; */
	/* print_radio(radio); */
}

