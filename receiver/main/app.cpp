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

QueueHandle_t queue = nullptr;
TaskHandle_t handle = nullptr;
esp_avrc_rn_evt_cap_mask_t s_avrc_peer_rn_cap;
int sample_rate = 44100;

uint32_t app::get_sample_rate() {
	return sample_rate;
}

void handler(void* args) {
	app::Message msg;
	for (;;) {
		if (!queue) {
			ESP_LOGE(BT_APP_TAG, "%s, app_task_queue is null", __func__);
			vTaskDelay(100 / portTICK_PERIOD_MS);
		} else if (xQueueReceive(queue, &msg, portMAX_DELAY)) {
			if (msg.callback) {
				msg.callback(msg.event, msg.param);
			}

			if (msg.param) {
				free(msg.param);
			}
		} else {
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
	}
}

// Not quite sure what this function is for, it seems to just add null termination to a string
/* // @TODO Pretty sure this also leaks memory, since we never free the original text before overwriting it */
/* void create_metadata_buffer(esp_avrc_ct_cb_param_t *param) { */
/* 	uint8_t* attr_text = (uint8_t*)malloc(param->meta_rsp.attr_length + 1); */
/* 	memcpy(attr_text, param->meta_rsp.attr_text, param->meta_rsp.attr_length); */
/* 	attr_text[param->meta_rsp.attr_length] = 0; */

/* 	param->meta_rsp.attr_text = attr_text; */
/* } */

// @TODO Not quite sure why this is called new track
void av_new_track() {
	esp_avrc_ct_send_metadata_cmd(0, ESP_AVRC_MD_ATTR_TITLE | ESP_AVRC_MD_ATTR_ARTIST | ESP_AVRC_MD_ATTR_PLAYING_TIME);
	// @TODO Pretty sure we can listen for play/pause and pos here ESP_AVRC_RN_PLAY_STATUS_CHANGE and ESP_AVRC_RN_PLAY_POS_CHANGED
	esp_avrc_ct_send_register_notification_cmd(1, ESP_AVRC_RN_TRACK_CHANGE, 0);
}

// @TODO Here we can implement the notifications that we are registered for
void av_notify_evt_handler(uint8_t& id, esp_avrc_rn_param_t& param) {
	switch (id) {
		case ESP_AVRC_RN_TRACK_CHANGE:
			ESP_LOGD(BT_AV_TAG, "%s ESP_AVRC_RN_TRACK_CHANGE %d", __func__, id);
			// @TODO Do we need to reregister it every time we get the event?
			av_new_track();
			break;

		default:
			ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, id);
			break;
	}
}

void av_hdl_avrc_evt(uint16_t event, void* param) {
	ESP_LOGI(BT_AV_TAG, "%s evt %d", __func__, event);

	esp_avrc_ct_cb_param_t* rc = (esp_avrc_ct_cb_param_t*)(param);

	switch (event) {
		case ESP_AVRC_CT_CONNECTION_STATE_EVT: 
			ESP_LOGI(BT_AV_TAG, "AVRC conn_state evt: state %d, [%s]", rc->conn_stat.connected, addr_to_str(rc->conn_stat.remote_bda));

			if (rc->conn_stat.connected) {
				av_new_track();
				// get remote supported event_ids of peer AVRCP Target
				esp_avrc_ct_send_get_rn_capabilities_cmd(0);
			} else {
				// clear peer notification capability record
				s_avrc_peer_rn_cap.bits = 0;
			}
			break;

		case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
			ESP_LOGI(BT_AV_TAG, "AVRC passthrough rsp: key_code 0x%x, key_state %d", rc->psth_rsp.key_code, rc->psth_rsp.key_state);
			break;

		case ESP_AVRC_CT_METADATA_RSP_EVT: 
			ESP_LOGI(BT_AV_TAG, "AVRC metadata rsp: attribute id 0x%x, %s", rc->meta_rsp.attr_id, rc->meta_rsp.attr_text);
			// @TODO Do something with the metadata
			break;

		case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
			//ESP_LOGI(BT_AV_TAG, "AVRC event notification: %d, param: %d", (int)rc->change_ntf.event_id, (int)rc->change_ntf.event_parameter);
			av_notify_evt_handler(rc->change_ntf.event_id, rc->change_ntf.event_parameter);
			break;

		case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
			ESP_LOGI(BT_AV_TAG, "AVRC remote features %x", rc->rmt_feats.feat_mask);
			break;

		case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
			ESP_LOGI(BT_AV_TAG, "remote rn_cap: count %d, bitmask 0x%x", rc->get_rn_caps_rsp.cap_count,
					rc->get_rn_caps_rsp.evt_set.bits);
			s_avrc_peer_rn_cap.bits = rc->get_rn_caps_rsp.evt_set.bits;
			av_new_track();
			break;

		default:
			ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
			break;
	}
}

