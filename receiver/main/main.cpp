#include <cstdio>
#include <iostream>
#include <functional>
#include <cstring>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_avrc_api.h"
#include "esp_a2dp_api.h"
#include "driver/i2s.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "common.h"
#include "helper.h"
#include "app.h"

void init_nvs() {
    ESP_LOGI(BT_AV_TAG, "Initializing nvs");

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void init_i2s() {
    ESP_LOGI(BT_AV_TAG, "Initializing i2s");

	i2s_port_t i2s_port = I2S_PORT;

	i2s_config_t i2s_config = {
		.mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
		.sample_rate = app::get_sample_rate(),
		.bits_per_sample = (i2s_bits_per_sample_t)16,
		.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S),
		.intr_alloc_flags = 0, // default interrupt priority
		.dma_desc_num = 8,
		.dma_frame_num = 64,
		.use_apll = false,
		.tx_desc_auto_clear = true // avoiding noise in case of data unavailability
	};

	if (i2s_driver_install(i2s_port, &i2s_config, 0, nullptr) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "i2s_driver_install failed");
	}

	ESP_LOGI(BT_AV_TAG, "SAMPLE_RATE: %i", i2s_config.sample_rate);
	ESP_LOGI(BT_AV_TAG, "BITS_PER_SAMPLE: %i", i2s_config.bits_per_sample);

	i2s_pin_config_t pin_config = {
		.bck_io_num = 26,
		.ws_io_num = 25,
		.data_out_num = 33,
		.data_in_num = I2S_PIN_NO_CHANGE
	};

	if (i2s_set_pin(i2s_port, &pin_config) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "i2s_set_pin failed");
	}
}

bool bluetooth_start() {
	ESP_LOGI(BT_APP_TAG, "BT Start");
	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
		ESP_LOGI(BT_APP_TAG, "Already enabled");
		return true;
	}

	esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
		ESP_LOGI(BT_APP_TAG, "Idle");
		esp_bt_controller_init(&cfg);
		while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {}
	}

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
		if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) {
			ESP_LOGE(BT_APP_TAG, "BT Enable failed");
			return false;
		}
	}

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
		ESP_LOGI(BT_APP_TAG, "Enabled");
		return true;
	}

	ESP_LOGE(BT_APP_TAG, "BT Start failed");
	return false;
}

void app_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
	switch (event) {
		case ESP_BT_GAP_AUTH_CMPL_EVT: {
			if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
				ESP_LOGI(BT_AV_TAG, "authentication success: %s", param->auth_cmpl.device_name);
			} else {
				ESP_LOGE(BT_AV_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
			}
			break;
		}

		case ESP_BT_GAP_PIN_REQ_EVT: {
			esp_bd_addr_t peer_bd_addr = {0,0,0,0,0,0};
			memcpy(peer_bd_addr, param->pin_req.bda, ESP_BD_ADDR_LEN);
			ESP_LOGI(BT_AV_TAG, "partner address: %s", addr_to_str(peer_bd_addr));
			break;
		}

		case ESP_BT_GAP_CFM_REQ_EVT: {
			esp_bd_addr_t peer_bd_addr = {0,0,0,0,0,0};
			memcpy(peer_bd_addr, param->cfm_req.bda, ESP_BD_ADDR_LEN);
			ESP_LOGI(BT_AV_TAG, "partner address: %s", addr_to_str(peer_bd_addr));

			ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please confirm the passkey: %d", param->cfm_req.num_val);
			break;
		}

		case ESP_BT_GAP_KEY_NOTIF_EVT: {
			ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
			break;
		}

		case ESP_BT_GAP_KEY_REQ_EVT: {
			ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
			break;
		 }

		case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
			ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT stat:%d", param->read_rmt_name.stat);
			if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS ) {
				ESP_LOGI(BT_AV_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT remote name:%s", param->read_rmt_name.rmt_name);
				char remote_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
				memcpy(remote_name, param->read_rmt_name.rmt_name, ESP_BT_GAP_MAX_BDNAME_LEN );
			}
			break;
		}

		default: {
			ESP_LOGI(BT_AV_TAG, "%s invalid event: %d", __func__, event);
			break;
		}
	}
	return;
}

void init_bluetooth() {
	ESP_LOGI(BT_AV_TAG, "Initializing bluetooth");

	if (!bluetooth_start()) {
		ESP_LOGE(BT_AV_TAG, "Failed to initialize controller");
		return;
	}

	ESP_LOGI(BT_AV_TAG, "Controller initialized");

	esp_bluedroid_status_t bt_stack_status = esp_bluedroid_get_status();
	if (bt_stack_status == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
		if (esp_bluedroid_init() != ESP_OK) {
			ESP_LOGE(BT_AV_TAG, "Failed to initialize bluedroid");
			return;
		}
		ESP_LOGI(BT_AV_TAG, "Bluedroid initialized");
	}

	while (bt_stack_status != ESP_BLUEDROID_STATUS_ENABLED) {
		if (esp_bluedroid_enable() != ESP_OK) {
			ESP_LOGE(BT_AV_TAG, "Failed to enable bluedroid");
			vTaskDelay(100 / portTICK_PERIOD_MS);
		} else {
			ESP_LOGI(BT_AV_TAG, "Bluedroid enabled");
		}
		bt_stack_status = esp_bluedroid_get_status();
	}

	if (esp_bt_gap_register_callback(app_gap_callback) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG,"gap register failed");
		return;
	}

	// @TODO Do we need this?
	// Check menuconfig
	if ((esp_spp_init(ESP_SPP_MODE_CB)) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG,"esp_spp_init failed");
		return;
	}
}

extern "C" {
	void app_main();
}

void app_main() {
	ESP_LOGI(BT_AV_TAG, "Starting Car Stereo");
	ESP_LOGI(BT_AV_TAG, "Available Heap: %u", esp_get_free_heap_size());

	init_nvs();
	init_i2s();
	init_bluetooth();

	app::start();

	esp_bd_addr_t last_connection = {0,0,0,0,0,0};
	get_last_connection(last_connection);

	ESP_LOGI(BT_AV_TAG, "Last connection: %s", addr_to_str(last_connection));

	/* extern const uint8_t connect_wav_start[] asm("_binary_connect_wav_start"); */
	/* extern const uint8_t connect_wav_end[] asm("_binary_connect_wav_end"); */

	/* extern const uint8_t disconnect_wav_start[] asm("_binary_disconnect_wav_start"); */
	/* extern const uint8_t disconnect_wav_end[] asm("_binary_disconnect_wav_end"); */

	/* play_wav(connect_wav_start, connect_wav_end); */
	/* vTaskDelay(1000 / portTICK_PERIOD_MS); */
	/* play_wav(disconnect_wav_start, disconnect_wav_end); */
}
