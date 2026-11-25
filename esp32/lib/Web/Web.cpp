#include <Arduino.h>
#include "Web.h"
#include "LittleFS.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "Settings.h"
#include "Spotify.h"
// Port des Webservers auf 80 setzen
AsyncWebServer server(80);

static void sendStaticFile(AsyncWebServerRequest *request, const char *path, const char *contentType, bool download = false)
{
    // Settings reads unmount LittleFS after each request, so mount it again
    // before serving the next page or asset request.
    if (!LittleFS.begin())
    {
        Serial.println("Web: failed to mount LittleFS while serving " + String(path));
        request->send(500, "text/plain", "LittleFS mount failed; upload the filesystem image");
        return;
    }
    if (!LittleFS.exists(path))
    {
        Serial.println("Web: missing file " + String(path) + "; upload the filesystem image");
        request->send(404, "text/plain", "Web page missing; upload the filesystem image");
        return;
    }
    request->send(LittleFS, path, contentType, download);
}

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
    if (request->argName(zeroIndex) == "clientId" || request->argName(zeroIndex) == "clientSecret" || request->argName(zeroIndex) == "refreshToken")
        spotifyAuthSettingsChanged();
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
              { sendStaticFile(request, "/index.html", "text/html"); });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/index.html", "text/html"); });
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/style.css", "text/css"); });
    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/favicon.ico", "image/x-icon"); });
    server.on("/icons/favicon-16x16.png", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/icons/favicon-16x16.png", "image/png"); });
    server.on("/icons/favicon-32x32.png", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/icons/favicon-32x32.png", "image/png"); });
    server.on("/icons/favicon-96x96.png", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/icons/favicon-96x96.png", "image/png"); });
    server.on("/icons/favicon-128x128.png", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/icons/favicon-128x128.png", "image/png"); });
    server.on("/spotify_oauth_helper.py", HTTP_GET, [](AsyncWebServerRequest *request)
              { sendStaticFile(request, "/spotify_oauth_helper.py", "application/octet-stream", true); });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request)
              { handelGetSettingsRequest(request); });
    server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request)
              { handelSetSettingsRequest(request); });
    server.on("/auth/status", HTTP_GET, [](AsyncWebServerRequest *request)
              {
                  String deviceIp = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
                  String body = "{\"status\":\"" + String(getSpotifyAuthStatusName()) + "\",\"device_ip\":\"" + deviceIp + "\"}";
                  request->send(200, "application/json", body);
              });
    server.begin();
    return true;
}