void av_hdl_avrc_tg_evt(uint16_t event, void* p_param)
{
    ESP_LOGD(BT_AV_TAG, "%s evt %d", __func__, event);
    esp_avrc_tg_cb_param_t *rc = (esp_avrc_tg_cb_param_t *)(p_param);

    switch (event) {

    case ESP_AVRC_TG_CONNECTION_STATE_EVT: {
        ESP_LOGI(BT_AV_TAG, "AVRC conn_state evt: state %d, [%s]",rc->conn_stat.connected, addr_to_str(rc->conn_stat.remote_bda));
        break;
    }

    case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT: {
        ESP_LOGI(BT_AV_TAG, "AVRC passthrough cmd: key_code 0x%x, key_state %d", rc->psth_cmd.key_code, rc->psth_cmd.key_state);
        break;
    }

    case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT: {
        ESP_LOGI(BT_AV_TAG, "AVRC set absolute volume: %d%%", (int)rc->set_abs_vol.volume * 100/ 0x7f);
        /* volume_set_by_controller(rc->set_abs_vol.volume); */
        break;
    }

    case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT: {
        ESP_LOGI(BT_AV_TAG, "AVRC register event notification: %d, param: 0x%x", rc->reg_ntf.event_id, rc->reg_ntf.event_parameter);
        if (rc->reg_ntf.event_id == ESP_AVRC_RN_VOLUME_CHANGE) {
            ESP_LOGI(BT_AV_TAG, "AVRC Volume Changes Supported");
            /* s_volume_notify = true; */
            esp_avrc_rn_param_t rn_param;
            rn_param.volume = 128/2;
            esp_avrc_tg_send_rn_rsp(ESP_AVRC_RN_VOLUME_CHANGE, ESP_AVRC_RN_RSP_INTERIM, &rn_param);
        } else {
            ESP_LOGW(BT_AV_TAG, "AVRC Volume Changes NOT Supported");
        }
        break;
    }

    case ESP_AVRC_TG_REMOTE_FEATURES_EVT: {
        ESP_LOGI(BT_AV_TAG, "AVRC remote features %x, CT features %x", rc->rmt_feats.feat_mask, rc->rmt_feats.ct_feat_flag);
        break;
    }

    default:
        ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
        break;
    }
}



// @TODO This can be simplified a lot if we strip out the logging
void rc_ct_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t* param) {
	switch (event) {
		case ESP_AVRC_CT_METADATA_RSP_EVT:
		case ESP_AVRC_CT_CONNECTION_STATE_EVT:
		case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
		case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
		case ESP_AVRC_CT_REMOTE_FEATURES_EVT: 
		case ESP_AVRC_CT_GET_RN_CAPABILITIES_RSP_EVT:
			app::dispatch(av_hdl_avrc_evt, event, param, sizeof(esp_avrc_ct_cb_param_t));
			break;

		default:
			ESP_LOGE(BT_AV_TAG, "Invalid AVRC event: %d", event);
			break;
	}
}

void rc_tg_callback(esp_avrc_tg_cb_event_t event, esp_avrc_tg_cb_param_t *param)
{
	ESP_LOGD(BT_AV_TAG, "%s", __func__);
	switch (event) {
		case ESP_AVRC_TG_CONNECTION_STATE_EVT:
		case ESP_AVRC_TG_REMOTE_FEATURES_EVT:
		case ESP_AVRC_TG_PASSTHROUGH_CMD_EVT:
		case ESP_AVRC_TG_SET_ABSOLUTE_VOLUME_CMD_EVT:
		case ESP_AVRC_TG_REGISTER_NOTIFICATION_EVT:
		case ESP_AVRC_TG_SET_PLAYER_APP_VALUE_EVT:
			app::dispatch(av_hdl_avrc_tg_evt, event, param, sizeof(esp_avrc_tg_cb_param_t));
			break;
		default:
			ESP_LOGE(BT_AV_TAG, "Unsupported AVRC event: %d", event);
			break;
	}
}

