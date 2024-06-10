#include <Arduino.h>
#include "Display.h"
#include "rm67162.h"
#include <TFT_eSPI.h>
#include <cmath>
#include "LittleFS.h"

TFT_eSPI tft = TFT_eSPI();

bool initDisplay()
{
    Serial.println("Initializing display");
    try
    {
        rm67162_init(); // amoled lcd initialization
        lcd_setRotation(1);
    }
    catch (const std::exception &e)
    {
        Serial.println("Error initializing display");
        Serial.println(e.what());
        return false;
    }
    return true;
}

void displayClear(uint16_t color)
{
    TFT_eSprite sprite = TFT_eSprite(&tft);
    sprite.setSwapBytes(1);                                          // Swap the byte order for pushImage() and pushPixels() functions
    sprite.createSprite(536, 240);                                   // Create a sprite that covers the entire display
    sprite.fillSprite(color);                                        // Fill the sprite with the specified color
    lcd_PushColors(0, 0, 536, 240, (uint16_t *)sprite.getPointer()); // Push the sprite to the display
    sprite.deleteSprite();                                           // Delete the sprite to free up memory
}

uint16_t displayText(String text, int x, int y, const GFXfont *font, int width, uint16_t background, uint16_t color)
{
    uint16_t primary = ((0x1D & 0xF8) << 8) | ((0xB9 & 0xFC) << 3) | (0x54 >> 3);
    TFT_eSprite sprite = TFT_eSprite(&tft);
    sprite.setSwapBytes(1);
    sprite.setFreeFont(font); // Set the font to the free font
    int16_t textWidth = width > 0 ? width : sprite.textWidth(text);
    int16_t textHeight = sprite.fontHeight();

    // Create a sprite that's the size of the text
    sprite.createSprite(textWidth, textHeight);

    // Draw a filled rectangle with the background color
    sprite.fillRect(0, 0, textWidth, textHeight, background);

    // Draw the text on top of the rectangle
    sprite.setTextColor(color, background, true);
    sprite.drawString(text, 0, 0); // Draw the text at the top-left corner of the sprite

    // Push the sprite to the TFT display at the desired location

    lcd_PushColors(x, y, textWidth, textHeight, (uint16_t *)sprite.getPointer());
    // sprite1.pushSprite(x, y);

    // Delete the sprite to free up memory
    sprite.deleteSprite();

    return textHeight;
}

// void displayJpg(const char *filename, int xpos, int ypos)
// {
//     sprite.drawJpgFile(LittleFS, filename, xpos, ypos);
// }

int printX = X_PADDING;
int printY = Y_PADDING;
void displayPrintLine(String text)
{
    printX = X_PADDING;
    if (printY > 220)
    {
        return;
    }
    if (text.length() > 0)
    {
        printY += displayText(text, printX, printY, DEFAULT_FONT);
        printY += Y_PADDING;
    }
}

void displayPrint(String text)
{
    displayText(text, printX, printY, DEFAULT_FONT);
    printX += text.length();
    Serial.print(text);
    Serial.print(printX);
}