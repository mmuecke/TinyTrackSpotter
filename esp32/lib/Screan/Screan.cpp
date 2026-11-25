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
static String queuedArtworkUrl = "";
static String failedArtworkUrl = "";
static String placeholderArtworkUrl = "";
static SpotifyPlaybackStatus drawnPlaybackStatus = PLAYBACK_LOADING;
static bool drawnIsPlaying = false;
static const int progressBarWidth = 474;
static int drawnBarFillPixels = -1;

static int progressPixelWidth(float progress)
{
    if (progress < 0)
        progress = 0;
    if (progress > 1)
        progress = 1;
    return (int)(progress * progressBarWidth);
}

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

enum ArtworkPlaceholderIcon
{
    ARTWORK_ICON_DEFAULT,
    ARTWORK_ICON_NOT_LOADED,
    ARTWORK_ICON_LOADING,
    ARTWORK_ICON_RESIZE_ERROR
};

static void drawArtworkPlaceholder(const char *label, ArtworkPlaceholderIcon icon = ARTWORK_ICON_DEFAULT)
{
    displayFillRect(12, 23, 160, 160, primary);
    displayFillRect(15, 26, 154, 154, artworkBackground);
    // Every artwork-state symbol is centered on the 160 px cover tile.
    const int iconCenterX = 92;
    const int iconCenterY = 103;
    if (icon == ARTWORK_ICON_LOADING)
    {
        // Eight ticks around a centered spinner.
        displayFillRect(iconCenterX - 4, iconCenterY - 38, 8, 12, primary);
        displayFillRect(iconCenterX + 17, iconCenterY - 30, 8, 8, primary);
        displayFillRect(iconCenterX + 26, iconCenterY - 4, 12, 8, primary);
        displayFillRect(iconCenterX + 17, iconCenterY + 22, 8, 8, muted);
        displayFillRect(iconCenterX - 4, iconCenterY + 26, 8, 12, muted);
        displayFillRect(iconCenterX - 25, iconCenterY + 22, 8, 8, muted);
        displayFillRect(iconCenterX - 38, iconCenterY - 4, 12, 8, muted);
        displayFillRect(iconCenterX - 25, iconCenterY - 30, 8, 8, muted);
    }
    else if (icon == ARTWORK_ICON_RESIZE_ERROR)
    {
        // Broken-image frame plus a red warning triangle and exclamation mark.
        displayFillRect(iconCenterX - 30, iconCenterY - 25, 60, 4, muted);
        displayFillRect(iconCenterX - 30, iconCenterY + 19, 60, 4, muted);
        displayFillRect(iconCenterX - 30, iconCenterY - 25, 4, 48, muted);
        displayFillRect(iconCenterX + 26, iconCenterY - 25, 4, 48, muted);
        displayFillRect(iconCenterX - 18, iconCenterY + 5, 14, 4, primary);
        displayFillRect(iconCenterX - 8, iconCenterY - 5, 14, 4, primary);
        displayFillRect(iconCenterX + 2, iconCenterY + 5, 12, 4, primary);
        displayFillRect(iconCenterX + 8, iconCenterY - 24, 3, 7, errorColor);
        displayFillRect(iconCenterX + 5, iconCenterY - 18, 9, 4, errorColor);
        displayFillRect(iconCenterX + 2, iconCenterY - 14, 15, 4, errorColor);
        displayFillRect(iconCenterX - 1, iconCenterY - 10, 21, 4, errorColor);
        displayFillRect(iconCenterX + 7, iconCenterY - 21, 3, 6, TFT_WHITE);
        displayFillRect(iconCenterX + 7, iconCenterY - 13, 3, 3, TFT_WHITE);
    }
    else if (icon == ARTWORK_ICON_NOT_LOADED)
    {
        // Empty picture frame with a simple landscape, centered in the tile.
        displayFillRect(iconCenterX - 28, iconCenterY - 22, 56, 4, muted);
        displayFillRect(iconCenterX - 28, iconCenterY + 22, 56, 4, muted);
        displayFillRect(iconCenterX - 28, iconCenterY - 22, 4, 48, muted);
        displayFillRect(iconCenterX + 24, iconCenterY - 22, 4, 48, muted);
        displayFillRect(iconCenterX - 20, iconCenterY + 12, 8, 4, primary);
        displayFillRect(iconCenterX - 12, iconCenterY + 4, 8, 4, primary);
        displayFillRect(iconCenterX - 4, iconCenterY - 4, 8, 4, primary);
        displayFillRect(iconCenterX + 4, iconCenterY + 4, 8, 4, primary);
        displayFillRect(iconCenterX + 12, iconCenterY + 12, 8, 4, primary);
        displayFillRect(iconCenterX + 10, iconCenterY - 13, 6, 6, muted);
    }
    else
    {
        // A small music mark for non-artwork empty states.
        displayFillRect(76, 70, 8, 21, primary);
        displayFillRect(88, 61, 8, 30, primary);
        displayFillRect(100, 75, 8, 16, primary);
        displayFillRect(63, 98, 58, 2, muted);
    }
    if (label && label[0])
    {
        String displayLabel = label;
        int labelWidth = displayTextWidth(displayLabel, DEFAULT_FONT);
        if (labelWidth > 150)
        {
            displayLabel = fitWithEllipsis(displayLabel, DEFAULT_FONT, 150);
            labelWidth = displayTextWidth(displayLabel, DEFAULT_FONT);
        }
        int labelX = 15 + max(0, (154 - labelWidth) / 2);
        displayText(displayLabel, labelX, 119, DEFAULT_FONT, min(154, labelWidth + 2), artworkBackground, muted);
    }
}

