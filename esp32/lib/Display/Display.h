#ifndef Display_h

#define Display_h

#include <Arduino.h>
#include <TFT_eSPI.h>
// #include <Fonts/FreeMono18pt7b.h>
// #include <Fonts/FreeMono24pt7b.h>
#define DEFAULT_FONT &FreeSans12pt7b
#define X_PADDING 5
#define Y_PADDING 5
#define Y_OFFSET 2

bool initDisplay();

void displayClear(uint16_t background = TFT_BLACK);
uint16_t displayText(
    String text,
    int x,
    int y,
    const GFXfont *font = DEFAULT_FONT,
    int width = 0,
    uint16_t background = TFT_BLACK,
    uint16_t color = TFT_WHITE);
void displayPrintLine(String text);
void displayPrint(String text);
void displayJpg(String filename, int xpos, int ypos, int scale = 1);
void displayFillRect(int x, int y, int width, int height, uint16_t color);

#endif