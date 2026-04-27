#include <Arduino.h>

#include "queues.h"
#include "tasks.h"

namespace {

void haltWithError(const char* message) {
    Serial.println(message);

    while(true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);

    if (!initQueues()) {
        haltWithError("queue initialization failed");
    }

    if (!createApplicationTasks()) {
        haltWithError("task creation failed");
    }
}

void loop() {
    vTaskSuspend(nullptr);
}
