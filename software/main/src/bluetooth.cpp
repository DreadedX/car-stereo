#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"

#include "bluetooth.h"
#include "leds.h"

#include "helper.h"
#include <cstring>

#define BT_TAG "APP_BT"

static bool start() {
	ESP_LOGI(BT_TAG, "BT Start");
	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
		ESP_LOGI(BT_TAG, "Already enabled");
		return true;
	}

	esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
		ESP_LOGI(BT_TAG, "Idle");
		esp_bt_controller_init(&cfg);
		while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {}
	}

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
		if (esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) {
			ESP_LOGE(BT_TAG, "BT Enable failed");
			return false;
		}
	}

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
		ESP_LOGI(BT_TAG, "Enabled");
		return true;
	}

	ESP_LOGE(BT_TAG, "BT Start failed");
	return false;
}

static void app_gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param) {
	switch (event) {
		case ESP_BT_GAP_AUTH_CMPL_EVT: {
			if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
				ESP_LOGI(BT_TAG, "authentication success: %s", param->auth_cmpl.device_name);
			} else {
				ESP_LOGE(BT_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
			}
			break;
		}

		case ESP_BT_GAP_PIN_REQ_EVT: {
			esp_bd_addr_t peer_bd_addr = {0,0,0,0,0,0};
			memcpy(peer_bd_addr, param->pin_req.bda, ESP_BD_ADDR_LEN);
			ESP_LOGI(BT_TAG, "partner address: %s", addr_to_str(peer_bd_addr));
			break;
		}

		case ESP_BT_GAP_CFM_REQ_EVT: {
			esp_bd_addr_t peer_bd_addr = {0,0,0,0,0,0};
			memcpy(peer_bd_addr, param->cfm_req.bda, ESP_BD_ADDR_LEN);
			ESP_LOGI(BT_TAG, "partner address: %s", addr_to_str(peer_bd_addr));

			ESP_LOGI(BT_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please confirm the passkey: %d", param->cfm_req.num_val);
			break;
		}

		case ESP_BT_GAP_KEY_NOTIF_EVT: {
			ESP_LOGI(BT_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%d", param->key_notif.passkey);
			break;
		}

		case ESP_BT_GAP_KEY_REQ_EVT: {
			ESP_LOGI(BT_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
			break;
		 }

		case ESP_BT_GAP_READ_REMOTE_NAME_EVT: {
			ESP_LOGI(BT_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT stat:%d", param->read_rmt_name.stat);
			if (param->read_rmt_name.stat == ESP_BT_STATUS_SUCCESS ) {
				ESP_LOGI(BT_TAG, "ESP_BT_GAP_READ_REMOTE_NAME_EVT remote name:%s", param->read_rmt_name.rmt_name);
				char remote_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
				memcpy(remote_name, param->read_rmt_name.rmt_name, ESP_BT_GAP_MAX_BDNAME_LEN );
			}
			break;
		}

		default: {
			ESP_LOGI(BT_TAG, "%s invalid event: %d", __func__, event);
			break;
		}
	}
	return;
}

void bluetooth::init() {
	ESP_LOGI(BT_TAG, "Initializing bluetooth");

	if (!start()) {
		ESP_LOGE(BT_TAG, "Failed to initialize controller");
		return;
	}

	ESP_LOGI(BT_TAG, "Controller initialized");

	esp_bluedroid_status_t bt_stack_status = esp_bluedroid_get_status();
	if (bt_stack_status == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
		if (esp_bluedroid_init() != ESP_OK) {
			ESP_LOGE(BT_TAG, "Failed to initialize bluedroid");
			return;
		}
		ESP_LOGI(BT_TAG, "Bluedroid initialized");
	}

	while (bt_stack_status != ESP_BLUEDROID_STATUS_ENABLED) {
		if (esp_bluedroid_enable() != ESP_OK) {
			ESP_LOGE(BT_TAG, "Failed to enable bluedroid");
			vTaskDelay(pdMS_TO_TICKS(100));
		} else {
			ESP_LOGI(BT_TAG, "Bluedroid enabled");
		}
		bt_stack_status = esp_bluedroid_get_status();
	}

	if (esp_bt_gap_register_callback(app_gap_callback) != ESP_OK) {
		ESP_LOGE(BT_TAG,"gap register failed");
		return;
	}

	esp_bt_dev_set_device_name("207 Music");

	esp_bt_pin_code_t pin_code;
	pin_code[0] = '6';
	pin_code[1] = '9';
	pin_code[2] = '4';
	pin_code[3] = '2';
	pin_code[4] = '0';
	esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 5, pin_code);
}

void bluetooth::set_scan_mode(bool connectable) {
	if (esp_bt_gap_set_scan_mode(connectable ? ESP_BT_CONNECTABLE : ESP_BT_NON_CONNECTABLE, connectable ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE)) {
		ESP_LOGE(BT_TAG,"esp_bt_gap_set_scan_mode failed");
		return;
	}

	if (connectable) {
		leds::set_bluetooth(leds::Bluetooth::DISCOVERABLE);
	}
}


