
#include <Arduino.h>
#include <WiFiSetup.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>

WiFiMulti wifiMulti;
bool isConnected = false;
bool last_isConnected = false;
bool initWiFi(const char *wiFiSsid, const char *WiFiPass, const char *mdnsName)
{
    // init wifi
    wifiMulti.addAP(wiFiSsid, WiFiPass);
    Serial.print("Waiting for WiFi to connect...");

    while ((wifiMulti.run() != WL_CONNECTED))
    {
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    if (!MDNS.begin(mdnsName))
    {
        Serial.println("Failed to set up DNS address: " + String(mdnsName));
    }
    Serial.println("DNS address: " + String(mdnsName));
    return true;
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
    isConnected = (WiFi.status() == WL_CONNECTED);
    //  MDNS.update();
}