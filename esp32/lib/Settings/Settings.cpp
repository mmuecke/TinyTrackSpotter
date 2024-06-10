#include <Arduino.h>
#include "SettingsEntry.h"

const SettingsEntry settingsArray[] = {
    SettingsEntry("clientId", "/clientId.txt"),
    SettingsEntry("clientSecret", "/clientSecret.txt"),
    SettingsEntry("refreshToken", "/refreshToken.txt"),
    SettingsEntry("accessToken", "/accessToken.txt"),
};

SettingsEntry removeConst(const SettingsEntry &entry)
{
    return const_cast<SettingsEntry &>(entry); // Remove const qualifier
}

bool initSettings()
{
    Serial.println("Mounted the file system");
    for (const SettingsEntry &entry : settingsArray)
    {
        removeConst(entry).readValue();
    }
    return true;
}

bool hasSetting(String key)
{
    for (const SettingsEntry &constEntry : settingsArray)
    {
        SettingsEntry entry = removeConst(constEntry);
        if (entry.getKey() == key)
        {
            return true;
        }
    }
    return false;
}

String getSetting(String key, bool read = false)
{
    for (const SettingsEntry &constEntry : settingsArray)
    {
        SettingsEntry entry = removeConst(constEntry);
        if (entry.getKey() == key)
        {
            if (read)
            {
                bool success = entry.readValue();
                if (!success)
                {
                    return "";
                }
            }
            return entry.getValue();
        }
    }
    return "";
}

bool trySetSetting(String key, String value, bool write = true)
{
    for (const SettingsEntry &constEntry : settingsArray)
    {
        SettingsEntry entry = removeConst(constEntry);
        if (entry.getKey() == key)
        {
            String oldValue = entry.getValue();
            entry.setValue(value);
            if (write)
            {
                bool success = entry.writeValue();
                if (!success)
                {
                    entry.setValue(oldValue);
                    return false;
                }
            }

            return true;
        }
    }
    return false;
}
