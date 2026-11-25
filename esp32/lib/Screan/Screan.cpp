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

static String fitWithEllipsis(String text, const GFXfont *font, int maxWidth)
{
    const String suffix = "...";
    while (text.length() > 0 && displayTextWidth(text + suffix, font) > maxWidth)
    {
        int space = text.lastIndexOf(' ');
        if (space > 0)
            text = text.substring(0, space);
        else
            text.remove(text.length() - 1);
    }
    return text + suffix;
}

static void wrapAtSpaces(const String &text, const GFXfont *font, int maxWidth, String *lines, int maxLines)
{
    for (int i = 0; i < maxLines; ++i)
        lines[i] = "";

    String remaining = text;
    remaining.trim();
    for (int lineIndex = 0; lineIndex < maxLines && remaining.length() > 0; ++lineIndex)
    {
        int cutAt = remaining.length();
        while (cutAt > 0 && displayTextWidth(remaining.substring(0, cutAt), font) > maxWidth)
        {
            int space = remaining.lastIndexOf(' ', cutAt - 1);
            if (space <= 0)
            {
                lines[lineIndex] = fitWithEllipsis(remaining, font, maxWidth);
                remaining = "";
                break;
            }
            cutAt = space;
        }
        if (remaining.length() == 0)
            break;

        lines[lineIndex] = remaining.substring(0, cutAt);
        remaining = remaining.substring(cutAt);
        remaining.trim();
        if (lineIndex == maxLines - 1 && remaining.length() > 0)
            lines[lineIndex] = fitWithEllipsis(lines[lineIndex], font, maxWidth);
    }
}

static void drawArtwork(const String &artworkUrl, SpotifyPlaybackStatus status)
{
    displayFillRect(18, 32, 150, 150, primary);
    displayFillRect(21, 35, 144, 144, artworkBackground);
    if (artworkUrl.length() > 0 && (status == PLAYBACK_PLAYING || status == PLAYBACK_PAUSED))
    {
        displayJpg("/cover.jpg", 21, 35, 2);
        return;
    }

    const char *placeholder = "No cover art";
    if (status == PLAYBACK_NOTHING)
        placeholder = "No playback";
    else if (status == PLAYBACK_ERROR)
        placeholder = "Unavailable";
    else if (status == PLAYBACK_LOADING)
        placeholder = "Loading...";
    displayText(placeholder, 25, 95, DEFAULT_FONT, 136, artworkBackground, muted);
}

static void drawProgress(float progress, bool isPlaying)
{
    if (progress < 0)
        progress = 0;
    if (progress > 1)
        progress = 1;
    uint16_t iconColor = isPlaying ? primary : warning;
    if (isPlaying)
    {
        for (int row = 0; row < 17; ++row)
        {
            int rowWidth = row <= 8 ? row + 1 : 17 - row;
            int width = rowWidth * 14 / 9;
            if (width < 1)
                width = 1;
            displayFillRect(19 + 14 - width, 204 + row, width, 1, iconColor);
        }
    }
    else
    {
        displayFillRect(19, 205, 4, 15, iconColor);
        displayFillRect(27, 205, 4, 15, iconColor);
    }

    const int barX = 44;
    const int barY = 211;
    const int barWidth = 474;
    int filledWidth = (int)(progress * barWidth);
    displayFillRect(barX, barY, barWidth, 6, artworkBackground);
    if (filledWidth > 0)
        displayFillRect(barX, barY, filledWidth, 6, primary);
}

static void drawPlaybackScreen(SpotifyPlaybackStatus status, const String &title,
                               const String &subtitle, const String &artworkUrl,
                               float progress, bool isPlaying)
{
    displayClear(background);
    bool hasPlayback = status == PLAYBACK_PLAYING || status == PLAYBACK_PAUSED;
    if (!hasPlayback)
    {
        displayFillRect(0, 0, 536, 5, primary);
        displayText("TINYTRACKSPOTTER", 18, 10, DEFAULT_FONT, 500, background, primary);
        const char *stateLabel = status == PLAYBACK_NOTHING ? "NO ACTIVE PLAYBACK" :
                                 status == PLAYBACK_ERROR ? "PLAYBACK UNAVAILABLE" : "CHECKING PLAYBACK";
        uint16_t stateColor = status == PLAYBACK_ERROR ? errorColor : muted;
        displayText(stateLabel, 18, 35, DEFAULT_FONT, 500, background, stateColor);
    }
    drawArtwork(artworkUrl, status);

    if (status == PLAYBACK_NOTHING)
    {
        displayText("Nothing is playing", 190, 76, &FreeSans18pt7b, 326, background, white);
        displayText("Start playback on Spotify.", 190, 120, DEFAULT_FONT, 326, background, muted);
        return;
    }
    if (status == PLAYBACK_ERROR)
    {
        displayText("Playback unavailable", 190, 76, &FreeSans18pt7b, 326, background, white);
        displayText("Retrying automatically.", 190, 120, DEFAULT_FONT, 326, background, muted);
        return;
    }
    if (status == PLAYBACK_LOADING)
    {
        displayText("Looking for playback...", 190, 76, &FreeSans18pt7b, 326, background, white);
        return;
    }

    String titleLines[3];
    String subtitleLines[3];
    wrapAtSpaces(title, &FreeSans18pt7b, 326, titleLines, 3);
    wrapAtSpaces(subtitle, DEFAULT_FONT, 326, subtitleLines, 3);
    for (int i = 0; i < 3; ++i)
    {
        displayText(titleLines[i], 190, 28 + i * 29, &FreeSans18pt7b, 326, background, white);
        displayText(subtitleLines[i], 190, 119 + i * 22, DEFAULT_FONT, 326, background, muted);
    }
    drawProgress(progress, isPlaying);
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
        drawProgress(progress, isPlaying);
        drawnProgress = progress;
    }
}
