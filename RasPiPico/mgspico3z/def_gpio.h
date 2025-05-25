#pragma once
/**
 * CT-ARK (RaspberryPiPico firmware)
 * Copyright (c) 2024 Harumakkin.
 * SPDX-License-Identifier: MIT
 */


#include "hardware/spi.h"

// SD Card(SPI-0)
#define MMC_SPI_DEV 			spi0
#define MMC_SPI_TX_PIN  		19		// GP19 spi0 TX	 pin.25
#define MMC_SPI_RX_PIN  		16		// GP16 spi0 RX	 pin.21
#define MMC_SPI_SCK_PIN 		18		// GP18 spi0 SCK pin.24
#define MMC_SPI_CSN_PIN 		17		// GP17 spi0 CSn pin.22

// TangNano9K(SPI-1)
#define HARZ80_SPI_DEV 			spi1
#define HARZ80_SPI_PIN_TX 	 	11		// GP11 spi1 TX
#define HARZ80_SPI_PIN_RX  		8		// GP8 	spi1 RX
#define HARZ80_SPI_PIN_SCK 		10		// GP10 spi1 SCK
#define HARZ80_SPI_PIN_CS_n 	9		// GP9 	spi1 CS_n
#define HARZ80_GPIO_PIN_RESET_n	20		// RESET

// Si5351 Clock generator (I2C-0)
#define SI5351_I2C_DEV			i2c0
#define SI5351_I2C_ADDR 		0x60	// 0b0110 0000
#define SI5351_I2C_PIN_SDA 		0		// i2c0, GPIO_0
#define SI5351_I2C_PIN_SCL 		1		// i2c0, GPIO_1

// OLED Display(I2C-1)
#define SSD1306_I2C_DEV			i2c1
#define SSD1306_I2C_ADDR		(0x3c)
#define SSD1306_I2C_PIN_SDA		18		// i2c1, GPIO_18
#define SSD1306_I2C_PIN_SCL		19		// i2c1, GPIO_19

// HW-SW
#define	MGSPICO_SW1				28
#define	MGSPICO_SW2				27
#define	MGSPICO_SW3				26

// LED on Pico.
#define	GPIO_PICO_LED			25

// UART for stdio
#define PICO_UART1_DEV			uart1
#define PICO_UART1_TX			4
#define PICO_UART1_RX			5

