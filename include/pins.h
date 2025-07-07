#ifndef PINS_H
#define PINS_H

// Define pins for the ILI9341 display
#define TFT_CS   45
#define TFT_RST  0 
#define TFT_DC   35 
#define TFT_MOSI 36
#define TFT_CLK  37
#define TFT_LED  38
#define TFT_MISO 39

#define TOUCH_CLK  40
#define TOUCH_CS 41
#define TOUCH_DIN 42
#define TOUCH_DO 2
#define TOUCH_IRQ  1

// CAN Bus pins
#define CAN_RX_PIN 48
#define CAN_TX_PIN 47

// Status LED
#define STATUS_LED_PIN 15

// Colors
#define COLOR_BACKGROUND 0x0841
#define COLOR_CARD_BG    0x2124
#define COLOR_PRIMARY    0x07FF
#define COLOR_TEXT       0xFFFF
#define COLOR_SUCCESS    0x07E0
#define COLOR_WARNING    0xFFE0

#endif // PINS_H
