#include <Arduino.h>
#include "LittleFS.h"

bool initFileSystem()
{
    bool success = LittleFS.begin();
    if (!success)
    {
        Serial.println("Error mounting the file system");
        return false;
    }
    Serial.println("Mounted the file system");
    LittleFS.end();
    return true;
}
