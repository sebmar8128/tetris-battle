#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "queues.h"
#include "tasks.h"

namespace {

constexpr char STORAGE_NAMESPACE[] = "tb_users";
constexpr uint32_t USER_RECORD_MAGIC = 0x54425553; // TBUS
constexpr uint8_t USER_RECORD_VERSION = 1;

struct UserSettingsRecord {
    uint32_t magic;
    uint8_t version;
    uint8_t reserved[3];
    UserSettings settings;
};

bool isUppercaseUsername(const char* username) {
    if (username == nullptr || username[0] == '\0') {
        return false;
    }

    for (uint8_t i = 0; i < MAX_USERNAME_LEN && username[i] != '\0'; ++i) {
        if (username[i] < 'A' || username[i] > 'Z') {
            return false;
        }
    }

    return true;
}

void makeUserKey(const char* username, char* key, size_t keySize) {
    snprintf(key, keySize, "u_%s", username);
}

bool loadUserSettings(Preferences& prefs, const char* username, UserSettings& settings) {
    if (!isUppercaseUsername(username)) {
        return false;
    }

    char key[12] = {};
    makeUserKey(username, key, sizeof(key));
    const size_t storedLength = prefs.getBytesLength(key);
    if (storedLength != sizeof(UserSettingsRecord)) {
        return false;
    }

    UserSettingsRecord record = {};
    const size_t readLength = prefs.getBytes(key, &record, sizeof(record));
    if (readLength != sizeof(record)) {
        return false;
    }

    if (record.magic != USER_RECORD_MAGIC || record.version != USER_RECORD_VERSION) {
        return false;
    }

    settings = record.settings;
    settings.username[MAX_USERNAME_LEN] = '\0';
    settings.nextPreviewCount = min(settings.nextPreviewCount, MAX_NEXT_PIECES);
    return isUppercaseUsername(settings.username);
}

bool saveUserSettings(Preferences& prefs, const UserSettings& settings) {
    if (!isUppercaseUsername(settings.username)) {
        return false;
    }

    UserSettings clean = settings;
    clean.username[MAX_USERNAME_LEN] = '\0';
    clean.nextPreviewCount = min(clean.nextPreviewCount, MAX_NEXT_PIECES);

    const UserSettingsRecord record = {
        USER_RECORD_MAGIC,
        USER_RECORD_VERSION,
        {0, 0, 0},
        clean
    };

    char key[12] = {};
    makeUserKey(clean.username, key, sizeof(key));
    const size_t written = prefs.putBytes(key, &record, sizeof(record));
    return written == sizeof(record);
}

}  // namespace

void storageTask(void* pvParameters) {
    (void)pvParameters;

    StorageRequest request;

    while(true) {
        if (xQueueReceive(g_storageRequestQueue, &request, portMAX_DELAY) == pdTRUE) {
            StorageResponse response = {};
            response.key = request.key;
            response.status = StorageStatus::Failed;

            if (request.key == StorageKey::UserSettings) {
                Preferences prefs;
                const bool readOnly = request.operation == StorageOperation::Load;
                if (prefs.begin(STORAGE_NAMESPACE, readOnly)) {
                    if (request.operation == StorageOperation::Load) {
                        if (loadUserSettings(prefs, request.localUsername, response.payload.userSettings)) {
                            response.status = StorageStatus::Success;
                        } else {
                            response.status = StorageStatus::NotFound;
                        }
                    } else if (saveUserSettings(prefs, request.payload.userSettings)) {
                        response.status = StorageStatus::Success;
                        response.payload.userSettings = request.payload.userSettings;
                    }

                    prefs.end();
                } else {
                    if (request.operation == StorageOperation::Load) {
                        response.status = StorageStatus::NotFound;
                    }
                }
            }

            (void)xQueueSend(g_storageResponseQueue, &response, 0);
        }
    }
}
