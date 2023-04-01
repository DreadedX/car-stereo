#include "esp_log.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "driver/i2s.h"

#include "a2dp.h"
#include "helper.h"
#include "wav.h"
#include "storage.h"
#include "i2s.h"
#include "bluetooth.h"
#include "leds.h"

#define A2DP_TAG "APP_A2DP"

static void handle_connection_state(uint16_t event, esp_a2d_cb_param_t* a2d) {
	ESP_LOGI(A2DP_TAG, "partner address: %s", addr_to_str(a2d->conn_stat.remote_bda));

	ESP_LOGI(A2DP_TAG, "A2DP connection state: %s, [%s]", connection_state_to_str(a2d->conn_stat.state), addr_to_str(a2d->conn_stat.remote_bda));

	static int retry_count = 0;
	static bool was_connected = false;
	if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
		ESP_LOGI(A2DP_TAG, "ESP_A2D_CONNECTION_STATE_DISCONNECTED");
		leds::set_bluetooth(leds::Bluetooth::DISCONNECTED);

		if (a2d->conn_stat.disc_rsn == ESP_A2D_DISC_RSN_ABNORMAL && retry_count < 3) {
				ESP_LOGI(A2DP_TAG,"Connection try number: %d", retry_count);

				a2dp::connect_to_last();
		} else {
			bluetooth::set_scan_mode(true);

			if (was_connected) {
				WAV_PLAY(disconnect);
			}
		}
	} else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED){
		ESP_LOGI(A2DP_TAG, "ESP_A2D_CONNECTION_STATE_CONNECTED");
		leds::set_bluetooth(leds::Bluetooth::CONNECTED);

		bluetooth::set_scan_mode(false);
		retry_count = 0;
		was_connected = true;

		WAV_PLAY(connect);

		// record current connection
		nvs::set_last_connection(a2d->conn_stat.remote_bda);
		if (esp_bt_gap_read_remote_name(a2d->conn_stat.remote_bda) != ESP_OK) {
			ESP_LOGE(A2DP_TAG, "esp_bt_gap_read_remote_name failed");
		}
	} else if (a2d->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTING){
		ESP_LOGI(A2DP_TAG, "ESP_A2D_CONNECTION_STATE_CONNECTING");
		retry_count++;
	}
}

static void handle_audio_cfg(uint16_t event, esp_a2d_cb_param_t* a2d) {
	ESP_LOGI(A2DP_TAG, "a2dp audio_cfg_cb , codec type %d", a2d->audio_cfg.mcc.type);

	char oct0 = a2d->audio_cfg.mcc.cie.sbc[0];
	if (oct0 & (0x01 << 6)) {
		i2s::set_sample_rate(32000);
	} else if (oct0 & (0x01 << 5)) {
		i2s::set_sample_rate(44100);
	} else if (oct0 & (0x01 << 4)) {
		i2s::set_sample_rate(48000);
	} else {
		i2s::set_sample_rate(16000);
	}

	ESP_LOGI(A2DP_TAG, "a2dp audio_cfg_cb , sample_rate %d", i2s::get_sample_rate());
}

static void a2d_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t* a2d) {
	switch (event) {
		case ESP_A2D_CONNECTION_STATE_EVT:
			ESP_LOGD(A2DP_TAG, "%s ESP_A2D_CONNECTION_STATE_EVT", __func__);
			handle_connection_state(event, a2d);
			break;

		case ESP_A2D_AUDIO_STATE_EVT:
			ESP_LOGD(A2DP_TAG, "%s ESP_A2D_AUDIO_STATE_EVT", __func__);
			break;

		case ESP_A2D_AUDIO_CFG_EVT:
			ESP_LOGD(A2DP_TAG, "%s ESP_A2D_AUDIO_CFG_EVT", __func__);
			handle_audio_cfg(event, a2d);
			break;

		case ESP_A2D_PROF_STATE_EVT:
			if (ESP_A2D_INIT_SUCCESS == a2d->a2d_prof_stat.init_state) {
				ESP_LOGI(A2DP_TAG,"A2DP PROF STATE: Init Compl\n");
			} else {
				ESP_LOGI(A2DP_TAG,"A2DP PROF STATE: Deinit Compl\n");
			}
			break;

		default:
			ESP_LOGE(A2DP_TAG, "%s unhandled evt %d", __func__, event);
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

static void audio_data_callback(const uint8_t* data, uint32_t len) {
	i2s::write(data, len);
	/* Frame* frame = (Frame*)data; */
	/* for (int i = 0; i < len/4; i++) { */
	/* 	int16_t temp = frame[i].channel1; */
	/* 	frame[i].channel1 = frame[i].channel2; */
	/* 	frame[i].channel2 = temp; */
	/* } */

	/* size_t bytes_written; */
	/* if (i2s_write(I2S_PORT, data, len, &bytes_written, portMAX_DELAY) != ESP_OK) { */
	/* 	ESP_LOGE(A2DP_TAG, "i2s_write has failed"); */
	/* } */

	/* if (bytes_written < len) { */
	/* 	ESP_LOGE(A2DP_TAG, "Timeout: not all bytes were written to I2S"); */
	/* } */
}

void a2dp::init() {
	ESP_LOGI(A2DP_TAG, "Initializing A2DP");

	// Initialize A2DP sink
	if (esp_a2d_register_callback(a2d_callback) != ESP_OK){
		ESP_LOGE(A2DP_TAG,"esp_a2d_register_callback failed");
	}
	if (esp_a2d_sink_register_data_callback(audio_data_callback) != ESP_OK){
		ESP_LOGE(A2DP_TAG,"esp_a2d_sink_register_data_callback failed");
	}
	if (esp_a2d_sink_init() != ESP_OK){
		ESP_LOGE(A2DP_TAG,"esp_a2d_sink_init failed");
	}
}

void a2dp::connect_to_last() {
	if (nvs::has_last_connection()) {
		esp_bd_addr_t last_connection = {0,0,0,0,0,0};
		nvs::get_last_connection(last_connection);
		if (esp_a2d_sink_connect(last_connection) == ESP_FAIL) {
			ESP_LOGE(A2DP_TAG, "Failed connecting to device!");
		}
	} else {
		bluetooth::set_scan_mode(true);
	}
}

