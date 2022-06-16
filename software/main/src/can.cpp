#include <cstdint>
#include <string.h>
#include <cmath>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "hal/spi_types.h"

#include "can.h"
#include "can_data.h"
#include "avrcp.h"
#include "helper.h"

#define CAN_TAG "APP_CAN"

#define PIN_NUM_MOSI 23
#define PIN_NUM_MISO 19
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5

#define MCP_SIDH        0
#define MCP_SIDL        1
#define MCP_EID8        2
#define MCP_EID0        3

#define MCP_TXB_EXIDE_M 0x08
#define MCP_DLC_MASK    0x0F
#define MCP_RTR_MASK    0x40

#define MCP_RXB_RX_STDEXT 0x00
#define MCP_RXB_RX_MASK   0x60
#define MCP_RXB_BUKT_MASK (1<<2)

#define MCP_STAT_TXIF_MASK 0xA8
#define MCP_STAT_TX0IF     0x08
#define MCP_STAT_TX1IF     0x20
#define MCP_STAT_TX2IF     0x80
#define MCP_STAT_RXIF_MASK 0x03

// Instructions
#define MCP_WRITE       0x02
#define MCP_READ        0x03
#define MCP_BITMOD      0x05
#define MCP_READ_RX0    0x90
#define MCP_READ_RX1    0x94
#define MCP_READ_STATUS 0xA0
#define MCP_RESET       0xC0

// Registers
#define MCP_RXF0SIDH 0x00
#define MCP_RXF1SIDH 0x04
#define MCP_RXF2SIDH 0x08
#define MCP_CANSTAT  0x0E
#define MCP_CANCTRL  0x0F
#define MCP_RXM0SIDH 0x20
#define MCP_RXM1SIDH 0x24
#define MCP_CNF3     0x28
#define MCP_CNF2     0x29
#define MCP_CNF1     0x2A
#define MCP_CANINTE  0x2B
#define MCP_CANINTF  0x2C
#define MCP_TXB0CTRL 0x30
#define MCP_TXB1CTRL 0x40
#define MCP_TXB2CTRL 0x50
#define MCP_RXB0CTRL 0x60
#define MCP_RXB1CTRL 0x70

// CANCTRL Register values
#define MODE_NORMAL     0x00
#define MODE_SLEEP      0x20
#define MODE_LISTENONLY 0x60
#define MODE_CONFIG     0x80
#define MODE_MASK       0xE0

// CANINTF Register bits
#define MCP_RX0IF 0x01
#define MCP_RX1IF 0x02
#define MCP_TX0IF 0x04
#define MCP_TX1IF 0x08
#define MCP_TX2IF 0x10
#define MCP_WAKIF 0x40

static void send_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_active = false) {
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8;
	t.tx_buffer = &cmd;
	if (keep_active) {
		t.flags = SPI_TRANS_CS_KEEP_ACTIVE;
	}

	esp_err_t ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);

}

static uint8_t read_register(spi_device_handle_t spi, const uint8_t address) {
	// Acquire the bus in order to use SPI_TRANS_CS_KEEP_ACTIVE
	esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
	ESP_ERROR_CHECK(ret);

	// Send the command
	{
		spi_transaction_t t;
		memset(&t, 0, sizeof(t));
		t.length = 8*2;
		uint8_t cmd[] = { MCP_READ, address };
		t.tx_buffer = cmd;
		t.flags = SPI_TRANS_CS_KEEP_ACTIVE;

		ret = spi_device_polling_transmit(spi, &t);
		ESP_ERROR_CHECK(ret);
	}

	// Read the data
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8;
	t.flags = SPI_TRANS_USE_RXDATA;

	ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);

	spi_device_release_bus(spi);
	ESP_ERROR_CHECK(ret);

	return t.rx_data[0];
}

static void set_register(spi_device_handle_t spi, const uint8_t address, const uint8_t value) {
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8*3;
	uint8_t cmd[] = { MCP_WRITE, address, value };
	t.tx_buffer = cmd;

	esp_err_t ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
}

static void modify_register(spi_device_handle_t spi, const uint8_t address, const uint8_t mask, const uint8_t data) {
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8*4;
	uint8_t cmd[] = { MCP_BITMOD, address, mask, data };
	t.tx_buffer = cmd;

	esp_err_t ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
}

static bool request_new_mode(spi_device_handle_t spi, const uint8_t new_mode) {
	unsigned long start = esp_timer_get_time() / 1000;

	for (;;) {
		modify_register(spi, MCP_CANCTRL, MODE_MASK, new_mode);
		uint8_t state = read_register(spi, MCP_CANSTAT);
		if ((state & MODE_MASK) == new_mode) {
			return true;
		} else if ((esp_timer_get_time() / 1000 - start) > 200) {
			ESP_LOGE(CAN_TAG, "Failed to set new mode");
			abort();
		}
	}

}

