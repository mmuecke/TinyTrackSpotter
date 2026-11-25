#include <Arduino.h>
#include "Settings.h"
#include "Spotify.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LittleFS.h"
#include <WiFi.h>

enum SpotifyState
{
    INIT,
    UPDATE,
    UPDATEIMAGE,
    REFRESHTOKEN,
    WAIT
};

bool hasData = false;
static SpotifyPlaybackStatus playbackStatus = PLAYBACK_LOADING;
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
static float progressMs = 0;
static float durationMs = 0;
static unsigned long progressSampleAt = 0;
bool isPlaying = false;
bool last_isPlaying = false;
int lastUpdate = millis();
static SpotifyAuthStatus spotifyAuthStatus = SPOTIFY_AUTH_NOT_CONFIGURED;
static bool imageDownloadFailed = false;
static bool authCheckRequested = true;
static unsigned long lastAuthCheck = 0;
static const unsigned long authCheckIntervalMs = 10000;
static bool refreshHadConnectionError = false;
static bool refreshAccessToken();

SpotifyPlaybackStatus getPlaybackStatus()
{
    return playbackStatus;
}

SpotifyArtworkStatus getArtworkStatus()
{
    if (imageUrl.length() == 0)
        return ARTWORK_NONE;
    if (imageDownloadFailed)
        return ARTWORK_ERROR;
    return downloaded_imageUrl == imageUrl ? ARTWORK_READY : ARTWORK_LOADING;
}

static void clearPlaybackData(SpotifyPlaybackStatus status)
{
    hasData = false;
    name = "";
    subtitle = "";
    imageUrl = "";
    progress = 0;
    progressMs = 0;
    durationMs = 0;
    progressSampleAt = millis();
    isPlaying = false;
    playbackStatus = status;
}

SpotifyAuthStatus getSpotifyAuthStatus()
{
    return spotifyAuthStatus;
}

const char *getSpotifyAuthStatusName()
{
    switch (spotifyAuthStatus)
    {
    case SPOTIFY_AUTH_NOT_CONFIGURED:
        return "not_configured";
    case SPOTIFY_AUTH_REQUIRED:
        return "authentication_required";
    case SPOTIFY_AUTH_CONNECTED:
        return "connected";
    case SPOTIFY_AUTH_CONNECTION_ERROR:
        return "connection_error";
    default:
        return "connection_error";
    }
}

void spotifyAuthSettingsChanged()
{
    authCheckRequested = true;
    spotifyAuthStatus = SPOTIFY_AUTH_NOT_CONFIGURED;
}

void checkSpotifyAuthentication()
{
    String clientId = getSetting("clientId", true);
    String clientSecret = getSetting("clientSecret", true);
    String refreshToken = getSetting("refreshToken", true);
    if (clientId.length() == 0 || clientSecret.length() == 0 || refreshToken.length() == 0)
    {
        spotifyAuthStatus = SPOTIFY_AUTH_NOT_CONFIGURED;
        authCheckRequested = false;
        return;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        spotifyAuthStatus = SPOTIFY_AUTH_CONNECTION_ERROR;
        authCheckRequested = true;
        return;
    }

    authCheckRequested = false;
    lastAuthCheck = millis();
    if (refreshAccessToken())
    {
        spotifyAuthStatus = SPOTIFY_AUTH_CONNECTED;
        spotifyState = UPDATE;
        lastUpdate = millis();
    }
    else
        spotifyAuthStatus = refreshHadConnectionError ? SPOTIFY_AUTH_CONNECTION_ERROR : SPOTIFY_AUTH_REQUIRED;
}

void updateSpotifyAuthentication()
{
    if (WiFi.status() != WL_CONNECTED)
    {
        if (spotifyAuthStatus != SPOTIFY_AUTH_NOT_CONFIGURED)
            spotifyAuthStatus = SPOTIFY_AUTH_CONNECTION_ERROR;
        authCheckRequested = true;
        return;
    }

    if (authCheckRequested || (spotifyAuthStatus == SPOTIFY_AUTH_CONNECTION_ERROR && millis() - lastAuthCheck >= authCheckIntervalMs))
        checkSpotifyAuthentication();
}

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
    last_ImageUrl = imageUrl;
    return imageUrl;
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
    float currentProgress = progress;
    if (isPlaying && durationMs > 0)
    {
        float currentProgressMs = progressMs + (unsigned long)(millis() - progressSampleAt);
        if (currentProgressMs > durationMs)
            currentProgressMs = durationMs;
        currentProgress = currentProgressMs / durationMs;
    }
    last_progress = currentProgress;
    return currentProgress;
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

