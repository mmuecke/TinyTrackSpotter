#ifndef FileSystem_h

#define FileSystem_h

#include <Arduino.h>
#include "LittleFS.h"

extern fs::LittleFSFS LittleFS;

bool initFileSystem();

#endif