static void set_CANCTRL_mode(spi_device_handle_t spi, uint8_t new_mode) {
	ESP_LOGI(CAN_TAG, "set_CANCTRL_mode");
	if ((read_register(spi, MCP_CANSTAT) & MODE_MASK) == MODE_SLEEP && new_mode != MODE_SLEEP) {
		ESP_LOGI(CAN_TAG, "if");
		uint8_t wake_int_enabled = (read_register(spi, MCP_CANINTE) & MCP_WAKIF);
		if (!wake_int_enabled) {
			modify_register(spi, MCP_CANINTE, MCP_WAKIF, MCP_WAKIF);
		}

		modify_register(spi, MCP_CANINTF, MCP_WAKIF, MCP_WAKIF);

		request_new_mode(spi, MODE_LISTENONLY);

		if (!wake_int_enabled) {
			modify_register(spi, MCP_CANINTE, MCP_WAKIF, 0);
		}
	}

	modify_register(spi, MCP_CANINTF, MCP_WAKIF, 0);

	request_new_mode(spi, new_mode);
	ESP_LOGI(CAN_TAG, "done!");
}

static void config_rate(spi_device_handle_t spi) {
	// This is for 16MHz, 125kBPS
	set_register(spi, MCP_CNF1, 0x01);
	set_register(spi, MCP_CNF2, 0xb1);
	set_register(spi, MCP_CNF3, 0x05);
}

static void init_CAN_buffers(spi_device_handle_t spi) {
	uint8_t a1 = MCP_TXB0CTRL;
	uint8_t a2 = MCP_TXB1CTRL;
	uint8_t a3 = MCP_TXB2CTRL;

	for (int i = 0; i < 14; i++) {
		set_register(spi, a1, 0);
		set_register(spi, a2, 0);
		set_register(spi, a3, 0);

		a1++;
		a2++;
		a3++;
	}

	set_register(spi, MCP_RXB0CTRL, 0);
	set_register(spi, MCP_RXB1CTRL, 0);
}

static uint8_t read_status(spi_device_handle_t spi) {
	// Acquire the bus in order to use SPI_TRANS_CS_KEEP_ACTIVE
	esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
	ESP_ERROR_CHECK(ret);

	send_cmd(spi, MCP_READ_STATUS, true);

	// Read the data
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8;
	t.flags = SPI_TRANS_USE_RXDATA;

	ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);

	spi_device_release_bus(spi);
	ESP_ERROR_CHECK(ret);

	return t.rx_data[0];
}

static void read_can_msg(spi_device_handle_t spi, uint8_t buffer_load_addr, uint16_t* id, uint8_t* len, uint8_t* buf) {
	// Acquire the bus in order to use SPI_TRANS_CS_KEEP_ACTIVE
	esp_err_t ret = spi_device_acquire_bus(spi, portMAX_DELAY);
	ESP_ERROR_CHECK(ret);

	send_cmd(spi, buffer_load_addr, true);

	// Read id + length +  data
	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8*(5+8);
	uint8_t data[5+8];
	t.rx_buffer = data;

	ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);

	*id = (data[MCP_SIDH] << 3) + (data[MCP_SIDL] >> 5);

	*len = data[4] & MCP_DLC_MASK;

	for (int i = 0; i < *len; ++i) {
		buf[i] = data[5 + i];
	}

	// Make sure we release the bus
	spi_device_release_bus(spi);
	ESP_ERROR_CHECK(ret);
}

static void write_id(spi_device_handle_t spi, const uint8_t addr, const uint16_t id) {
	uint8_t cmd[6];
	cmd[0] = MCP_WRITE;
	cmd[1] = addr;
	cmd[MCP_SIDH+2] = id >> 3;
	cmd[MCP_SIDL+2] = (id & 0x07) << 5;
	cmd[MCP_EID0+2] = 0;
	cmd[MCP_EID8+2] = 0;

	spi_transaction_t t;
	memset(&t, 0, sizeof(t));
	t.length = 8*6;
	t.tx_buffer = cmd;

	esp_err_t ret = spi_device_polling_transmit(spi, &t);
	ESP_ERROR_CHECK(ret);
}

static bool available(uint8_t status) {
	return status & MCP_STAT_RXIF_MASK;
}

static uint8_t read_rx_tx_status(uint8_t status) {
	uint8_t ret = (status & (MCP_STAT_TXIF_MASK | MCP_STAT_RXIF_MASK));

	ret = (ret & MCP_STAT_TX0IF ? MCP_TX0IF : 0) |
	      (ret & MCP_STAT_TX1IF ? MCP_TX1IF : 0) |
	      (ret & MCP_STAT_TX2IF ? MCP_TX2IF : 0) |
	      (ret & MCP_STAT_RXIF_MASK);

	return ret;
}

