#include <Arduino.h>
#include "Settings.h"
#include "Spotify.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LittleFS.h"

enum SpotifyState
{
    INIT,
    UPDATE,
    UPDATEIMAGE,
    REFRESHTOKEN,
    WAIT
};

bool hasData = false;
SpotifyState spotifyState = INIT;
String imageUrl = "";
String downloaded_imageUrl = "";
String last_ImageUrl = "";
String name = "";
String last_name = "";
String subtitle = "";
String last_subtitle = "";
float progress = 0;
float last_progress = 0;
bool isPlaying = false;
bool last_isPlaying = false;
int lastUpdate = millis();

boolean getHasData()
{
    return hasData;
}

boolean hasImageUrlUpdate()
{
    return downloaded_imageUrl != last_ImageUrl;
}

String getImageUrl()
{
    last_ImageUrl = downloaded_imageUrl;
    return downloaded_imageUrl;
}

boolean hasNameUpdate()
{
    return name != last_name;
}

String getName()
{
    last_name = name;
    return name;
}

boolean hasSubtitleUpdate()
{
    return subtitle != last_subtitle;
}

String getSubtitle()
{
    last_subtitle = subtitle;
    return subtitle;
}

boolean hasProgressUpdate()
{
    return progress != last_progress;
}

float getProgress()
{
    last_progress = progress;
    return progress;
}

boolean hasIsPlayingUpdate()
{
    return isPlaying != last_isPlaying;
}

boolean getIsPlaing()
{
    last_isPlaying = isPlaying;
    return isPlaying;
}

boolean getCurrentlyPlaying()
{
    String accessToken = getSetting("accessToken", true);
    Serial.println("accessToken: " + accessToken);
    HTTPClient http;
    http.begin("https://api.spotify.com/v1/me/player/currently-playing?additional_types=episode"); // Specify the URL
    http.addHeader("Content-Type", "application/json");                                            // Specify the content-type header
    http.addHeader("Authorization", "Bearer " + accessToken);                                      // Add the access token to the Authorization header
    int httpCode = http.GET();                                                                     // Make the HTTP request

    Serial.println("HTTP Code: " + String(httpCode)); // Log httpCode to console
    if (httpCode > 0)
    { // Check the returning code
        if (httpCode == 401)
        {
            Serial.println("Unauthorized, refreshing token");
            return false;
        }
        if (httpCode == 204)
        {
            Serial.println("No content");
            return true;
        }
        if (httpCode != 200)
        {
            return false;
        }
        String payload = http.getString(); // Get the request response payload
        // Serial.println("Payload: " + payload); // Log payload to console
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error)
        {
            Serial.println("deserializeJson() failed: " + String(error.f_str()));
            return false;
        }

        JsonObject currentPlaying = doc.as<JsonObject>(); // Convert the payload to JsonObject

        name = "";
        if (currentPlaying.containsKey("item"))
        {
            JsonObject item = currentPlaying["item"].as<JsonObject>();
            if (item.containsKey("show"))
            {
                JsonObject show = item["show"].as<JsonObject>();
                if (show.containsKey("name"))
                {
                    name = show["name"].as<String>();
                }
            }
            if (name == "" && item.containsKey("name"))
            {
                name = item["name"].as<String>();
            }
        }

        subtitle = "";
        if (currentPlaying.containsKey("item"))
        {
            JsonObject item = currentPlaying["item"].as<JsonObject>();
            if (item.containsKey("artists"))
            {
                JsonArray artists = item["artists"].as<JsonArray>();
                for (JsonVariant artist : artists)
                {
                    JsonObject artistObj = artist.as<JsonObject>();
                    if (artistObj.containsKey("name"))
                    {
                        if (subtitle != "")
                        {
                            subtitle += ", ";
                        }
                        subtitle += artistObj["name"].as<String>();
                    }
                }
            }
            if (subtitle == "" && item.containsKey("name"))
            {
                subtitle = item["name"].as<String>();
            }
        }

        progress = 0;
        if (currentPlaying.containsKey("progress_ms") && currentPlaying.containsKey("item"))
        {
            JsonObject item = currentPlaying["item"].as<JsonObject>();
            if (item.containsKey("duration_ms"))
            {
                float progress_ms = currentPlaying["progress_ms"].as<float>();
                float duration_ms = item["duration_ms"].as<float>();
                progress = (progress_ms && duration_ms) ? progress_ms / duration_ms : 0;
            }
        }

        isPlaying = false;
        if (currentPlaying.containsKey("is_playing"))
        {
            isPlaying = currentPlaying["is_playing"].as<bool>();
        }

        imageUrl = "";
        if (currentPlaying.containsKey("item"))
        {
            JsonObject item = currentPlaying["item"].as<JsonObject>();
            if (item.containsKey("album"))
            {
                JsonObject album = item["album"].as<JsonObject>();
                if (album.containsKey("images"))
                {
                    JsonArray images = album["images"].as<JsonArray>();
                    if (images.size() > 1)
                    {
                        JsonObject image = images[1].as<JsonObject>();
                        if (image.containsKey("url"))
                        {
                            imageUrl = image["url"].as<String>();
                        }
                    }
                }
            }
            if (imageUrl == "" && item.containsKey("images"))
            {
                JsonArray images = item["images"].as<JsonArray>();
                if (images.size() > 1)
                {
                    JsonObject image = images[1].as<JsonObject>();
                    if (image.containsKey("url"))
                    {
                        imageUrl = image["url"].as<String>();
                    }
                }
            }
        }
        // Serial.println("Name: " + name);
        // Serial.println("Subtitle: " + subtitle);
        // Serial.println("Progress: " + String(progress));
        // Serial.println("Playing: " + String(isPlaying));

        hasData = true;
    }

    http.end(); // Close connection

    return true;
}

