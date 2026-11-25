#ifndef Spotify_h

#define Spotify_h

#include <Arduino.h>
#include <ArduinoJson.h>

enum SpotifyAuthStatus
{
    SPOTIFY_AUTH_NOT_CONFIGURED,
    SPOTIFY_AUTH_REQUIRED,
    SPOTIFY_AUTH_CONNECTED,
    SPOTIFY_AUTH_CONNECTION_ERROR
};

SpotifyAuthStatus getSpotifyAuthStatus();
const char *getSpotifyAuthStatusName();
void checkSpotifyAuthentication();
void updateSpotifyAuthentication();
void spotifyAuthSettingsChanged();

boolean getHasData();

boolean hasImageUrlUpdate();
String getImageUrl();

boolean hasNameUpdate();
String getName();

boolean hasSubtitleUpdate();
String getSubtitle();

boolean hasProgressUpdate();
float getProgress();

boolean hasIsPlayingUpdate();
boolean getIsPlaing();

void updateCurrentPlaying();
#endif