static void can_task(void* params) {
	spi_device_handle_t spi = *(spi_device_handle_t*)params;

	for (;;) {
		uint8_t status = read_status(spi);

		if (available(status)) {
			uint8_t rx_tx_status = read_rx_tx_status(status);

			uint16_t id;
			uint8_t len;
			uint8_t buf[8];

			if (rx_tx_status & MCP_RX0IF) {
				read_can_msg(spi, MCP_READ_RX0, &id, &len, buf);
			} else if (rx_tx_status & MCP_RX1IF) {
				read_can_msg(spi, MCP_READ_RX0, &id, &len, buf);
			}

			static bool enabled = false;
			switch (id) {
				case BUTTONS_ID:
					if (enabled) {
						can::Buttons buttons = can::convert<can::Buttons>(buf, len);

						static MultiPurposeButton button_forward(avrcp::play_pause, avrcp::forward);
						button_forward.update(buttons.forward);

						static MultiPurposeButton button_backward(nullptr, avrcp::backward);
						button_backward.update(buttons.backward);

						static uint8_t scroll = 0;
						if (scroll != buttons.scroll) {
							int8_t diff = buttons.scroll - scroll;

							if (diff < 0) {
								avrcp::seek_backward();
							} else {
								avrcp::seek_forward();
							}

							scroll = buttons.scroll;
						}
					}
					break;

				case VOLUME_ID:
					if (enabled) {
						can::Volume volume = can::convert<can::Volume>(buf, len);
						avrcp::set_volume(ceil(volume.volume * 4.2f));
					}
					break;

				case RADIO_ID:
					{
						can::Radio radio = can::convert<can::Radio>(buf, len);
						enabled = (radio.source == can::Source::AUX2) && (radio.enabled);
					}
					break;
				
				default:
					break;
			}
		}
	}
}

void can::init() {
	ESP_LOGI(CAN_TAG, "Initializing can");

	spi_bus_config_t buscfg = {
		.mosi_io_num=PIN_NUM_MOSI,
		.miso_io_num=PIN_NUM_MISO,
		.sclk_io_num=PIN_NUM_CLK,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1,
		.max_transfer_sz=0
	};
	esp_err_t ret = spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(CAN_TAG, "Adding device");
	spi_device_interface_config_t devcfg = {
		.mode=0,
		.clock_speed_hz=10*1000*1000,
		.spics_io_num=PIN_NUM_CS,
		.queue_size=7,
	};
	spi_device_handle_t* spi = new spi_device_handle_t;
	ret = spi_bus_add_device(VSPI_HOST, &devcfg, spi);
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(CAN_TAG, "Resetting MCP1525");
	send_cmd(*spi, MCP_RESET);
	// @TODO Check if this is really needed
	// Copied from other libary, here to give the MCP2515 time to wake up
	vTaskDelay(5 / portTICK_PERIOD_MS);

	ESP_LOGI(CAN_TAG, "Enter config mode");
	set_CANCTRL_mode(*spi, MODE_CONFIG);

	ESP_LOGI(CAN_TAG, "Setting rate");
	config_rate(*spi);

	ESP_LOGI(CAN_TAG, "Init CAN buffers");
	init_CAN_buffers(*spi);

	ESP_LOGI(CAN_TAG, "Setting interrupt mode");
	set_register(*spi, MCP_CANINTE, MCP_RX0IF | MCP_RX1IF);

	ESP_LOGI(CAN_TAG, "Enable receive buffers");
	/* modify_register(*spi, MCP_RXB0CTRL, MCP_RXB_RX_MASK | MCP_RXB_BUKT_MASK, MCP_RXB_RX_STDEXT | MCP_RXB_BUKT_MASK); */
	modify_register(*spi, MCP_RXB0CTRL, MCP_RXB_RX_MASK, MCP_RXB_RX_STDEXT);
	modify_register(*spi, MCP_RXB1CTRL, MCP_RXB_RX_MASK, MCP_RXB_RX_STDEXT);

	ESP_LOGI(CAN_TAG, "Init mask");
	write_id(*spi, MCP_RXM0SIDH, 0x3ff);
	write_id(*spi, MCP_RXM1SIDH, 0x3ff);

	ESP_LOGI(CAN_TAG, "Init filter");
	write_id(*spi, MCP_RXF0SIDH, RADIO_ID);
	write_id(*spi, MCP_RXF1SIDH, VOLUME_ID);
	write_id(*spi, MCP_RXF2SIDH, BUTTONS_ID);

	ESP_LOGI(CAN_TAG, "Enter normal mode");
	set_CANCTRL_mode(*spi, MODE_NORMAL);

	ESP_LOGI(CAN_TAG, "Init done!");

	xTaskCreatePinnedToCore(can_task, "CAN Task", 4096, spi, 0, nullptr, 0);
}
