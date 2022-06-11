#pragma once

#include <cstdint>
#include <stdint.h>

namespace can {
	#pragma pack(1)
	struct Buttons {
		uint8_t _1 : 1;
		bool source : 1;
		bool volume_down : 1;
		bool volume_up : 1;
		uint8_t _2 : 2;
		bool backward : 1;
		bool forward : 1;

		uint8_t scroll : 8;

		uint8_t _3 : 8;
	};
	#pragma pack()

	enum Source : uint8_t {
		Bluetooth = 0b111,
		USB = 0b110,
		AUX2 = 0b101,
		AUX1 = 0b100,
		CD_Changer = 0b011,
		CD = 0b010,
		Tuner = 0b001
	};

	enum DiskStatus : uint8_t {
		Init = 0b00,
		Unavailable = 0b01,
		Available = 0b10
	};

	#pragma pack(1)
	struct Radio {
		uint8_t _1 : 5;
		bool muted : 1;
		uint8_t _2 : 1;
		bool enabled : 1;

		uint8_t _3 : 4;
		bool cd_changer_available : 1;
		DiskStatus disk_status : 2;
		uint8_t _4 : 1;

		uint8_t _5 : 4;
		Source source : 3;
		uint8_t _6 : 1;

		uint8_t _7 : 8;
	};
	#pragma pack()
}
