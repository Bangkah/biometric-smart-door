#pragma once
// test/stubs/freertos/FreeRTOS.h — native test single-threaded: tidak
// perlu penjadwal sungguhan.
#include <stdint.h>
typedef int BaseType_t;
typedef uint32_t TickType_t;
#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(x) ((TickType_t)(x))
void vTaskDelay(TickType_t ticks);
