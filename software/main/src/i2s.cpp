#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include "esp_log.h"
#include "driver/i2s.h"

#include "i2s.h"
#include "config.h"

#define I2S_TAG "APP_I2S"

#define RINGBUF_SIZE (16 * 1024)
#define AUDIO_SAMPLE_SIZE (16 * 2 / 8) // 16bit, 2ch, 8bit/byte

static RingbufHandle_t ringbuffer = nullptr;

static void task(void*) {
	ESP_LOGI(I2S_TAG, "Starting i2s task");
	for (;;) {
		size_t length = 0;
		uint8_t* data = (uint8_t*)xRingbufferReceive(ringbuffer, &length, portMAX_DELAY);

		// @TODO Swap channels
		if (length) {
			size_t bytes_written;
			if (i2s_write(I2S_PORT, data, length, &bytes_written, portMAX_DELAY) != ESP_OK) {
				ESP_LOGE(I2S_TAG, "i2s_write has failed");
			}

			if (bytes_written < length) {
				ESP_LOGE(I2S_TAG, "Timeout: not all bytes were written to I2S");
			}

			vRingbufferReturnItem(ringbuffer, data);
		}
	}
}

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
		.bck_io_num = I2S_PIN_BCK,
		.ws_io_num = I2S_PIN_WS,
		.data_out_num = I2S_PIN_DATA,
		.data_in_num = I2S_PIN_NO_CHANGE,
	};

	if (i2s_set_pin(i2s_port, &pin_config) != ESP_OK) {
		ESP_LOGE(I2S_TAG, "i2s_set_pin failed");
	}

	ringbuffer = xRingbufferCreate(RINGBUF_SIZE, RINGBUF_TYPE_BYTEBUF);
	if (!ringbuffer) {
		ESP_LOGE(I2S_TAG, "Failed to create ringbuffer");
		return;
	}

	xTaskCreate(task, "I2S Task", 2048, nullptr, 0, nullptr);
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

void i2s::write(const uint8_t* data, size_t length) {
	UBaseType_t items;
	vRingbufferGetInfo(ringbuffer, nullptr, nullptr, nullptr, nullptr, &items);

	if (items < RINGBUF_SIZE * 3/8) {
		xRingbufferSend(ringbuffer, data, AUDIO_SAMPLE_SIZE, portMAX_DELAY);
	} else if (items > RINGBUF_SIZE * 5/8) {
		length -= AUDIO_SAMPLE_SIZE;
	}

	if (!xRingbufferSend(ringbuffer, data, length, portMAX_DELAY)) {
		ESP_LOGE(I2S_TAG, "Failed to write to ringbuffer");
	}
}