void handle_connection_state(uint16_t event, esp_a2d_cb_param_t* a2d) {
	static int retry_count = 0;
	// determine remote BDA
	esp_bd_addr_t peer_bd_addr = {0,0,0,0,0,0};
	memcpy(peer_bd_addr, a2d->conn_stat.remote_bda, ESP_BD_ADDR_LEN);
	ESP_LOGI(BT_AV_TAG, "partner address: %s", addr_to_str(peer_bd_addr));

	// @TODO Here we can handle on connection state change things

	ESP_LOGI(BT_AV_TAG, "A2DP connection state: %s, [%s]", connection_state_to_str(a2d->conn_stat.state), addr_to_str(a2d->conn_stat.remote_bda));

	if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
		ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_DISCONNECTED");

		/* 	ESP_LOGI(BT_AV_TAG, "i2s_stop"); */
		/* 	i2s_stop(i2s_port); */
		/* 	i2s_zero_dma_buffer(i2s_port); */

		extern const uint8_t disconnect_wav_start[] asm("_binary_disconnect_wav_start");
		extern const uint8_t disconnect_wav_end[] asm("_binary_disconnect_wav_end");
		vTaskDelay(500 / portTICK_PERIOD_MS);
		play_wav(disconnect_wav_start, disconnect_wav_end);

		// @TODO Hope this works correctly
		if (a2d->conn_stat.disc_rsn == ESP_A2D_DISC_RSN_ABNORMAL && has_last_connection()) {
			if (retry_count < 3 ){
				ESP_LOGI(BT_AV_TAG,"Connection try number: %d", retry_count);

				esp_bd_addr_t last_connection = {0,0,0,0,0,0};
				get_last_connection(last_connection);
				if (esp_a2d_sink_connect(last_connection) == ESP_FAIL) {
					ESP_LOGE(BT_AV_TAG, "Failed connecting to device!");
				}
			} else {
				clean_last_connection();
				set_scan_mode(true);
			}
		} else {
			set_scan_mode(true);
		}
	} else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED){
		ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_CONNECTED");

		set_scan_mode(false);
		retry_count = 0;

		/* ESP_LOGI(BT_AV_TAG,"i2s_start"); */
		/* if (i2s_start(i2s_port)!=ESP_OK){ */
		/* 	ESP_LOGE(BT_AV_TAG, "i2s_start"); */
		/* } */

		extern const uint8_t connect_wav_start[] asm("_binary_connect_wav_start");
		extern const uint8_t connect_wav_end[] asm("_binary_connect_wav_end");
		play_wav(connect_wav_start, connect_wav_end);
		vTaskDelay(500 / portTICK_PERIOD_MS);

		// record current connection
		set_last_connection(a2d->conn_stat.remote_bda);
		if (esp_bt_gap_read_remote_name(a2d->conn_stat.remote_bda) != ESP_OK) {
			ESP_LOGE(BT_AV_TAG, "esp_bt_gap_read_remote_name failed");
		}


	} else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING){
		ESP_LOGI(BT_AV_TAG, "ESP_A2D_CONNECTION_STATE_CONNECTING");
		retry_count++;
	}
}

void handle_audio_state(uint16_t event, esp_a2d_cb_param_t* a2d) {
	if (a2d->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
		/* if (i2s_start(I2S_PORT) != ESP_OK){ */
		/* 	ESP_LOGE(BT_AV_TAG, "i2s_start failed"); */
		/* } */
	} else {
		/* i2s_stop(I2S_PORT); */
		/* i2s_zero_dma_buffer(I2S_PORT); */
	}
}

