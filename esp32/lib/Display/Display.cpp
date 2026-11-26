#include <Arduino.h>
#include "Display.h"
#include "rm67162.h"
#include <TFT_eSPI.h>
#include <cmath>
#include "LittleFS.h"
// #include <TFT_eFEX.h>
#include <TJpg_Decoder.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

bool pushImage(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data);

static const int COVER_SIZE = 200;
static const int DISPLAY_WIDTH = 536;
static const int DISPLAY_HEIGHT = 240;
static const int CLEAR_CHUNK_ROWS = 16;
static uint16_t clearBuffer[DISPLAY_WIDTH * CLEAR_CHUNK_ROWS];
static const size_t MAX_COVER_JPEG_BYTES = 192 * 1024;
static uint16_t coverPixels[COVER_SIZE * COVER_SIZE];
static uint16_t coverWidth = 0;
static uint16_t coverHeight = 0;
static int coverX = 0;
static int coverY = 0;
static volatile int coverDecodeState = 0; // 0 idle, 1 decoding, 2 ready, 3 failed
static volatile bool coverDecodeBoundsError = false;

static bool captureCoverBlock(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *data)
{
    if (x < 0 || y < 0 || x + w > coverWidth || y + h > coverHeight ||
        x + w > COVER_SIZE || y + h > COVER_SIZE)
    {
        coverDecodeBoundsError = true;
        return false;
    }
    for (uint16_t row = 0; row < h; ++row)
        memcpy(coverPixels + (y + row) * coverWidth + x, data + row * w, w * sizeof(uint16_t));
    return true;
}

static void scaleCoverBufferToTile()
{
    uint16_t sourceWidth = coverWidth;
    uint16_t sourceHeight = coverHeight;
    if (sourceWidth == 0 || sourceHeight == 0)
        return;

    // Upscale in place from bottom-right to top-left so source pixels are read
    // before an expanded destination can overwrite them.
    for (int y = COVER_SIZE - 1; y >= 0; --y)
    {
        uint16_t sourceY = (uint32_t)y * sourceHeight / COVER_SIZE;
        for (int x = COVER_SIZE - 1; x >= 0; --x)
        {
            uint16_t sourceX = (uint32_t)x * sourceWidth / COVER_SIZE;
            coverPixels[y * COVER_SIZE + x] = coverPixels[sourceY * sourceWidth + sourceX];
        }
    }
    coverWidth = COVER_SIZE;
    coverHeight = COVER_SIZE;
}

