#include <Arduino.h>
#include "Settings.h"
#include "Spotify.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "LittleFS.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

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
volatile SpotifyState spotifyState = INIT;
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
static String playbackItemId = "";
static uint64_t playbackTimestamp = 0;
bool isPlaying = false;
bool last_isPlaying = false;
volatile int lastUpdate = millis();
static volatile SpotifyAuthStatus spotifyAuthStatus = SPOTIFY_AUTH_NOT_CONFIGURED;
static bool imageDownloadFailed = false;
static volatile bool authCheckRequested = true;
static unsigned long lastAuthCheck = 0;
static const unsigned long authCheckIntervalMs = 10000;
static bool refreshHadConnectionError = false;
static bool refreshAccessToken();
static SemaphoreHandle_t playbackMutex = nullptr;
static QueueHandle_t spotifyCommandQueue = nullptr;
static bool playbackWorkerStarted = false;
static bool playbackWorkerStartAttempted = false;

static void lockPlaybackData()
{
    if (playbackMutex)
        xSemaphoreTake(playbackMutex, portMAX_DELAY);
}

static void unlockPlaybackData()
{
    if (playbackMutex)
        xSemaphoreGive(playbackMutex);
}

SpotifyPlaybackStatus getPlaybackStatus()
{
    lockPlaybackData();
    SpotifyPlaybackStatus status = playbackStatus;
    unlockPlaybackData();
    return status;
}

SpotifyArtworkStatus getArtworkStatus()
{
    lockPlaybackData();
    SpotifyArtworkStatus status;
    if (imageUrl.length() == 0)
        status = ARTWORK_NONE;
    else if (imageDownloadFailed)
        status = ARTWORK_ERROR;
    else
        status = downloaded_imageUrl == imageUrl ? ARTWORK_READY : ARTWORK_LOADING;
    unlockPlaybackData();
    return status;
}

static void clearPlaybackData(SpotifyPlaybackStatus status)
{
    lockPlaybackData();
    hasData = false;
    name = "";
    subtitle = "";
    imageUrl = "";
    progress = 0;
    progressMs = 0;
    durationMs = 0;
    progressSampleAt = millis();
    playbackItemId = "";
    playbackTimestamp = 0;
    isPlaying = false;
    playbackStatus = status;
    unlockPlaybackData();
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
    lockPlaybackData();
    bool value = hasData;
    unlockPlaybackData();
    return value;
}

boolean hasImageUrlUpdate()
{
    lockPlaybackData();
    bool changed = downloaded_imageUrl != last_ImageUrl;
    unlockPlaybackData();
    return changed;
}

String getImageUrl()
{
    lockPlaybackData();
    last_ImageUrl = imageUrl;
    String value = imageUrl;
    unlockPlaybackData();
    return value;
}

boolean hasNameUpdate()
{
    lockPlaybackData();
    bool changed = name != last_name;
    unlockPlaybackData();
    return changed;
}

String getName()
{
    lockPlaybackData();
    last_name = name;
    String value = name;
    unlockPlaybackData();
    return value;
}

boolean hasSubtitleUpdate()
{
    lockPlaybackData();
    bool changed = subtitle != last_subtitle;
    unlockPlaybackData();
    return changed;
}

String getSubtitle()
{
    lockPlaybackData();
    last_subtitle = subtitle;
    String value = subtitle;
    unlockPlaybackData();
    return value;
}

boolean hasProgressUpdate()
{
    lockPlaybackData();
    bool changed = progress != last_progress;
    unlockPlaybackData();
    return changed;
}

float getProgress()
{
    lockPlaybackData();
    float currentProgress = progress;
    if (isPlaying && durationMs > 0)
    {
        float currentProgressMs = progressMs + (unsigned long)(millis() - progressSampleAt);
        if (currentProgressMs > durationMs)
            currentProgressMs = durationMs;
        currentProgress = currentProgressMs / durationMs;
    }
    last_progress = currentProgress;
    unlockPlaybackData();
    return currentProgress;
}

boolean hasIsPlayingUpdate()
{
    lockPlaybackData();
    bool changed = isPlaying != last_isPlaying;
    unlockPlaybackData();
    return changed;
}

