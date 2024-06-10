#ifndef SettingsEntry_h

#define SettingsEntry_h

#include <Arduino.h>
#include "LittleFS.h"

extern fs::LittleFSFS LittleFS;

class SettingsEntry
{
public:
    SettingsEntry(const String key, const char *path);
    const char *getPath();
    String getKey();
    String getValue();
    void setValue(String value);
    bool writeValue();
    bool readValue();

private:
    String key;
    const char *path;
    String value = "";
};

#endif