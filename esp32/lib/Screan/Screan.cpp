#include "Screan.h"
#include <Spotify.h>
#include <WiFiSetup.h>
#include <Display.h>

uint16_t color = TFT_WHITE;
uint16_t background = TFT_BLACK; // ((0x19 & 0xF8) << 8) | ((0x14 & 0xFC) << 3) | (0x14 >> 3);
uint16_t primary = ((0x1D & 0xF8) << 8) | ((0xB9 & 0xFC) << 3) | (0x54 >> 3);
bool isInitialized = false;
void initScrean()
{
    displayClear(background);
}
void displayWifiState(boolean connected)
{
    if (connected)
    {
        // displayText("Connected", 0, 20, &FreeMono18pt7b, 200, background, color);
    }
    else
    {
        displayText("Not Connected", 0, 20, &FreeMono18pt7b, 200, background, color);
    }
}
void displayNameLine1(String name)
{
    displayText(name, 190, 20, &FreeSans18pt7b, 336, background, color);
}

void displayNameLine2(String name)
{
    displayText(name, 190, 60, &FreeSans18pt7b, 336, background, color);
}

void displayNameLine3(String subtitle)
{
    displayText(subtitle, 190, 110, &FreeSans12pt7b, 336, background, color);
}
void displayProgressBar(int x, int y, int width, int height, float progress, uint16_t barColor, uint16_t backgroundColor)
{
    int barWidth = map(progress * 100, 0, 100, 0, width);
    displayFillRect(x + barWidth, y, width - barWidth, height, backgroundColor);
    displayFillRect(x, y, barWidth, height, barColor);
}
void updateScrean()
{
    if (!isInitialized)
    {
        displayClear(background);
        // displayWifiState(getWiFiStatus());
        // displayNameLine1("Line 1 Text ....1....2....3....4..");
        // displayNameLine2("Line 2 Text ......................");
        // displayNameLine3("Line 3 Text ......................");
        isInitialized = true;
    }

    if (hasWiFiStatusUpdate())
    {
        Serial.println("Update WiFiStatus:" + getWiFiStatus());
        displayWifiState(getWiFiStatus());
    }
    if (getHasData())
    {
        if (hasProgressUpdate())
        {
            float progres = getProgress();
            Serial.println("Update Progress:" + String(progres));
            displayProgressBar(20, 190, 496, 5, progres, primary, background);
        }
        if (hasNameUpdate())
        {
            displayFillRect(20, 20, 150, 150, background);
            Serial.println("Update Name:" + getName());
            String name = getName();
            if (name.length() > 20)
            {
                int breakPoint = name.lastIndexOf(' ', 20);
                if (breakPoint == -1)
                    breakPoint = name.lastIndexOf(',', 20);
                if (breakPoint == -1)
                    breakPoint = 20;

                displayNameLine1(name.substring(0, breakPoint));
                displayNameLine2(name.substring(breakPoint + 1, name.length()));
            }
            else
            {
                displayNameLine1(name);
                displayNameLine2("");
            }
        }
        if (hasSubtitleUpdate())
        {
            Serial.println("Update Subtitle:" + getSubtitle());
            displayNameLine3(getSubtitle());
        }
        if (hasImageUrlUpdate())
        {
            Serial.println("Update ImageUrl:" + getImageUrl());
            displayJpg("/cover.jpg", 20, 20, 2);
        }
    }
}