boolean getIsPlaing()
{
    lockPlaybackData();
    last_isPlaying = isPlaying;
    bool value = isPlaying;
    unlockPlaybackData();
    return value;
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
    String trackId = item["id"].as<String>();
    if (trackId.length() == 0)
        trackId = item["uri"].as<String>();
    uint64_t stateTimestamp = currentPlaying["timestamp"].as<uint64_t>();
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

    bool trackIsPlaying = currentPlaying["is_playing"].as<bool>();
    unsigned long sampleAt = millis();
    lockPlaybackData();
    bool playbackAnchorUnchanged = trackId.length() > 0 && stateTimestamp > 0 &&
                                   trackId == playbackItemId && stateTimestamp == playbackTimestamp &&
                                   trackDurationMs == durationMs && trackIsPlaying == isPlaying;
    if (artworkUrl != imageUrl)
    {
        downloaded_imageUrl = "";
        imageDownloadFailed = false;
    }
    name = trackTitle;
    subtitle = trackSubtitle;
    progress = trackProgress;
    if (!playbackAnchorUnchanged)
    {
        progressMs = trackProgressMs;
        durationMs = trackDurationMs;
        progressSampleAt = sampleAt;
    }
    isPlaying = trackIsPlaying;
    playbackItemId = trackId;
    playbackTimestamp = stateTimestamp;
    imageUrl = artworkUrl;
    hasData = true;
    playbackStatus = isPlaying ? PLAYBACK_PLAYING : PLAYBACK_PAUSED;
    unlockPlaybackData();
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

static bool downloadImage(const String &artworkUrl)
{
    if (artworkUrl.length() == 0)
        return false;

    HTTPClient http;
    if (!http.begin(artworkUrl))
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

static void updateCurrentPlayingStep()
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
    {
        lockPlaybackData();
        String artworkUrl = imageUrl;
        bool needsDownload = artworkUrl != downloaded_imageUrl;
        if (needsDownload)
        {
            downloaded_imageUrl = "";
            imageDownloadFailed = false;
        }
        unlockPlaybackData();

        if (needsDownload)
        {
            if (artworkUrl.length() > 0)
            {
                Serial.println("Downloading cover art");
                bool downloaded = downloadImage(artworkUrl);
                lockPlaybackData();
                if (imageUrl == artworkUrl)
                {
                    if (downloaded)
                        downloaded_imageUrl = artworkUrl;
                    imageDownloadFailed = !downloaded;
                }
                unlockPlaybackData();
                if (!downloaded)
                    removeCoverImage();
            }
            else
            {
                removeCoverImage();
            }
        }
        spotifyState = WAIT;
        lastUpdate = millis();
        break;
    }
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

static void executeSpotifyControl(SpotifyControlCommand command)
{
    String endpoint;
    String method;
    const char *commandName;
    if (command == SPOTIFY_CONTROL_NEXT)
    {
        endpoint = "https://api.spotify.com/v1/me/player/next";
        method = "POST";
        commandName = "skip to next";
    }
    else
    {
        lockPlaybackData();
        bool currentlyPlaying = isPlaying;
        unlockPlaybackData();
        endpoint = currentlyPlaying ? "https://api.spotify.com/v1/me/player/pause"
                                    : "https://api.spotify.com/v1/me/player/play";
        method = "PUT";
        commandName = currentlyPlaying ? "pause" : "resume";
    }

    Serial.printf("Spotify control: %s\n", commandName);
    for (int attempt = 0; attempt < 2; ++attempt)
    {
        String accessToken = getSetting("accessToken", true);
        HTTPClient http;
        if (!http.begin(endpoint))
        {
            Serial.printf("ERROR: Could not start Spotify %s request\n", commandName);
            return;
        }
        http.addHeader("Authorization", "Bearer " + accessToken);
        http.addHeader("Content-Length", "0");
        int httpCode = http.sendRequest(method.c_str());
        http.end();

        if (httpCode == 204)
        {
            Serial.printf("Spotify control succeeded: %s\n", commandName);
            spotifyState = UPDATE;
            lastUpdate = millis();
            return;
        }
        if (httpCode == 401 && attempt == 0 && refreshAccessToken())
            continue;

        if (httpCode == 401)
        {
            spotifyAuthStatus = SPOTIFY_AUTH_REQUIRED;
            authCheckRequested = false;
        }
        Serial.printf("ERROR: Spotify %s failed (HTTP %d)\n", commandName, httpCode);
        if (httpCode == 403)
            Serial.println("Playback control requires Spotify Premium and user-modify-playback-state authorization; reconnect Spotify if the scope was just added.");
        else if (httpCode == 404)
            Serial.println("Spotify has no active playback device for this command.");
        return;
    }
}

static void spotifyPlaybackWorker(void *)
{
    for (;;)
    {
        SpotifyControlCommand command;
        if (spotifyCommandQueue && xQueueReceive(spotifyCommandQueue, &command, 0) == pdTRUE)
            executeSpotifyControl(command);
        updateCurrentPlayingStep();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void updateCurrentPlaying()
{
    if (spotifyAuthStatus != SPOTIFY_AUTH_CONNECTED)
        return;
    if (playbackWorkerStarted || playbackWorkerStartAttempted)
        return;
    playbackWorkerStartAttempted = true;

    playbackMutex = xSemaphoreCreateMutex();
    if (!playbackMutex)
    {
        Serial.println("ERROR: Could not create Spotify playback mutex");
        return;
    }

    spotifyCommandQueue = xQueueCreate(4, sizeof(SpotifyControlCommand));
    if (!spotifyCommandQueue)
    {
        Serial.println("ERROR: Could not create Spotify control command queue");
        vSemaphoreDelete(playbackMutex);
        playbackMutex = nullptr;
        return;
    }

    if (xTaskCreatePinnedToCore(spotifyPlaybackWorker, "spotify-playback", 8192,
                                nullptr, 1, nullptr, 0) != pdPASS)
    {
        Serial.println("ERROR: Could not start Spotify playback worker");
        vQueueDelete(spotifyCommandQueue);
        spotifyCommandQueue = nullptr;
        vSemaphoreDelete(playbackMutex);
        playbackMutex = nullptr;
        return;
    }
    playbackWorkerStarted = true;
}

void requestSpotifyControl(SpotifyControlCommand command)
{
    if (spotifyAuthStatus != SPOTIFY_AUTH_CONNECTED || !spotifyCommandQueue)
        return;
    if (xQueueSend(spotifyCommandQueue, &command, 0) != pdTRUE)
        Serial.println("ERROR: Spotify control queue is full; button command was ignored");
}