void refreshAccessToken()
{
    String clientId = getSetting("clientId", true);
    String clientSecret = getSetting("clientSecret", true);
    String refreshToken = getSetting("refreshToken", true);
    HTTPClient http;
    http.begin("https://accounts.spotify.com/api/token");                // Specify the URL
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Specify the content-type header
    // http.addHeader("Authorization", "Basic " + clientId + ":" + clientSecret);  // Authorization header

    String requestBody = "grant_type=refresh_token";
    requestBody += "&refresh_token=" + refreshToken;
    requestBody += "&client_id=" + clientId;
    requestBody += "&client_secret=" + clientSecret;

    int httpCode = http.POST(requestBody); // Make the HTTP request

    if (httpCode > 0)
    {                                      // Check the returning code
        String payload = http.getString(); // Get the request response payload

        Serial.println("HTTP Code: " + String(httpCode)); // Log httpCode to console
        Serial.println("Payload: " + payload);            // Log payload to console

        JsonDocument doc;
        deserializeJson(doc, payload);
        JsonObject root = doc.as<JsonObject>(); // Convert the payload to JsonObject

        String accessToken = root["access_token"].as<String>();
        trySetSetting("accessToken", accessToken, true);
    }

    http.end(); // Close connection
                /* code */
}

boolean downloadImage()
{
    if (imageUrl.length() > 0)
    {
        HTTPClient http;
        http.begin(imageUrl);
        int httpCode = http.GET();
        if (httpCode == 200)
        { // Check for the returning code
            if (!LittleFS.begin())
            {
                Serial.println("Error mounting the file system");
                LittleFS.end();
                return false;
            }
            File file = LittleFS.open("/cover.jpg", "w");
            if (!file)
            {
                Serial.println("Failed to open file for writing");
                LittleFS.end();
                return false;
            }
            // Get the image data
            Stream *responseStream = http.getStreamPtr();
            // Write the image data to a file
            byte buffer[1024] = {0};
            while (responseStream->available())
            {
                size_t count = responseStream->readBytes(buffer, sizeof(buffer));
                file.write(buffer, count);
            }
            file.close();
            LittleFS.end();
        }
        else
        {
            Serial.println("Failed to download image, HTTP code: " + String(httpCode));
        }
        http.end();

        return true;
    }
}

void updateCurrentPlaying()
{
    bool success; // Declare success outside the switch statement
    switch (spotifyState)
    {
    case INIT:
        spotifyState = UPDATE;
        break;
    case UPDATE:
        Serial.println("Updating current playing");
        success = getCurrentlyPlaying();
        if (!success)
        {
            spotifyState = REFRESHTOKEN;
        }
        else
        {
            spotifyState = UPDATEIMAGE;
        }
        break;
    case UPDATEIMAGE:
        if (imageUrl != downloaded_imageUrl)
        {
            Serial.println("Downloading image");
            downloadImage();
            downloaded_imageUrl = imageUrl;
        }
        spotifyState = WAIT;
        break;
    case REFRESHTOKEN:
        Serial.println("Refreshing token");
        refreshAccessToken();
        spotifyState = UPDATE;
        break;
    case WAIT:
        if (millis() - lastUpdate > 10000)
        {
            Serial.println("Go to next update");
            spotifyState = UPDATE;
            lastUpdate = millis();
        }
        break;
    default:
        break;
    }
}
