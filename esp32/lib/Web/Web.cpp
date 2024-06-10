#include <Arduino.h>
#include "Web.h"
#include "LittleFS.h"
#include <ESPAsyncWebServer.h>
#include "Settings.h"
// Port des Webservers auf 80 setzen
AsyncWebServer server(80);

bool checkSingleArg(AsyncWebServerRequest *request)
{
    size_t args = request->args();
    if (args == 0)
    {
        request->send(400, "text/plain", "Bad Request: No Arguments");
        return false;
    }
    if (args > 1)
    {
        request->send(400, "text/plain", "Bad Request: Too many Arguments");
        return false;
    }
    String key = request->argName(0);
    if (!hasSetting(key))
    {
        request->send(400, "text/plain", "Bad Request: Unknown Setting");
        return false;
    }
    return true;
}

void handelSetSettingsRequest(AsyncWebServerRequest *request)
{
    if (!checkSingleArg(request))
    {
        return;
    }
    const size_t zeroIndex = 0;

    if (!trySetSetting(request->argName(zeroIndex), request->arg(zeroIndex)))
    {
        AsyncWebServerResponse *response = request->beginResponse(500, "text/plain", "Internal Server Error: Failed to write setting");
        response->addHeader("Access-Control-Allow-Origin", "*");
        request->send(response);
        // request->send(500, "text/plain", "Internal Server Error: Failed to write setting");
        return;
    }
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "OK");
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
    // request->send(200, "text/plain", "OK");
}

void handelGetSettingsRequest(AsyncWebServerRequest *request)
{
    if (!checkSingleArg(request))
    {
        return;
    }
    String value = getSetting(request->argName(0), true);
    AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", value);
    response->addHeader("Access-Control-Allow-Origin", "*");
    request->send(response);
    // request->send(200, "text/plain", value);
}

bool initWebServer()
{
    // init webserver
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "NotFund"); });
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", String(), false); });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.html", String(), false); });
    server.on("/index.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/index.js", String(), false); });
    server.on("/settings.js", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/settings.js", String(), false); });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/style.css", String(), false); });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
              { handelGetSettingsRequest(request); });
    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request)
              { handelSetSettingsRequest(request); });
    server.begin();
    return true;
}
