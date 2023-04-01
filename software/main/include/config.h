#pragma once

#include "hal/gpio_types.h"

// FOR PROTOTYPE
#ifdef CONFIG_CAR_STEREO_PROTOTYPE
	#pragma message ( "Building for the prototype" )
	#define I2S_PIN_BCK GPIO_NUM_26
	#define I2S_PIN_WS GPIO_NUM_25
	#define I2S_PIN_DATA GPIO_NUM_33
	#define TWAI_PIN_CTX GPIO_NUM_5
	#define TWAI_PIN_CRX GPIO_NUM_19
#else
	#pragma message ( "Building for the production" )
	#define I2S_PIN_BCK GPIO_NUM_5
	#define I2S_PIN_WS GPIO_NUM_16
	#define I2S_PIN_DATA GPIO_NUM_17
	#define TWAI_PIN_CTX GPIO_NUM_32
	#define TWAI_PIN_CRX GPIO_NUM_35
#endif

