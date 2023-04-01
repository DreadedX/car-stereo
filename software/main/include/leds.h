#pragma once

namespace leds {
	enum Bluetooth {
		DISCOVERABLE,
		CONNECTED,
		DISCONNECTED,
	};

	void init();
	void set_bluetooth(Bluetooth state);
}
