#pragma once

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"

#define BOARD_LCD_HOST SPI2_HOST
#define BOARD_LCD_H_RES 466
#define BOARD_LCD_V_RES 466
#define BOARD_LCD_DRAW_LINES 40

#define BOARD_LCD_CS GPIO_NUM_9
#define BOARD_LCD_CLK GPIO_NUM_10
#define BOARD_LCD_DATA0 GPIO_NUM_11
#define BOARD_LCD_DATA1 GPIO_NUM_12
#define BOARD_LCD_DATA2 GPIO_NUM_13
#define BOARD_LCD_DATA3 GPIO_NUM_14
#define BOARD_LCD_RST GPIO_NUM_21
#define BOARD_LCD_EN GPIO_NUM_42

#define BOARD_I2C_PORT I2C_NUM_0
#define BOARD_I2C_SDA GPIO_NUM_47
#define BOARD_I2C_SCL GPIO_NUM_48
