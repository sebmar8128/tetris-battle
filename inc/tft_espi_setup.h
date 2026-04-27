#pragma once

// Project-local TFT_eSPI setup for a 4.0-inch 480x320 ST7796 SPI TFT.
#define ST7796_DRIVER 1
#define DISABLE_ALL_LIBRARY_WARNINGS 1
#define USE_HSPI_PORT 1

#define TFT_WIDTH  320
#define TFT_HEIGHT 480

#if defined(BOARD_ROLE_A) && defined(BOARD_ROLE_B)
#error "Build with only one board role: BOARD_ROLE_A or BOARD_ROLE_B."
#elif !defined(BOARD_ROLE_A) && !defined(BOARD_ROLE_B)
#error "Build with 'pio run -e a' or 'pio run -e b'."
#endif

// #define TFT_MISO 14 (UNUSED RIGHT NOW)
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS   46
#define TFT_DC   10
#define TFT_RST  9

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
