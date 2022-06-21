#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/twai.h"

#include "esp_log.h"
#include "twai.h"

#define TWAI_TAG "APP_TWAI"

static void listen(void*) {
	for (;;) {
		twai_message_t message;
		if (twai_receive(&message, portMAX_DELAY) != ESP_OK) {
			ESP_LOGI(TWAI_TAG, "Failed to receive message");
			continue;
		}

		if (message.extd) {
			ESP_LOGI(TWAI_TAG, "Message is in Extended Format");
		}

		ESP_LOGI(TWAI_TAG, "ID=%d, Length=%d", message.identifier, message.data_length_code);
	}
}

void twai::init() {
	twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(GPIO_NUM_5, GPIO_NUM_19, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
	twai_filter_config_t f_config = {
		.acceptance_code = (0b100100101 << 5) + (0b1000011111 << 21),
		.acceptance_mask = (0b011000000 << 5),
		.single_filter = false
	};

	if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
		ESP_LOGI(TWAI_TAG, "Driver installed");
	} else {
		ESP_LOGI(TWAI_TAG, "Failed to install the driver");
		return;
	}

	if (twai_start() == ESP_OK) {
		ESP_LOGI(TWAI_TAG, "Driver started");
	} else {
		ESP_LOGI(TWAI_TAG, "Failed to start driver");
	}

	xTaskCreate(listen, "TWAI Listener", 2048, nullptr, 0, nullptr);
}
