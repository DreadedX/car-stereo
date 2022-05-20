#include "esp_log.h"
#include "esp_avrc_api.h"

#include "avrcp.h"
#include "helper.h"

#define AVRCP_TAG "APP_AVRCP"

static uint8_t volume = 127;
static void rc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) {
	ESP_LOGD(AVRCP_TAG, "%s evt %d", __func__, event);

	switch (event) {
		case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
			ESP_LOGI(AVRCP_TAG, "AVRC set absolute volume: %d%%", (int)param->set_abs_vol.volume * 100/ 0x7f);
			// The phone want to change the volume
			break;

		case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
			ESP_LOGI(AVRCP_TAG, "AVRC register event notification: %d, param: 0x%x", param->reg_ntf.event_id, param->reg_ntf.event_parameter);
			if (param->reg_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
				ESP_LOGI(AVRCP_TAG, "AVRC Volume Changes Supported");

				// Notify the phone of the current volume
				// @TODO This does not appear to set the volume correctly on my OnePlus 7T Pro
				// However for now it is not really relevant as the volume control is doen by the car
				// In the future however it might be nice to sync the value on the phone, the esp and the car
				// This will require some sort of CAN bus access
				esp_avrc_rn_param_t rn_param;
				rn_param.volume = volume;
				esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rn_param);
			} else {
				ESP_LOGW(AVRCP_TAG, "AVRC Volume Changes NOT Supported");
			}
			break;

		default:
			ESP_LOGE(AVRCP_TAG, "%s unhandled evt %d", __func__, event);
			break;
	}
}

void avrcp::init() {
	ESP_LOGI(AVRCP_TAG, "Initializing AVRCP");

	// Initialize AVRCP target
	if (esp_avrc_tg_init() == ESP_OK) {
		esp_avrc_tg_register_callback(rc_tg_callback);
		esp_avrc_rn_evt_cap_mask_t evt_set = {0};
		esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
		if (esp_avrc_tg_set_rn_evt_cap(&evt_set) != ESP_OK) {
			ESP_LOGE(AVRCP_TAG, "esp_avrc_tg_set_rn_evt_cap failed");
		}
	} else {
		ESP_LOGE(AVRCP_TAG, "esp_avrc_tg_init failed");
	}
}

uint8_t avrcp::get_volume() {
	return volume;
}

void avrcp::set_volume(uint8_t v) {
	volume = v;
	esp_avrc_rn_param_t rn_param;
	rn_param.volume = volume;
	esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_CHANGED, &rn_param);
}
