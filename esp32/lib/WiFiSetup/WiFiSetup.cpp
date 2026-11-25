
#include <Arduino.h>
#include <WiFiSetup.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>

WiFiMulti wifiMulti;
bool isConnected = false;
bool last_isConnected = false;
static bool mdnsStarted = false;
static const char *configuredMdnsName = nullptr;
static unsigned long lastReconnectAttempt = 0;
static const unsigned long reconnectIntervalMs = 5000;

static void startMdnsIfNeeded(const char *mdnsName)
{
    if (!mdnsStarted && WiFi.status() == WL_CONNECTED)
    {
        mdnsStarted = MDNS.begin(mdnsName);
        Serial.printf("mDNS %s: %s.local\n", mdnsStarted ? "started" : "failed", mdnsName);
    }
}

bool initWiFi(const char *wiFiSsid, const char *WiFiPass, const char *mdnsName)
{
    configuredMdnsName = mdnsName;
    wifiMulti.addAP(wiFiSsid, WiFiPass);
    WiFi.mode(WIFI_STA);
    Serial.printf("Wi-Fi: connecting to %s (timeout 15 s)\n", wiFiSsid);
    const unsigned long startedAt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 15000)
    {
        wifiMulti.run();
        delay(100);
    }
    isConnected = WiFi.status() == WL_CONNECTED;
    last_isConnected = !isConnected;
    if (isConnected)
    {
        Serial.printf("Wi-Fi connected; IP address: %s\n", WiFi.localIP().toString().c_str());
        startMdnsIfNeeded(mdnsName);
        return true;
    }
    Serial.println("Wi-Fi: connection timed out; startup will continue and retry in background");
    return false;
}

bool hasWiFiStatusUpdate()
{
    return isConnected != last_isConnected;
}

bool getWiFiStatus()
{
    last_isConnected = isConnected;
    return isConnected;
}

void updateWiFi()
{
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected != isConnected)
    {
        isConnected = connected;
        Serial.printf("Wi-Fi status: %s\n", isConnected ? "connected" : "disconnected");
        if (isConnected)
        {
            Serial.printf("Wi-Fi IP address: %s\n", WiFi.localIP().toString().c_str());
            if (configuredMdnsName != nullptr)
                startMdnsIfNeeded(configuredMdnsName);
        }
    }
    if (!connected && millis() - lastReconnectAttempt >= reconnectIntervalMs)
    {
        lastReconnectAttempt = millis();
        Serial.println("Wi-Fi: retrying connection");
        wifiMulti.run();
    }
}
