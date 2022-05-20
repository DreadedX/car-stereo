#pragma once
#include "esp_bt_device.h"
#include "esp_a2dp_api.h"

const char* addr_to_str(esp_bd_addr_t bda);
const char* connection_state_to_str(esp_a2d_connection_state_t state);

void set_scan_mode(bool connectable);

void get_last_connection(esp_bd_addr_t last_connection);
void set_last_connection(esp_bd_addr_t last_connection);
bool has_last_connection();
void clean_last_connection();

void play_wav(const uint8_t start[], const uint8_t end[]);
