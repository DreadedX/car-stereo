#pragma once

#include "esp_bt_device.h"

namespace nvs {
	void init();

	void get_last_connection(esp_bd_addr_t last_connection);
	void set_last_connection(esp_bd_addr_t last_connection);
	bool has_last_connection();
}
