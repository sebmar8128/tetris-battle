#pragma once

// Project-local TFT_eSPI setup for a 4.0-inch 480x320 ST7796 SPI TFT.
// Adjust these GPIOs to match the physical wiring before hardware bring-up.
#define ST7796_DRIVER 1
#define DISABLE_ALL_LIBRARY_WARNINGS 1

#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  14

#define LOAD_GLCD  1
#define LOAD_FONT2 1
#define LOAD_FONT4 1
#define LOAD_FONT6 1
#define LOAD_FONT7 1
#define LOAD_FONT8 1
#define LOAD_GFXFF 1
#define SMOOTH_FONT 1

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  16000000
