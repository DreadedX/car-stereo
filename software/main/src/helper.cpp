#include "esp_log.h"

#include "helper.h"

const char* addr_to_str(esp_bd_addr_t bda) {
    static char bda_str[18];
    sprintf(bda_str, "%02x:%02x:%02x:%02x:%02x:%02x", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return (const char*)bda_str;
}

const char* connection_state_to_str(esp_a2d_connection_state_t state) {
	const char* states[4] = {"Disconnected", "Connecting", "Connected", "Disconnecting"};
	return states[state];
}