static String selectArtworkUrl(JsonArray images)
{
    String fallbackUrl;
    String closestCoverUrl;
    int closestCoverWidth = 32767;
    int largestWidth = -1;
    String largestUrl;

    for (JsonVariant image : images)
    {
        String url = image["url"].as<String>();
        if (url.length() == 0)
            continue;
        if (fallbackUrl.length() == 0)
            fallbackUrl = url;

        int width = image["width"].as<int>();
        if (width > largestWidth)
        {
            largestWidth = width;
            largestUrl = url;
        }
        if (width >= 160 && width < closestCoverWidth)
        {
            closestCoverWidth = width;
            closestCoverUrl = url;
        }
    }

    if (closestCoverUrl.length() > 0)
        return closestCoverUrl;
    return largestUrl.length() > 0 ? largestUrl : fallbackUrl;
}

boolean getCurrentlyPlaying()
{
    String accessToken = getSetting("accessToken", true);
    HTTPClient http;
    http.begin("https://api.spotify.com/v1/me/player/currently-playing?additional_types=episode"); // Specify the URL
    http.addHeader("Content-Type", "application/json");                                            // Specify the content-type header
    http.addHeader("Authorization", "Bearer " + accessToken);                                      // Add the access token to the Authorization header
    int httpCode = http.GET();                                                                     // Make the HTTP request

    Serial.println("Spotify playback HTTP status: " + String(httpCode));
    if (httpCode == 401)
    {
        http.end();
        clearPlaybackData(PLAYBACK_ERROR);
        spotifyAuthStatus = SPOTIFY_AUTH_REQUIRED;
        authCheckRequested = false;
        return false;
    }
    if (httpCode == 204)
    {
        Serial.println("Spotify playback: no active item (204)");
        http.end();
        clearPlaybackData(PLAYBACK_NOTHING);
        return true;
    }
    if (httpCode != 200)
    {
        http.end();
        clearPlaybackData(PLAYBACK_ERROR);
        return true;
    }

    String payload = http.getString();
    http.end();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.println("Spotify playback JSON error: " + String(error.f_str()));
        clearPlaybackData(PLAYBACK_ERROR);
        return true;
    }

    JsonObject currentPlaying = doc.as<JsonObject>();
    if (!currentPlaying.containsKey("item") || currentPlaying["item"].isNull())
    {
        Serial.println("Spotify playback: response has no item");
        clearPlaybackData(PLAYBACK_NOTHING);
        return true;
    }

    JsonObject item = currentPlaying["item"].as<JsonObject>();
    String trackTitle = item["name"].as<String>();
    String itemType = currentPlaying["currently_playing_type"].as<String>();
    Serial.printf("Playback item: type='%s', name='%s'\n", itemType.c_str(), trackTitle.c_str());
    if (trackTitle.length() == 0)
    {
        Serial.println("Spotify playback: item name is empty");
        clearPlaybackData(PLAYBACK_NOTHING);
        return true;
    }

    String trackSubtitle = "";
    if (item.containsKey("show") && !item["show"].isNull())
    {
        JsonObject show = item["show"].as<JsonObject>();
        trackSubtitle = show["name"].as<String>();
        Serial.printf("Playback podcast show: '%s'\n", trackSubtitle.c_str());
    }
    if (trackSubtitle.length() == 0 && item.containsKey("show_name"))
        trackSubtitle = item["show_name"].as<String>();

    if (trackSubtitle.length() == 0 && item.containsKey("artists"))
    {
        JsonArray artists = item["artists"].as<JsonArray>();
        Serial.printf("Playback artist count: %u\n", (unsigned)artists.size());
        for (JsonVariant artist : artists)
        {
            String artistName = artist["name"].as<String>();
            if (artistName.length() > 0)
            {
                Serial.printf("Playback artist: '%s'\n", artistName.c_str());
                if (trackSubtitle.length() > 0)
                    trackSubtitle += ", ";
                trackSubtitle += artistName;
            }
        }
    }

    if (trackSubtitle.length() == 0 && item.containsKey("album") && !item["album"].isNull())
    {
        JsonObject album = item["album"].as<JsonObject>();
        JsonArray artists = album["artists"].as<JsonArray>();
        if (artists.size() > 0)
            Serial.printf("Playback artist fallback count (album): %u\n", (unsigned)artists.size());
        for (JsonVariant artist : artists)
        {
            String artistName = artist["name"].as<String>();
            if (artistName.length() > 0)
            {
                if (trackSubtitle.length() > 0)
                    trackSubtitle += ", ";
                trackSubtitle += artistName;
                Serial.printf("Playback album artist: '%s'\n", artistName.c_str());
            }
        }
    }
    if (trackSubtitle.length() == 0)
    {
        Serial.printf("Playback item '%s' has no artist/show subtitle fields\n", trackTitle.c_str());
    }

    float trackDurationMs = item["duration_ms"].as<float>();
    float trackProgressMs = currentPlaying["progress_ms"].as<float>();
    float trackProgress = trackDurationMs > 0 ? trackProgressMs / trackDurationMs : 0;
    if (trackProgress < 0)
        trackProgress = 0;
    if (trackProgress > 1)
        trackProgress = 1;

    String artworkUrl = "";
    if (item.containsKey("album") && !item["album"].isNull())
    {
        JsonObject album = item["album"].as<JsonObject>();
        JsonArray images = album["images"].as<JsonArray>();
        artworkUrl = selectArtworkUrl(images);
    }
    if (artworkUrl.length() == 0)
    {
        JsonArray images = item["images"].as<JsonArray>();
        artworkUrl = selectArtworkUrl(images);
    }

    if (artworkUrl != imageUrl)
    {
        downloaded_imageUrl = "";
        imageDownloadFailed = false;
    }
    name = trackTitle;
    subtitle = trackSubtitle;
    progress = trackProgress;
    progressMs = trackProgressMs;
    durationMs = trackDurationMs;
    isPlaying = currentPlaying["is_playing"].as<bool>();
    progressSampleAt = millis();
    imageUrl = artworkUrl;
    hasData = true;
    playbackStatus = isPlaying ? PLAYBACK_PLAYING : PLAYBACK_PAUSED;
    Serial.printf("Playback parsed: title='%s', subtitle='%s', progress=%.3f, playing=%s\n",
                  name.c_str(), subtitle.c_str(), progress, isPlaying ? "yes" : "no");
    return true;
}

