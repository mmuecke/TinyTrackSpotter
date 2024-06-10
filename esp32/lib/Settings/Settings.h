#ifndef Settings_h

#define Settings_h

#include <Arduino.h>
#include "SettingsEntry.h"

extern const SettingsEntry settingsArray[];

bool initSettings();
bool hasSetting(String key);
String getSetting(String key, bool read = false);
bool trySetSetting(String key, String value, bool write = true);

#endif