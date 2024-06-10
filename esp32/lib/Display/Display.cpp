#include <Arduino.h>
#include "Display.h"
#include "rm67162.h"
#include <TFT_eSPI.h>
#include <cmath>
#include "LittleFS.h"
// #include <TFT_eFEX.h>
#include <TJpg_Decoder.h>

TFT_eSPI tft = TFT_eSPI();

bool pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data)
{
    lcd_PushColors(x, y, w, h, data);
    return true;
}
bool initDisplay()
{
    Serial.println("Initializing display");
    try
    {
        rm67162_init(); // amoled lcd initialization
        lcd_setRotation(1);
        // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
        TJpgDec.setSwapBytes(true); // Swap the byte order for pushImage() and pushPixels() functions
        // The decoder must be given the exact name of the rendering function above
        TJpgDec.setCallback(pushImage);
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

void displayFillRect(int x, int y, int width, int height, uint16_t color)
{
    TFT_eSprite sprite = TFT_eSprite(&tft);
    sprite.setSwapBytes(1);
    sprite.createSprite(width, height);
    sprite.fillRect(0, 0, width, height, color);
    lcd_PushColors(x, y, width, height, (uint16_t *)sprite.getPointer());
    sprite.deleteSprite();
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

void displayJpg(String filename, int xpos, int ypos, int scale)
{
    TJpgDec.setJpgScale(scale);
    Serial.println("!!!!!!!! Displaying jpg");
    if (!LittleFS.begin())
    {
        Serial.println("Error mounting the file system");
        return;
    }
    TJpgDec.drawFsJpg(xpos, ypos, filename, LittleFS);
    // TFT_eSprite sprite = TFT_eSprite(&tft);
    // sprite.drawJpgFile(LittleFS, filename, xpos, ypos);
    // TFT_eFEX fex = TFT_eFEX(&tft);
    // fex.setSwapBytes(1);
    // fex.drawJpgFile(LittleFS, filename.c_str(), 0, 0);
    // Serial.println(fex.getWriteError());
    // lcd_PushColors(xpos, ypos, 300 + ypos, 300 + ypos, (uint16_t *)&fex);
    Serial.println("!!!!!!!! Displayed jpg");
}

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
    displayText(text.c_str(), printX, printY, DEFAULT_FONT);
    printX += text.length();
    Serial.print(text);
    Serial.print(printX);
}