static bool refreshAccessToken()
{
    String clientId = getSetting("clientId", true);
    String clientSecret = getSetting("clientSecret", true);
    String refreshToken = getSetting("refreshToken", true);
    HTTPClient http;
    http.begin("https://accounts.spotify.com/api/token");                // Specify the URL
    http.addHeader("Content-Type", "application/x-www-form-urlencoded"); // Specify the content-type header
    // http.addHeader("Authorization", "Basic " + clientId + ":" + clientSecret);  // Authorization header

    if (clientId.length() == 0 || clientSecret.length() == 0 || refreshToken.length() == 0)
    {
        http.end();
        spotifyAuthStatus = SPOTIFY_AUTH_NOT_CONFIGURED;
        return false;
    }

    String requestBody = "grant_type=refresh_token";
    requestBody += "&refresh_token=" + refreshToken;
    requestBody += "&client_id=" + clientId;
    requestBody += "&client_secret=" + clientSecret;

    int httpCode = http.POST(requestBody); // Make the HTTP request
    refreshHadConnectionError = httpCode < 0 || httpCode == 429 || httpCode >= 500;

    bool refreshed = false;
    if (httpCode == 200)
    {                                      // Check the returning code
        String payload = http.getString(); // Get the request response payload

        Serial.println("HTTP Code: " + String(httpCode)); // Log httpCode to console
        Serial.println("Payload: " + payload);            // Log http response payload
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        JsonObject root = doc.as<JsonObject>(); // Convert the payload to JsonObject

        String accessToken = root["access_token"].as<String>();
        refreshed = !error && accessToken.length() > 0 && trySetSetting("accessToken", accessToken, true);
    }

    http.end(); // Close connection
    if (!refreshed)
        Serial.println("Spotify authentication check failed (HTTP " + String(httpCode) + ")");
    return refreshed;
}