void handle_audio_cfg(uint16_t event, esp_a2d_cb_param_t* a2d) {
	sample_rate = 16000;

	ESP_LOGI(BT_AV_TAG, "a2dp audio_cfg_cb , codec type %d", a2d->audio_cfg.mcc.type);

	char oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
	if (oct0 & (0x01 << 6)) {
		sample_rate = 32000;
	} else if (oct0 & (0x01 << 5)) {
		sample_rate = 44100;
	} else if (oct0 & (0x01 << 4)) {
		sample_rate = 48000;
	}

	ESP_LOGI(BT_AV_TAG, "a2dp audio_cfg_cb , sample_rate %d", sample_rate);

	if (i2s_set_clk(I2S_PORT, sample_rate, 16, I2S_CHANNEL_STEREO) != ESP_OK){
		ESP_LOGE(BT_AV_TAG, "i2s_set_clk failed with samplerate=%d", sample_rate);
	} else {
		ESP_LOGI(BT_AV_TAG, "audio player configured, samplerate=%d", sample_rate);
	}
}

void av_hdl_a2d_evt(uint16_t event, void* param) {
	esp_a2d_cb_param_t* a2d = (esp_a2d_cb_param_t*)(param);
	switch (event) {
		case ESP_A2D_CONNECTION_STATE_EVT:
			ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_CONNECTION_STATE_EVT", __func__);
			handle_connection_state(event, a2d);
			break;

		case ESP_A2D_AUDIO_STATE_EVT:
			ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_AUDIO_STATE_EVT", __func__);
			handle_audio_state(event, a2d);
			break;

		case ESP_A2D_AUDIO_CFG_EVT:
			ESP_LOGD(BT_AV_TAG, "%s ESP_A2D_AUDIO_CFG_EVT", __func__);
			handle_audio_cfg(event, a2d);
			break;

		case ESP_A2D_PROF_STATE_EVT:
			if (ESP_A2D_INIT_SUCCESS == a2d->a2d_prof_stat.init_state) {
				ESP_LOGI(BT_AV_TAG,"A2DP PROF STATE: Init Compl\n");
			} else {
				ESP_LOGI(BT_AV_TAG,"A2DP PROF STATE: Deinit Compl\n");
			}
			break;

		default:
			ESP_LOGE(BT_AV_TAG, "%s unhandled evt %d", __func__, event);
			break;
	}
}

void a2d_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* param) {
	switch (event) {
		case ESP_A2D_CONNECTION_STATE_EVT:
		case ESP_A2D_AUDIO_STATE_EVT:
		case ESP_A2D_AUDIO_CFG_EVT:
		case ESP_A2D_PROF_STATE_EVT:
			app::dispatch(av_hdl_a2d_evt, event, param, sizeof(esp_a2d_cb_param_t));
			break;

		default:
			ESP_LOGE(BT_AV_TAG, "Invalid A2DP event: %d", event);
			break;
	}
}

/**
 * @brief Utility structure that can be used to split a int32_t up into 2 separate channels with int16_t data.
 * @author Phil Schatzmann
 * @copyright Apache License Version 2
 */
struct __attribute__((packed)) Frame {
  int16_t channel1;
  int16_t channel2;

  Frame(int v=0){
    channel1 = channel2 = v;
  }

  Frame(int ch1, int ch2){
    channel1 = ch1;
    channel2 = ch2;
  }

};

void audio_data_callback(const uint8_t* data, uint32_t len) {
	Frame* frame = (Frame*)data;
	for (int i = 0; i < len/4; i++) {
		int16_t temp = frame[i].channel1;
		frame[i].channel1 = frame[i].channel2;
		frame[i].channel2 = temp;
	}

	size_t bytes_written;
	if (i2s_write(I2S_PORT, data, len, &bytes_written, portMAX_DELAY) != ESP_OK) {
		ESP_LOGE(BT_AV_TAG, "i2s_write has failed");
	}

	if (bytes_written < len) {
		ESP_LOGE(BT_AV_TAG, "Timeout: not all bytes were written to I2S");
	}
}

