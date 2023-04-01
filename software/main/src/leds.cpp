#include "driver/gpio.h"
#include "sys/lock.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "leds.h"

static leds::Bluetooth state = leds::Bluetooth::DISCONNECTED;
static _lock_t lock;
static void update_bluetooth_led(void*) {
	static uint8_t current_level = 0;
	for (;;) {
		_lock_acquire(&lock);
		switch (state) {
			case leds::Bluetooth::DISCOVERABLE:
				current_level = !current_level;
				break;
			case leds::Bluetooth::CONNECTED:
				current_level = 1;
				break;
			case leds::Bluetooth::DISCONNECTED:
				current_level = 0;
				break;
		}
		_lock_release(&lock);

		gpio_set_level(LED_PIN_BLUETOOTH, current_level);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

void leds::init() {
	// Setup the gpio pin
	gpio_reset_pin(LED_PIN_BLUETOOTH);
	gpio_set_direction(LED_PIN_BLUETOOTH, GPIO_MODE_OUTPUT);

	// Start the task
	xTaskCreate(update_bluetooth_led, "Bluetooth LED", 1024, nullptr, 0, nullptr);
}

void leds::set_bluetooth(leds::Bluetooth s) {
	_lock_acquire(&lock);
	state = s;
	_lock_release(&lock);
}
