#include "Screan.h"
#include <WiFiSetup.h>
#include <Display.h>

static const uint16_t white = TFT_WHITE;
static const uint16_t background = ((7 & 0xF8) << 8) | ((15 & 0xFC) << 3) | (12 >> 3);
static const uint16_t primary = ((0x1D & 0xF8) << 8) | ((0xB9 & 0xFC) << 3) | (0x54 >> 3);
static const uint16_t muted = ((0xA8 & 0xF8) << 8) | ((0xB8 & 0xFC) << 3) | (0xAE >> 3);
static const uint16_t warning = TFT_YELLOW;
static const uint16_t errorColor = TFT_RED;
static const uint16_t artworkBackground = ((28 & 0xF8) << 8) | ((40 & 0xFC) << 3) | (32 >> 3);

static bool playbackScreenDrawn = false;
static String drawnTitle = "";
static String drawnSubtitle = "";
static String drawnArtworkUrl = "";
static float drawnProgress = -1;
static SpotifyPlaybackStatus drawnPlaybackStatus = PLAYBACK_LOADING;
static bool drawnIsPlaying = false;

static void splitText(const String &text, int maxLength, String &first, String &second)
{
    String value = text;
    value.trim();
    if (value.length() <= maxLength)
    {
        first = value;
        second = "";
        return;
    }

    int splitAt = value.lastIndexOf(' ', maxLength);
    if (splitAt <= 0)
        splitAt = maxLength;
    first = value.substring(0, splitAt);
    int secondStart = splitAt;
    while (secondStart < value.length() && value[secondStart] == ' ')
        ++secondStart;
    second = value.substring(secondStart);
    if (second.length() > maxLength)
        second = second.substring(0, maxLength - 3) + "...";
}

static void drawArtwork(const String &artworkUrl, SpotifyPlaybackStatus status)
{
    displayFillRect(18, 55, 150, 150, primary);
    displayFillRect(21, 58, 144, 144, artworkBackground);
    if (artworkUrl.length() > 0 && (status == PLAYBACK_PLAYING || status == PLAYBACK_PAUSED))
    {
        displayJpg("/cover.jpg", 21, 58, 2);
        return;
    }

    const char *placeholder = "No cover art";
    if (status == PLAYBACK_NOTHING)
        placeholder = "No playback";
    else if (status == PLAYBACK_ERROR)
        placeholder = "Unavailable";
    else if (status == PLAYBACK_LOADING)
        placeholder = "Loading...";
    displayText(placeholder, 25, 118, DEFAULT_FONT, 136, artworkBackground, muted);
}

static void drawProgress(float progress)
{
    if (progress < 0)
        progress = 0;
    if (progress > 1)
        progress = 1;
    int filledWidth = (int)(progress * 500.0f);
    displayFillRect(18, 220, 500, 6, artworkBackground);
    if (filledWidth > 0)
        displayFillRect(18, 220, filledWidth, 6, primary);
}

static void drawPlaybackScreen(SpotifyPlaybackStatus status, const String &title,
                               const String &subtitle, const String &artworkUrl,
                               float progress, bool isPlaying)
{
    displayClear(background);
    displayFillRect(0, 0, 536, 5, primary);
    displayText("TINYTRACKSPOTTER", 18, 10, DEFAULT_FONT, 500, background, primary);

    const char *stateLabel = "NOW PLAYING";
    uint16_t stateColor = primary;
    if (status == PLAYBACK_PAUSED || !isPlaying)
    {
        stateLabel = "PAUSED";
        stateColor = warning;
    }
    if (status == PLAYBACK_NOTHING)
    {
        stateLabel = "NO ACTIVE PLAYBACK";
        stateColor = muted;
    }
    else if (status == PLAYBACK_ERROR)
    {
        stateLabel = "PLAYBACK UNAVAILABLE";
        stateColor = errorColor;
    }
    else if (status == PLAYBACK_LOADING)
    {
        stateLabel = "CHECKING PLAYBACK";
        stateColor = muted;
    }
    displayText(stateLabel, 190, 34, DEFAULT_FONT, 326, background, stateColor);
    drawArtwork(artworkUrl, status);

    if (status == PLAYBACK_NOTHING)
    {
        displayText("Nothing is playing", 190, 73, &FreeSans18pt7b, 326, background, white);
        displayText("Start playback on Spotify.", 190, 119, DEFAULT_FONT, 326, background, muted);
        return;
    }
    if (status == PLAYBACK_ERROR)
    {
        displayText("Playback unavailable", 190, 73, &FreeSans18pt7b, 326, background, white);
        displayText("Retrying automatically.", 190, 119, DEFAULT_FONT, 326, background, muted);
        return;
    }
    if (status == PLAYBACK_LOADING)
    {
        displayText("Looking for playback...", 190, 73, &FreeSans18pt7b, 326, background, white);
        return;
    }

    String titleLine1;
    String titleLine2;
    splitText(title, 27, titleLine1, titleLine2);
    displayText(titleLine1, 190, 58, &FreeSans18pt7b, 326, background, white);
    displayText(titleLine2, 190, 88, &FreeSans18pt7b, 326, background, white);

    String subtitleLine1;
    String subtitleLine2;
    splitText(subtitle, 40, subtitleLine1, subtitleLine2);
    displayText(subtitleLine1, 190, 128, DEFAULT_FONT, 326, background, muted);
    displayText(subtitleLine2, 190, 151, DEFAULT_FONT, 326, background, muted);
    displayText(isPlaying ? "Playing" : "Paused", 190, 179, DEFAULT_FONT, 326, background, stateColor);
    drawProgress(progress);
}

