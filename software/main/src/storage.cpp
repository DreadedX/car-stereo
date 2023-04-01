#include <cstring>

#include "esp_log.h"
#include "nvs_flash.h"

#include "storage.h"

#define NVS_TAG "APP_NVS"

void nvs::init() {
    ESP_LOGI(NVS_TAG, "Initializing nvs");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void nvs::get_last_connection(esp_bd_addr_t last_connection) {
	nvs_handle handle;
	esp_err_t err = nvs_open("stereo", NVS_READONLY, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "NVS OPEN ERRRO");
	}

	esp_bd_addr_t bda;
	size_t size = sizeof(bda);
	err = nvs_get_blob(handle, "last_bda", bda, &size);
	if (err != ESP_OK) {
		if (err == ESP_ERR_NVS_NOT_FOUND) {
			ESP_LOGI(NVS_TAG, "nvs_blob does not exists");
		} else {
			ESP_LOGE(NVS_TAG, "get_nvs_blob failed");
		}
	}

	nvs_close(handle);
	if (err == ESP_OK) {
		memcpy(last_connection, bda, size);
	}
}

void nvs::set_last_connection(esp_bd_addr_t bda) {
	esp_bd_addr_t last_connection = {0,0,0,0,0,0};
	get_last_connection(last_connection);

	if (memcmp(bda, last_connection, ESP_BD_ADDR_LEN) == 0) {
		// Nothing has changed
		return;
	}

	nvs_handle handle;
	esp_err_t err = nvs_open("stereo", NVS_READWRITE, &handle);
	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "NVS OPEN ERRRO");
	}

	err = nvs_set_blob(handle, "last_bda", bda, ESP_BD_ADDR_LEN);
	if (err == ESP_OK) {
		err = nvs_commit(handle);
	} else {
		ESP_LOGE(NVS_TAG, "NVS_WRITE_ERROR");
	}

	if (err != ESP_OK) {
		ESP_LOGE(NVS_TAG, "NVS COMMIT ERROR");
	}

	nvs_close(handle);
}

bool nvs::has_last_connection() {
	esp_bd_addr_t empty = {0,0,0,0,0,0};
	esp_bd_addr_t last_connection = {0,0,0,0,0,0};
	get_last_connection(last_connection);

	int result = memcmp(last_connection, empty, ESP_BD_ADDR_LEN);
	return result != 0;
}
