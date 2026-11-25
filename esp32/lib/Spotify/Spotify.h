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

enum SpotifyPlaybackStatus
{
    PLAYBACK_LOADING,
    PLAYBACK_PLAYING,
    PLAYBACK_PAUSED,
    PLAYBACK_NOTHING,
    PLAYBACK_ERROR
};

enum SpotifyArtworkStatus
{
    ARTWORK_NONE,
    ARTWORK_LOADING,
    ARTWORK_READY,
    ARTWORK_ERROR
};

enum SpotifyControlCommand
{
    SPOTIFY_CONTROL_NEXT,
    SPOTIFY_CONTROL_TOGGLE_PLAYBACK
};

SpotifyAuthStatus getSpotifyAuthStatus();
const char *getSpotifyAuthStatusName();
void checkSpotifyAuthentication();
void updateSpotifyAuthentication();
void spotifyAuthSettingsChanged();
SpotifyPlaybackStatus getPlaybackStatus();
SpotifyArtworkStatus getArtworkStatus();

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
void requestSpotifyControl(SpotifyControlCommand command);
#endif