void displaySpotifyAuthStatus(SpotifyAuthStatus status)
{
    playbackScreenDrawn = false;
    displayClear(background);
    displayFillRect(0, 0, 536, 5, primary);
    displayText("TINYTRACKSPOTTER", 18, 15, DEFAULT_FONT, 500, background, primary);

    String message = "Spotify setup required";
    uint16_t messageColor = white;
    if (status == SPOTIFY_AUTH_CONNECTED)
    {
        message = "Spotify connected";
        messageColor = primary;
    }
    else if (status == SPOTIFY_AUTH_REQUIRED)
    {
        message = "Spotify authorization required";
        messageColor = warning;
    }
    else if (status == SPOTIFY_AUTH_CONNECTION_ERROR)
    {
        message = "Spotify connection error";
        messageColor = errorColor;
    }
    displayText(message, 18, 49, &FreeSans18pt7b, 500, background, messageColor);
    displayFillRect(18, 94, 500, 1, muted);

    if (status == SPOTIFY_AUTH_NOT_CONFIGURED || status == SPOTIFY_AUTH_REQUIRED)
    {
        displayText("Open this page in your browser:", 18, 105, DEFAULT_FONT, 500, background, muted);
        displayText("http://spotifydisplay.local", 18, 131, &FreeSans18pt7b, 500, background, primary);
        if (WiFi.status() == WL_CONNECTED)
        {
            displayText("Or use the device IP:", 18, 171, DEFAULT_FONT, 500, background, muted);
            displayText("http://" + WiFi.localIP().toString(), 18, 193, DEFAULT_FONT, 500, background, white);
        }
        else
        {
            displayText("Connect the device to Wi-Fi first.", 18, 171, DEFAULT_FONT, 500, background, warning);
        }
    }
    else if (status == SPOTIFY_AUTH_CONNECTED)
    {
        displayText("Authentication is ready.", 18, 105, DEFAULT_FONT, 500, background, muted);
    }
    else
    {
        displayText("Check Wi-Fi, then retry from the setup page.", 18, 105, DEFAULT_FONT, 500, background, muted);
    }
}

void initScrean()
{
    playbackScreenDrawn = false;
    displayClear(background);
}

void updateScrean()
{
    if (getSpotifyAuthStatus() != SPOTIFY_AUTH_CONNECTED)
        return;

    SpotifyPlaybackStatus status = getPlaybackStatus();
    String title = getName();
    String subtitle = getSubtitle();
    String artworkUrl = getImageUrl();
    float progress = getProgress();
    bool isPlaying = getIsPlaing();

    bool contentChanged = !playbackScreenDrawn || status != drawnPlaybackStatus ||
                          title != drawnTitle || subtitle != drawnSubtitle ||
                          artworkUrl != drawnArtworkUrl || isPlaying != drawnIsPlaying;
    if (contentChanged)
    {
        drawPlaybackScreen(status, title, subtitle, artworkUrl, progress, isPlaying);
        playbackScreenDrawn = true;
        drawnPlaybackStatus = status;
        drawnTitle = title;
        drawnSubtitle = subtitle;
        drawnArtworkUrl = artworkUrl;
        drawnIsPlaying = isPlaying;
        drawnProgress = progress;
        return;
    }

    if (progress != drawnProgress)
    {
        drawProgress(progress);
        drawnProgress = progress;
    }
}