boolean downloadImage()
{
    if (imageUrl.length() == 0)
        return false;

    HTTPClient http;
    if (!http.begin(imageUrl))
    {
        Serial.println("Failed to start cover-art request");
        return false;
    }
    int httpCode = http.GET();
    if (httpCode != 200)
    {
        Serial.println("Failed to download image, HTTP code: " + String(httpCode));
        http.end();
        return false;
    }
    if (!LittleFS.begin())
    {
        Serial.println("Error mounting the file system for cover art");
        http.end();
        return false;
    }
    File file = LittleFS.open("/cover.jpg", "w");
    if (!file)
    {
        Serial.println("Failed to open cover-art file for writing");
        LittleFS.end();
        http.end();
        return false;
    }

    Stream *responseStream = http.getStreamPtr();
    int expectedBytes = http.getSize();
    size_t totalBytes = 0;
    byte buffer[1024];
    unsigned long lastDataAt = millis();
    while (http.connected() && (expectedBytes < 0 || totalBytes < (size_t)expectedBytes))
    {
        int availableBytes = responseStream->available();
        if (availableBytes <= 0)
        {
            if (millis() - lastDataAt > 3000)
                break;
            delay(1);
            continue;
        }
        size_t bytesToRead = (size_t)availableBytes;
        if (bytesToRead > sizeof(buffer))
            bytesToRead = sizeof(buffer);
        if (expectedBytes >= 0 && bytesToRead > (size_t)expectedBytes - totalBytes)
            bytesToRead = (size_t)expectedBytes - totalBytes;
        size_t count = responseStream->readBytes(buffer, bytesToRead);
        if (count == 0)
            break;
        size_t written = file.write(buffer, count);
        totalBytes += written;
        lastDataAt = millis();
        if (written != count)
            break;
    }
    file.close();
    LittleFS.end();
    http.end();

    return totalBytes > 0 && (expectedBytes <= 0 || totalBytes == (size_t)expectedBytes);
}

static void removeCoverImage()
{
    if (!LittleFS.begin())
        return;
    if (LittleFS.exists("/cover.jpg"))
        LittleFS.remove("/cover.jpg");
    LittleFS.end();
}

void updateCurrentPlaying()
{
    if (spotifyState == REFRESHTOKEN)
    {
        Serial.println("Refreshing Spotify access token");
        if (refreshAccessToken())
        {
            spotifyAuthStatus = SPOTIFY_AUTH_CONNECTED;
            spotifyState = UPDATE;
        }
        else
        {
            spotifyAuthStatus = refreshHadConnectionError ? SPOTIFY_AUTH_CONNECTION_ERROR : SPOTIFY_AUTH_REQUIRED;
            spotifyState = WAIT;
            lastUpdate = millis();
        }
        return;
    }

    if (spotifyAuthStatus != SPOTIFY_AUTH_CONNECTED)
        return;

    switch (spotifyState)
    {
    case INIT:
        spotifyState = UPDATE;
        break;
    case UPDATE:
        Serial.println("Updating current playing");
        if (!getCurrentlyPlaying())
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
            downloaded_imageUrl = "";
            if (imageUrl.length() > 0)
            {
                Serial.println("Downloading cover art");
                if (downloadImage())
                {
                    downloaded_imageUrl = imageUrl;
                    imageDownloadFailed = false;
                }
                else
                {
                    imageDownloadFailed = true;
                    removeCoverImage();
                }
            }
            else
            {
                removeCoverImage();
            }
        }
        spotifyState = WAIT;
        lastUpdate = millis();
        break;
    case WAIT:
    {
        bool trackEnded = isPlaying && durationMs > 0 &&
                          (unsigned long)(millis() - progressSampleAt) >=
                              (unsigned long)max(0.0f, durationMs - progressMs);
        if (trackEnded || millis() - lastUpdate > 2000)
        {
            Serial.println(trackEnded ? "Track ended; refreshing playback" : "Go to next update");
            spotifyState = UPDATE;
            lastUpdate = millis();
        }
        break;
    }
    default:
        break;
    }
}
