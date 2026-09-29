#pragma once
// test/stubs/freertos/semphr.h — mutex no-op: native test berjalan
// single-threaded sehingga tidak ada kontensi sungguhan untuk diuji di sini.
#include <freertos/FreeRTOS.h>
typedef struct QueueDefinition* SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
