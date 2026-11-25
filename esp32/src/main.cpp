#include <Arduino.h>
#include "Display.h"
#include "HTTPClient.h"
#include "Settings.h"
#include "SettingsEntry.h"
#include "Web.h"
#include "WiFiSetup.h"
#include "Spotify.h"
#include "Screan.h"

// Daten des WiFi-Netzwerks
char const *WIFI_SSID = "FRITZ!Box 7530 GS";
char const *WIFI_PASS = "07546095582494095943";
char const *MDNS_NAME = "spotifyDisplay";
SpotifyAuthStatus lastDisplayedAuthStatus = SPOTIFY_AUTH_NOT_CONFIGURED;

// // Define an array with TFT colors
// uint16_t tftColors[] = {
//     TFT_RED,
//     TFT_GREEN,
//     TFT_BLUE,
//     TFT_YELLOW,
//     TFT_CYAN,
//     TFT_MAGENTA,
//     TFT_WHITE,
//     TFT_BLACK};

void setup()
{
  Serial.begin(19200);
  Serial.println("Starting device");

  bool success = initDisplay();
  if (!success)
  {
    Serial.println("Error initializing display");
    return;
  }
  initScrean();
  Serial.println("Initialized display");
  displayPrintLine("Initialized display");

  success = initSettings();
  if (!success)
  {
    Serial.println("Error initializing settings");
    return;
  }
  Serial.println("Initialized settings");
  displayPrintLine("Initialized settings");

  displayPrintLine("Connecting WiFi: " + String(WIFI_SSID));
  success = initWiFi(WIFI_SSID, WIFI_PASS, MDNS_NAME);
  if (!success)
  {
    Serial.println("Wi-Fi unavailable; continuing startup with background retries");
    displayPrintLine("WiFi offline; retrying");
  }
  else
  {
    Serial.println("Initialized WiFi");
    displayPrintLine("Initialized WiFi");
  }

  checkSpotifyAuthentication();
  lastDisplayedAuthStatus = getSpotifyAuthStatus();

  success = initWebServer();
  if (!success)
  {
    Serial.println("Error initializing web server");
    return;
  }
  displayPrintLine("Initialized web server");

  // success = initSpotify();
  // if (!success)
  // {
  //   Serial.println("Error initializing Spotify");
  //   return;
  // }
  // Serial.println("Initialized Spotify");
  // displayPrintLine("Initialized Spotify");

  // displayPrintLine("Initialization complete");
  Serial.println("Startup complete");
  displaySpotifyAuthStatus(lastDisplayedAuthStatus);
}

void loop()
{
  updateWiFi();
  updateSpotifyAuthentication();
  SpotifyAuthStatus authStatus = getSpotifyAuthStatus();
  if (authStatus != lastDisplayedAuthStatus)
  {
    lastDisplayedAuthStatus = authStatus;
    displaySpotifyAuthStatus(authStatus);
  }
  // if (hasWiFiStatusUpdate() && getWiFiStatus())
  // {
  //   displayPrintLine(String(MDNS_NAME) + ".local");
  // }
  // updateCurrentPlaying();
  // updateScrean();
}