static void drawArtwork(const String &artworkUrl, SpotifyPlaybackStatus status)
{
    if (status == PLAYBACK_PLAYING || status == PLAYBACK_PAUSED)
    {
        ArtworkPlaceholderIcon icon = ARTWORK_ICON_LOADING;
        if (artworkUrl.length() == 0 || getArtworkStatus() == ARTWORK_ERROR)
            icon = ARTWORK_ICON_NOT_LOADED;
        else if (artworkUrl == failedArtworkUrl)
            icon = ARTWORK_ICON_RESIZE_ERROR;
        placeholderArtworkUrl = artworkUrl;
        drawArtworkPlaceholder("", icon);
        return;
    }
    placeholderArtworkUrl = "";
    const char *label = "No cover art";
    if (status == PLAYBACK_NOTHING) label = "No playback";
    else if (status == PLAYBACK_ERROR) label = "Unavailable";
    else if (status == PLAYBACK_LOADING) label = "Looking...";
    drawArtworkPlaceholder(label);
}

static void drawArtworkText(const String &title, const String &subtitle)
{
    displayFillRect(185, 8, 343, 198, background);
    String titleLines[3];
    String subtitleLines[3];
    wrapAtSpaces(title, &FreeSans18pt7b, 326, titleLines, 3);
    wrapAtSpaces(subtitle, DEFAULT_FONT, 326, subtitleLines, 3);
    int titleY = 22;
    for (int i = 0; i < 3; ++i)
    {
        if (titleLines[i].length() == 0) continue;
        uint16_t height = displayText(titleLines[i], 190, titleY, &FreeSans18pt7b, 326, background, white);
        Serial.printf("Screen title line %d, y=%d, width=%u: '%s'\n", i + 1, titleY,
                      (unsigned)displayTextWidth(titleLines[i], &FreeSans18pt7b), titleLines[i].c_str());
        titleY += height + 4;
    }
    int subtitleY = max(107, titleY + 6);
    for (int i = 0; i < 3; ++i)
    {
        if (subtitleLines[i].length() == 0) continue;
        uint16_t height = displayText(subtitleLines[i], 190, subtitleY, DEFAULT_FONT, 326, background, muted);
        Serial.printf("Screen artist/show line %d, y=%d, width=%u: '%s'\n", i + 1, subtitleY,
                      (unsigned)displayTextWidth(subtitleLines[i], DEFAULT_FONT), subtitleLines[i].c_str());
        subtitleY += height + 2;
    }
}

