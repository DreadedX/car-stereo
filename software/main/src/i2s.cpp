#include "esp_log.h"
#include "driver/i2s.h"

#include "i2s.h"

#define I2S_TAG "APP_I2S"

void i2s::init() {
	ESP_LOGI(I2S_TAG, "Initializing i2s");

	i2s_port_t i2s_port = I2S_PORT;

	i2s_config_t i2s_config = {
		.mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_TX),
		.sample_rate = get_sample_rate(),
		.bits_per_sample = (i2s_bits_per_sample_t)16,
		.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
		.communication_format = (i2s_comm_format_t) (I2S_COMM_FORMAT_STAND_I2S),
		.intr_alloc_flags = 0, // default interrupt priority
		.dma_desc_num = 8,
		.dma_frame_num = 64,
		.use_apll = false,
		.tx_desc_auto_clear = true, // avoiding noise in case of data unavailability
		.fixed_mclk = 0,
		.mclk_multiple = I2S_MCLK_MULTIPLE_DEFAULT,
		.bits_per_chan = I2S_BITS_PER_CHAN_DEFAULT,
	};

	if (i2s_driver_install(i2s_port, &i2s_config, 0, nullptr) != ESP_OK) {
		ESP_LOGE(I2S_TAG, "i2s_driver_install failed");
	}

	i2s_pin_config_t pin_config = {
		.mck_io_num = 0,
		.bck_io_num = 26,
		.ws_io_num = 25,
		.data_out_num = 33,
		.data_in_num = I2S_PIN_NO_CHANGE,
	};

	if (i2s_set_pin(i2s_port, &pin_config) != ESP_OK) {
		ESP_LOGE(I2S_TAG, "i2s_set_pin failed");
	}
}

static uint32_t sample_rate = 44100;
void i2s::set_sample_rate(uint32_t sp) {
	sample_rate = sp;

	if (i2s_set_clk(I2S_PORT, sample_rate, 16, I2S_CHANNEL_STEREO) != ESP_OK){
		ESP_LOGE(I2S_TAG, "i2s_set_clk failed with samplerate=%d", sample_rate);
	} else {
		ESP_LOGI(I2S_TAG, "samplerate=%d", sample_rate);
	}
}

uint32_t i2s::get_sample_rate() {
	return sample_rate;
}
