#include "esp_log.h"
#include "esp_avrc_api.h"

#include "avrcp.h"
#include "helper.h"

#define AVRCP_TAG "APP_AVRCP"

static esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;
static esp_avrc_playback_stat_t playback_status = ESP_AVRC_PLAYBACK_STOPPED;

static uint8_t volume = 127;
static _lock_t volume_lock;
static bool volume_notify = false;

static void playback_changed() {
	if (esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_TEST, &s_avrc_peer_rn_cap, ESP_AVRC_RN_PLAY_STATUS_CHANGE)) {
		esp_avrc_ct_send_register_notification_cmd(1, ESP_AVRC_RN_PLAY_STATUS_CHANGE, 0);
	}
}

static void notify_handler(uint8_t event_id, esp_avrc_rn_param_t *event_parameter) {
	switch (event_id) {
		case ESP_AVRC_RN_PLAY_STATUS_CHANGE:
			ESP_LOGI(AVRCP_TAG, "Playback status changed: 0x%x", event_parameter->playback);
			playback_status = event_parameter->playback;
			playback_changed();
			break;

		default:
			ESP_LOGI(AVRCP_TAG, "unhandled event: %d", event_id);
			break;
	}
}

static void set_volume_value(uint8_t v) {
	ESP_LOGI(AVRCP_TAG, "Setting internal volume value to %i", v);
	_lock_acquire(&volume_lock);
	volume = v;
	_lock_release(&volume_lock);
}

static void rc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param) {
	ESP_LOGD(AVRCP_TAG, "%s event %d", __func__, event);

	switch (event) {
		case ESP_AVRC_CT_CONNECTION_STATE_EVT: {
			uint8_t *bda = param->conn_stat.remote_bda;
			ESP_LOGI(AVRCP_TAG, "AVRC conn_state event: state %d, [%02x:%02x:%02x:%02x:%02x:%02x]",
					param->conn_stat.connected, bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);

			if (param->conn_stat.connected) {
				/* get remote supported event_ids of peer AVRCP Target */
				esp_avrc_ct_send_get_rn_capabilities_cmd(0);
			} else {
				/* clear peer notification capability record */
				s_avrc_peer_rn_cap.bits = 0;
			}
			break;
		}

		case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
			notify_handler(param->change_ntf.event_id, &param->change_ntf.event_parameter);
			break;

		case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
			ESP_LOGI(AVRCP_TAG, "remote rn_cap: count %d, bitmask 0x%x", param->get_rn_caps_rsp.cap_count, param->get_rn_caps_rsp.evt_set.bits);
			s_avrc_peer_rn_cap.bits = param->get_rn_caps_rsp.evt_set.bits;
			playback_changed();
			break;

		default:
			ESP_LOGE(AVRCP_TAG, "%s unhandled event %d", __func__, event);
			break;
	}
}

static void rc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t* param) {
	ESP_LOGD(AVRCP_TAG, "%s event %d", __func__, event);

	switch (event) {
		case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
			ESP_LOGI(AVRCP_TAG, "AVRC set absolute volume: %d%%", (int)param->set_abs_vol.volume * 100/ 0x7f);
			// The phone want to change the volume
			set_volume_value(param->set_abs_vol.volume);
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
				volume_notify = true;
				esp_avrc_rn_param_t rn_param;
				rn_param.volume = volume;
				esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rn_param);
			} else if (param->reg_ntf.event_id == ESP_AVRC_RN_PLAY_POS_CHANGED) {
				ESP_LOGI(AVRCP_TAG, "We can change the play position?");
			} else {
				ESP_LOGI(AVRCP_TAG, "Something else!");
			}
			break;

		default:
			ESP_LOGE(AVRCP_TAG, "%s unhandled event %d", __func__, event);
			break;
	}
}

void avrcp::init() {
	ESP_LOGI(AVRCP_TAG, "Initializing AVRCP");

	if (esp_avrc_ct_init() == ESP_OK) {
		esp_avrc_ct_register_callback(rc_ct_callback);
	}

	// Initialize AVRCP target
	if (esp_avrc_tg_init() == ESP_OK) {
		{
			esp_avrc_tg_register_callback(rc_tg_callback);
			esp_avrc_rn_evt_cap_mask_t evt_set = {0};
			esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
			if (esp_avrc_tg_set_rn_evt_cap(&evt_set) != ESP_OK) {
				ESP_LOGE(AVRCP_TAG, "esp_avrc_tg_set_rn_evt_cap failed");
			}
		}

		/* { */
		/* 	esp_avrc_tg_register_callback(rc_tg_callback); */
		/* 	esp_avrc_rn_evt_cap_mask_t evt_set = {0}; */
		/* 	esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_PLAY_POS_CHANGED); */
		/* 	if (esp_avrc_tg_set_rn_evt_cap(&evt_set) != ESP_OK) { */
		/* 		ESP_LOGE(AVRCP_TAG, "esp_avrc_tg_set_rn_evt_cap failed"); */
		/* 	} */
		/* } */
	} else {
		ESP_LOGE(AVRCP_TAG, "esp_avrc_tg_init failed");
	}
}

bool avrcp::is_playing() {
	return playback_status == ESP_AVRC_PLAYBACK_PLAYING;
}

static void send_cmd(esp_avrc_pt_cmd_t cmd) {
	esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
	ESP_ERROR_CHECK(ret);

	ret = esp_avrc_ct_send_passthrough_cmd(1, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);
	ESP_ERROR_CHECK(ret);
}

void avrcp::play_pause() {
	if (is_playing()) {
		ESP_LOGI(AVRCP_TAG, "Pausing");
		send_cmd(ESP_AVRC_PT_CMD_PAUSE);
	} else {
		ESP_LOGI(AVRCP_TAG, "Playing");
		send_cmd(ESP_AVRC_PT_CMD_PLAY);
	}
}

void avrcp::forward() {
	ESP_LOGI(AVRCP_TAG, "Forward");

	send_cmd(ESP_AVRC_PT_CMD_FORWARD);
}

void avrcp::backward() {
	ESP_LOGI(AVRCP_TAG, "Backward");

	send_cmd(ESP_AVRC_PT_CMD_BACKWARD);
}

void avrcp::set_volume(uint8_t v) {
	set_volume_value(v);

	if (volume_notify) {
		ESP_LOGI(AVRCP_TAG, "Setting remote volume value to %i", v);
		esp_avrc_rn_param_t rn_param;
		rn_param.volume = volume;
		esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_CHANGED, &rn_param);
		volume_notify = false;
	}
}

