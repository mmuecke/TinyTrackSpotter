#include <Arduino.h>
#include "SettingsEntry.h"
#include "LittleFS.h"

/**
 * @brief Constructor for SettingsEntry class.
 *
 * @param key The key for the settings entry.
 * @param path The path to the file where the settings entry is stored.
 */
SettingsEntry::SettingsEntry(const String key, const char *path)
{
    this->key = key;
    this->path = path;
}

/**
 * @brief Getter for the key of the settings entry.
 *
 * @return The key of the settings entry.
 */
String SettingsEntry::getKey()
{
    return this->key;
}

/**
 * @brief Getter for the path to the file where the settings entry is stored.
 *
 * @return The path to the file where the settings entry is stored.
 */
const char *SettingsEntry::getPath()
{
    return this->path;
}

/**
 * @brief Getter for the value of the settings entry.
 *
 * @return The value of the settings entry.
 */
String SettingsEntry::getValue()
{
    return this->value;
}

/**
 * @brief Setter for the value of the settings entry.
 *
 * @param value The new value for the settings entry.
 */
void SettingsEntry::setValue(String value)
{
    this->value = value;
}

/**
 * @brief Writes the value of the settings entry to the file.
 *
 * @return True if the write was successful, false otherwise.
 */
bool SettingsEntry::writeValue()
{
    if (!LittleFS.begin())
    {
        Serial.println("Error mounting the file system");
        return false;
    }
    if (LittleFS.exists(path))
    {
        bool removed = LittleFS.remove(path);
        if (!removed)
        {
            Serial.println("Failed to remove file for writing: " + String(path));
            return false;
        }
    }
    File file = LittleFS.open(path, "w");
    if (!file)
    {
        Serial.println("Failed to open file for writing: " + String(path));
        return false;
    }

    file.print(value);
    file.close();

    Serial.println("Written value for key " + key + " in file " + String(path));
    LittleFS.end();
    return true;
}

/**
 * @brief Reads the value of the settings entry from the file.
 *
 * @return True if the read was successful, false otherwise.
 */
bool SettingsEntry::readValue()
{
    if (!LittleFS.begin())
    {
        Serial.println("Error mounting the file system");
        return false;
    }
    if (!LittleFS.exists(path))
    {
        File file = LittleFS.open(path, "w");
        if (!file)
        {
            Serial.println("Failed to create file: " + String(path));
            return false;
        }
        file.print(""); // Write a default value
        file.close();
    }
    File file = LittleFS.open(path, "r");
    if (!file)
    {
        Serial.println("Failed to open file for reading: " + String(path));
        return false;
    }

    value = file.readString();
    file.close();

    Serial.println("Read value for key " + key + " from file " + String(path));
    LittleFS.end();
    return true;
}
