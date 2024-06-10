#ifndef WiFiSetup_h

#define WiFiSetup_h
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <mDNS.h>

extern WiFiMulti wifiMulti;
bool hasWiFiStatusUpdate();
bool getWiFiStatus();
bool initWiFi(const char *wiFiSsid, const char *WiFiPass, const char *mdnsName);
void updateWiFi();
#endif