void setup_stack(uint16_t event, void* param) {
	ESP_LOGI(BT_APP_TAG, "Setting up stack");

	esp_bt_dev_set_device_name("Car Stereo");

	// Initialize AVRCP controller
	esp_err_t result = esp_avrc_ct_init();
	if (result == ESP_OK) {
		result = esp_avrc_ct_register_callback(rc_ct_callback);
		if (result == ESP_OK) {
			ESP_LOGI(BT_AV_TAG, "AVRCP controller initialized");
		} else {
			ESP_LOGE(BT_AV_TAG, "esp_avrc_ct_register_callback: %d", result);
		}
	} else {
		ESP_LOGE(BT_AV_TAG, "esp_avrc_ct_init: %d", result);
	}

	// Initialize AVRCP target
	if (esp_avrc_tg_init() == ESP_OK) {
		esp_avrc_tg_register_callback(rc_tg_callback);
		esp_avrc_rn_evt_cap_mask_t evt_set = {0};
		esp_avrc_rn_evt_bit_mask_operation(ESP_AVRC_BIT_MASK_OP_SET, &evt_set, ESP_AVRC_RN_VOLUME_CHANGE);
		if (esp_avrc_tg_set_rn_evt_cap(&evt_set) != ESP_OK) {
			ESP_LOGE(BT_AV_TAG, "esp_avrc_tg_set_rn_evt_cap failed");
		}
	} else {
		ESP_LOGE(BT_AV_TAG, "esp_avrc_tg_init failed");
	}

	// Initialize A2DP sink
	if (esp_a2d_register_callback(a2d_callback) != ESP_OK){
		ESP_LOGE(BT_AV_TAG,"esp_a2d_register_callback failed");
	}
	if (esp_a2d_sink_register_data_callback(audio_data_callback) != ESP_OK){
		ESP_LOGE(BT_AV_TAG,"esp_a2d_sink_register_data_callback failed");
	}
	if (esp_a2d_sink_init() != ESP_OK){
		ESP_LOGE(BT_AV_TAG,"esp_a2d_sink_init failed");
	}

	if (has_last_connection()) {
		esp_bd_addr_t last_connection = {0,0,0,0,0,0};
		get_last_connection(last_connection);
		if (esp_a2d_sink_connect(last_connection) == ESP_FAIL) {
			ESP_LOGE(BT_AV_TAG, "Failed connecting to device!");
		}
	}

	set_scan_mode(true);
}

void app::start() {
	ESP_LOGI(BT_AV_TAG, "Starting app task");

	if (queue == nullptr) {
		queue = xQueueCreate(10, sizeof(Message));
		ESP_LOGI(BT_AV_TAG, "Created queue");
	}

	if (handle == nullptr) {
		if (xTaskCreate(handler, "BtAppT", 2048, nullptr, configMAX_PRIORITIES - 3, &handle) != pdPASS) {
			ESP_LOGE(BT_APP_TAG, "Failed to create app task");
		}
		ESP_LOGI(BT_AV_TAG, "Created task");
	}

	dispatch(setup_stack, 0, nullptr, 0);

	esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
	esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;
	esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

	esp_bt_pin_code_t pin_code;
	pin_code[0] = '6';
	pin_code[1] = '9';
	pin_code[2] = '4';
	pin_code[3] = '2';
	pin_code[4] = '0';
	esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 5, pin_code);
}

/* void app::stop() { */
/* 	ESP_LOGI(BT_AV_TAG, "Destroying task app task"); */

/* 	if (handle != nullptr) { */
/* 		vTaskDelete(handle); */
/* 		handle = nullptr; */
/* 	} */

/* 	if (queue != nullptr) { */
/* 		vQueueDelete(queue); */
/* 		queue = nullptr; */
/* 	} */
/* } */

void send_msg(app::Message* msg) {
	ESP_LOGI(BT_APP_TAG, "Sending message");

	if (msg == nullptr || queue == nullptr) {
		ESP_LOGE(BT_APP_TAG, "send_msg failed");
		return;
	}

	if (xQueueSend(queue, msg, 10 / portTICK_PERIOD_MS) != pdTRUE) {
		ESP_LOGE(BT_APP_TAG, "xQueue send failed");
	}
}

void app::dispatch(Callback callback, uint16_t event, void* param, int param_len) {
	ESP_LOGI(BT_APP_TAG, "event 0x%x, param len %d", event, param_len);

	Message msg;
	memset(&msg, 0, sizeof(msg));

	msg.event = event;
	msg.callback = callback;

	if (param_len == 0) {
		send_msg(&msg);
	} else if (param && param_len > 0) {
		if ((msg.param = malloc(param_len)) != nullptr) {
			memcpy(msg.param, param, param_len);
			send_msg(&msg);
		}
	} else {
		ESP_LOGE(BT_APP_TAG, "Failed to dispatch!");
	}
}

