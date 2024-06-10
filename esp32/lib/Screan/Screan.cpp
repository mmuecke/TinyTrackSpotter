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
    displayText(name, 220, 20, &FreeSans18pt7b, 306, background, color);
}

void displayNameLine2(String name)
{
    displayText(name, 220, 60, &FreeSans18pt7b, 306, background, color);
}

void displayNameLine3(String subtitle)
{
    displayText(subtitle, 220, 110, &FreeSans12pt7b, 306, background, color);
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
        if (hasImageUrlUpdate())
        {
            Serial.println("Update ImageUrl:" + getImageUrl());
            // displayJpg("/cover.jpg", 10, 10);
        }
        if (hasNameUpdate())
        {
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
                displayNameLine2(name.substring(breakPoint, name.length()));
            }
            else
            {
                displayNameLine1("");
                displayNameLine2(name);
            }
        }
        if (hasSubtitleUpdate())
        {
            Serial.println("Update Subtitle:" + getSubtitle());
            displayNameLine3(getSubtitle());
        }
        if (hasProgressUpdate())
        {
            Serial.println("Update Progress:" + String(getProgress()));
        }
        if (hasIsPlayingUpdate())
        {
            Serial.println("Update IsPlaying:" + getIsPlaing());
        }
    }
}
