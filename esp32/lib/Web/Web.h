#ifndef Web_h

#define Web_h

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

extern AsyncWebServer server;

bool initWebServer();

#endif