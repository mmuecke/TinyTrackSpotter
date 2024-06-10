#ifndef Spotify_h

#define Spotify_h

#include <Arduino.h>
#include <ArduinoJson.h>

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