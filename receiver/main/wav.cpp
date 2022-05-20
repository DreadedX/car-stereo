#include "esp_log.h"
#include "driver/i2s.h"

#include "wav.h"
#include "i2s.h"

#define WAV_TAG "APP_WAV"
#define WAV_HEADER_SIZE 44
#define I2S_BUFFER_SIZE 512

struct PlayWavParam {
	const uint8_t* data;
	uint32_t len;
};

static void task(void* param) {
	ESP_LOGI(WAV_TAG, "Playing sample...");
	PlayWavParam* p = (PlayWavParam*)param;

	// Switch to mono since the samples are all in mono to save space
	i2s_zero_dma_buffer(I2S_PORT);
	i2s_set_clk(I2S_PORT, 44100, 16, I2S_CHANNEL_MONO);

	for (uint32_t index = 0; index < p->len; index += I2S_BUFFER_SIZE) {
		int rest = p->len - index;

		size_t byte_size = I2S_BUFFER_SIZE;
		if (rest < I2S_BUFFER_SIZE) {
			byte_size = rest;
		}

		const unsigned char* current = p->data + WAV_HEADER_SIZE + index;
		size_t bytes_written;
		if (i2s_write(I2S_PORT, current, byte_size, &bytes_written, portMAX_DELAY) != ESP_OK) {
			ESP_LOGE(WAV_TAG, "i2s_write has failed");
		}

		if (bytes_written < byte_size) {
			ESP_LOGE(WAV_TAG, "Timeout: not all bytes were written to I2S");
		}
	}

	// Switch back to stereo with the correct samplerate
	i2s_zero_dma_buffer(I2S_PORT);
	i2s_set_clk(I2S_PORT, i2s::get_sample_rate(), 16, I2S_CHANNEL_STEREO);

	ESP_LOGI(WAV_TAG, "Done");

	free(p);
	vTaskDelete(nullptr);
}

void wav::play(const uint8_t* start, const uint8_t* end) {
	ESP_LOGI(WAV_TAG, "Creating Play Wav Task");
	uint32_t len = (end - start - WAV_HEADER_SIZE);

	PlayWavParam* param = new PlayWavParam;
	param->data = start;
	param->len = len;

	if (xTaskCreate(task, "PlayWav", 2048, param, configMAX_PRIORITIES - 3, nullptr) != pdPASS) {
		ESP_LOGE(WAV_TAG, "Failed to create play wav task");
	}
}