static void coverDecodeTask(void *)
{
    bool ok = false;
    const char *errorReason = "unknown decoder error";
    uint8_t *jpeg = nullptr;
    File file;
    if (LittleFS.begin())
    {
        file = LittleFS.open("/cover.jpg", "r");
        bool coverFileExists = (bool)file;
        size_t coverFileSize = coverFileExists ? file.size() : 0;
        if (coverFileExists && coverFileSize > 0 && coverFileSize <= MAX_COVER_JPEG_BYTES)
        {
            size_t jpegSize = coverFileSize;
            jpeg = (uint8_t *)malloc(jpegSize);
            if (jpeg && file.read(jpeg, jpegSize) == jpegSize)
            {
                file.close();
                LittleFS.end();
                uint16_t sourceWidth = 0, sourceHeight = 0;
                if (TJpgDec.getJpgSize(&sourceWidth, &sourceHeight, jpeg, jpegSize) == JDR_OK)
                {
                    int scale = 1;
                    while ((sourceWidth + scale - 1) / scale > COVER_SIZE ||
                           (sourceHeight + scale - 1) / scale > COVER_SIZE)
                        scale *= 2;
                    if (scale <= 8)
                    {
                        coverWidth = (sourceWidth + scale - 1) / scale;
                        coverHeight = (sourceHeight + scale - 1) / scale;
                        memset(coverPixels, 0, sizeof(coverPixels));
                        coverDecodeBoundsError = false;
                        TJpgDec.setJpgScale(scale);
                        TJpgDec.setSwapBytes(true);
                        TJpgDec.setCallback(captureCoverBlock);
                        bool decoded = TJpgDec.drawJpg(0, 0, jpeg, jpegSize) == JDR_OK;
                        TJpgDec.setCallback(pushImage);
                        ok = decoded && !coverDecodeBoundsError;
                        if (!decoded)
                            errorReason = "JPEG decode failed";
                        else if (coverDecodeBoundsError)
                            errorReason = "resized JPEG exceeded cover bounds";
                        else
                            scaleCoverBufferToTile();
                    }
                    else
                        errorReason = "required JPEG resize scale exceeds 8";
                }
                else
                    errorReason = "invalid JPEG dimensions or header";
            }
            else
            {
                file.close();
                LittleFS.end();
                errorReason = jpeg ? "could not read complete cover image" : "not enough memory for cover image";
            }
        }
        else
        {
            if (file)
                file.close();
            LittleFS.end();
            errorReason = !coverFileExists ? "cover image file is missing" :
                          coverFileSize == 0 ? "cover image file is empty" : "cover image exceeds size limit";
        }
    }
    else
        errorReason = "could not mount LittleFS";
    free(jpeg);
    if (!ok)
        Serial.printf("ERROR: Cover resize/decode failed: %s\n", errorReason);
    coverDecodeState = ok ? 2 : 3;
    vTaskDelete(nullptr);
}

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
    for (size_t i = 0; i < sizeof(clearBuffer) / sizeof(clearBuffer[0]); ++i)
        clearBuffer[i] = color;
    for (int y = 0; y < DISPLAY_HEIGHT; y += CLEAR_CHUNK_ROWS)
        lcd_PushColors(0, y, DISPLAY_WIDTH, CLEAR_CHUNK_ROWS, clearBuffer);
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

uint16_t displayTextWidth(String text, const GFXfont *font)
{
    TFT_eSprite sprite = TFT_eSprite(&tft);
    sprite.setFreeFont(font);
    return sprite.textWidth(text);
}

bool displayQueueCoverDecode(int x, int y)
{
    if (coverDecodeState != 0)
        return false;
    coverX = x;
    coverY = y;
    coverDecodeState = 1;
    if (xTaskCreatePinnedToCore(coverDecodeTask, "cover-decode", 8192, nullptr, 1, nullptr, 0) != pdPASS)
    {
        Serial.println("ERROR: Could not start cover resize/decode task");
        coverDecodeState = 3;
        return false;
    }
    return true;
}

DisplayJpgTaskResult displayProcessCoverDecode()
{
    if (coverDecodeState == 1)
        return DISPLAY_JPG_PENDING;
    if (coverDecodeState == 2)
    {
        // Keep every transfer inside the 180x180 artwork tile. A malformed or
        // unexpected JPEG size must never let the LCD address window reach text.
        if (coverWidth == 0 || coverHeight == 0 || coverWidth > COVER_SIZE || coverHeight > COVER_SIZE)
        {
            Serial.printf("ERROR: Resized cover dimensions out of bounds: %ux%u (max %d)\n",
                          (unsigned)coverWidth, (unsigned)coverHeight, COVER_SIZE);
            coverDecodeState = 0;
            return DISPLAY_JPG_FAILED;
        }
        int centeredX = coverX + (COVER_SIZE - coverWidth) / 2;
        int centeredY = coverY + (COVER_SIZE - coverHeight) / 2;
        for (uint16_t row = 0; row < coverHeight; ++row)
            lcd_PushColors(centeredX, centeredY + row, coverWidth, 1,
                           coverPixels + row * coverWidth);
        coverDecodeState = 0;
        return DISPLAY_JPG_DRAWN;
    }
    if (coverDecodeState == 3)
    {
        coverDecodeState = 0;
        return DISPLAY_JPG_FAILED;
    }
    return DISPLAY_JPG_IDLE;
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