static void updateArtwork(const String &url, SpotifyPlaybackStatus status)
{
    DisplayJpgTaskResult result = displayProcessCoverDecode();
    if (result == DISPLAY_JPG_DRAWN)
    {
        drawnArtworkUrl = queuedArtworkUrl;
        failedArtworkUrl = "";
        placeholderArtworkUrl = "";
        queuedArtworkUrl = "";
        Serial.printf("Cover decode complete: '%s'\n", drawnArtworkUrl.c_str());
    }
    else if (result == DISPLAY_JPG_FAILED)
    {
        failedArtworkUrl = queuedArtworkUrl;
        placeholderArtworkUrl = failedArtworkUrl;
        queuedArtworkUrl = "";
        Serial.printf("ERROR: Cover resize/decode failed for image URL: %s\n", failedArtworkUrl.c_str());
        if (failedArtworkUrl == url)
            drawArtworkPlaceholder("", ARTWORK_ICON_RESIZE_ERROR);
    }

    bool hasPlayback = status == PLAYBACK_PLAYING || status == PLAYBACK_PAUSED;
    if (!hasPlayback || url.length() == 0)
    {
        if (drawnArtworkUrl.length() || queuedArtworkUrl.length() || placeholderArtworkUrl.length())
        {
            drawnArtworkUrl = "";
            queuedArtworkUrl = "";
            failedArtworkUrl = "";
            drawArtwork(url, status);
        }
        return;
    }

    SpotifyArtworkStatus artworkStatus = getArtworkStatus();
    if (url != drawnArtworkUrl && url != queuedArtworkUrl && url != failedArtworkUrl)
    {
        if (placeholderArtworkUrl != url)
        {
            drawArtworkPlaceholder("", artworkStatus == ARTWORK_ERROR
                                           ? ARTWORK_ICON_NOT_LOADED
                                           : ARTWORK_ICON_LOADING);
            placeholderArtworkUrl = url;
        }
    }

    if (url == drawnArtworkUrl)
        return;

    if (artworkStatus == ARTWORK_ERROR)
    {
        if (failedArtworkUrl != url)
            failedArtworkUrl = url;
        if (placeholderArtworkUrl != url)
        {
            drawArtworkPlaceholder("", ARTWORK_ICON_NOT_LOADED);
            placeholderArtworkUrl = url;
        }
    }
    else if (artworkStatus == ARTWORK_READY && queuedArtworkUrl.length() == 0 && url != failedArtworkUrl)
    {
        if (displayQueueCoverDecode(12, 23))
        {
            queuedArtworkUrl = url;
            if (placeholderArtworkUrl != url)
            {
                drawArtworkPlaceholder("", ARTWORK_ICON_LOADING);
                placeholderArtworkUrl = url;
            }
        }
        else
        {
            failedArtworkUrl = url;
            drawArtworkPlaceholder("", ARTWORK_ICON_RESIZE_ERROR);
            placeholderArtworkUrl = url;
        }
    }
}

static void drawPlaybackIndicator(bool isPlaying)
{
    uint16_t iconColor = isPlaying ? primary : warning;
    if (isPlaying)
    {
        for (int row = 0; row < 17; ++row)
        {
            int rowWidth = row <= 8 ? row + 1 : 17 - row;
            int width = rowWidth * 14 / 9;
            if (width < 1)
                width = 1;
            displayFillRect(19, 214 + row, width, 1, iconColor);
        }
    }
    else
    {
        displayFillRect(19, 215, 4, 15, iconColor);
        displayFillRect(27, 215, 4, 15, iconColor);
    }

}

static void drawProgressBar(float progress)
{
    const int barX = 44;
    const int barY = 220;
    const int barWidth = progressBarWidth;
    int filledWidth = progressPixelWidth(progress);
    if (drawnBarFillPixels < 0)
    {
        displayFillRect(barX, barY, barWidth, 6, artworkBackground);
        if (filledWidth > 0)
            displayFillRect(barX, barY, filledWidth, 6, primary);
    }
    else if (filledWidth > drawnBarFillPixels)
    {
        displayFillRect(barX + drawnBarFillPixels, barY,
                        filledWidth - drawnBarFillPixels, 6, primary);
    }
    else if (filledWidth < drawnBarFillPixels)
    {
        displayFillRect(barX + filledWidth, barY,
                        drawnBarFillPixels - filledWidth, 6, artworkBackground);
    }
    drawnBarFillPixels = filledWidth;
}

static void drawProgress(float progress, bool isPlaying)
{
    drawPlaybackIndicator(isPlaying);
    drawProgressBar(progress);
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

    drawArtworkText(title, subtitle);
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

    bool screenModeChanged = !playbackScreenDrawn || status != drawnPlaybackStatus;
    if (screenModeChanged)
    {
        drawnBarFillPixels = -1;
        drawPlaybackScreen(status, title, subtitle, artworkUrl, progress, isPlaying);
        playbackScreenDrawn = true;
        drawnPlaybackStatus = status;
        drawnTitle = title;
        drawnSubtitle = subtitle;
        drawnArtworkUrl = "";
        drawnIsPlaying = isPlaying;
    }

    bool hasPlayback = status == PLAYBACK_PLAYING || status == PLAYBACK_PAUSED;
    if (hasPlayback && (title != drawnTitle || subtitle != drawnSubtitle))
        drawArtworkText(title, subtitle);
    updateArtwork(artworkUrl, status);
    if (!hasPlayback && artworkUrl != drawnArtworkUrl)
        drawnArtworkUrl = artworkUrl;
    drawnTitle = title;
    drawnSubtitle = subtitle;

    if (isPlaying != drawnIsPlaying)
    {
        drawPlaybackIndicator(isPlaying);
        drawnIsPlaying = isPlaying;
    }

    drawProgressBar(progress);
}
