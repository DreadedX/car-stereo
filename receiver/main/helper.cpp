#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/i2s.h"
#include <cstring>

#include "helper.h"
#include "common.h"
#include "app.h"

const char* addr_to_str(esp_bd_addr_t bda) {
    static char bda_str[18];
    sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return (const char*)bda_str;
}

const char* connection_state_to_str(esp_a2d_connection_state_t state) {
	const char* states[4] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
	return states[state];
}

void set_scan_mode(bool connectable) {
	if (esp_bt_gap_set_scan_mode(connectable ? ESP_BT_CONNECTABLE : ESP_BT_NON_CONNECTABLE, connectable ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE)) {
		ESP_LOGE(BT_AV_TAG,"esp_bt_gap_set_scan_mode");
	}
}

void get_last_connection(esp_bd_addr_t last_connection) {
	nvs_handle handle;
	esp_err_t err = nvs_open("connected_dba", NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "NVS OPEN ERRRO");
	}

	esp_bd_addr_t bda;
	size_t size = sizeof(bda);
	err = nvs_get_blob(handle, "last_bda", bda, &size);
	if (err != ESP_OK) {
		if (err == ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGI(BT_AV_TAG, "nvs_blob does not exists");
		} else {
			ESP_LOGE(BT_AV_TAG, "get_nvs_blob failed");
		}
	}

	nvs_close(handle);
	if (err == ESP_OK) {
		memcpy(last_connection, bda, size);
	}
}

void set_last_connection(esp_bd_addr_t bda) {
	esp_bd_addr_t last_connection = {0,0,0,0,0,0};
	get_last_connection(last_connection);

	if (memcmp(bda, last_connection, ESP_BD_ADDR_LEN) == 0) {
		// Nothing has changed
		return;
	}

	nvs_handle handle;
	esp_err_t err = nvs_open("connected_dba", NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "NVS OPEN ERRRO");
	}

	err = nvs_set_blob(handle, "last_bda", bda, ESP_BD_ADDR_LEN);
	if (err == ESP_OK) {
		err = nvs_commit(handle);
	} else {
		ESP_LOGE(BT_AV_TAG, "NVS_WRITE_ERROR");
	}

	if (err != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "NVS COMMIT ERROR");
	}

	nvs_close(handle);
}

bool has_last_connection() {
	esp_bd_addr_t empty = {0,0,0,0,0,0};
	esp_bd_addr_t last_connection = {0,0,0,0,0,0};
	get_last_connection(last_connection);

	int result = memcmp(last_connection, empty, ESP_BD_ADDR_LEN);
	return result != 0;
}

void clean_last_connection() {
	esp_bd_addr_t empty = {0,0,0,0,0,0};
	set_last_connection(empty);
}

void play_wav(const uint8_t start[], const uint8_t end[]) {
	uint32_t len = (end - start - WAV_HEADER_SIZE);
	ESP_LOGI(BT_AV_TAG, "Playing sample...");

	// Switch to mono since the samples are all in mono to save space
	i2s_zero_dma_buffer(I2S_PORT);
	i2s_set_clk(I2S_PORT, 44100, 16, I2S_CHANNEL_MONO);

	for (uint32_t index = 0; index < len; index += I2S_BUFFER_SIZE) {
		int rest = len - index;

		size_t byte_size = I2S_BUFFER_SIZE;
		if (rest < I2S_BUFFER_SIZE) {
			byte_size = rest;
		}

		const unsigned char* current = start + WAV_HEADER_SIZE + index;
		size_t bytes_written;
		if (i2s_write(I2S_PORT, current, byte_size, &bytes_written, portMAX_DELAY) != ESP_OK) {
			ESP_LOGE(BT_AV_TAG, "i2s_write has failed");
		}
	}

	// Switch back to stereo
	i2s_zero_dma_buffer(I2S_PORT);
	i2s_set_clk(I2S_PORT, app::get_sample_rate(), 16, I2S_CHANNEL_STEREO);

	ESP_LOGI(BT_AV_TAG, "Done");
}

