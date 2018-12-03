#pragma once

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

void set_notification_uart(xTaskHandle h);
int setup_uart();
void stop_uart();
size_t read_uart(char * buf, size_t length);
size_t write_uart(const char * buf, size_t length);

#ifdef __cplusplus
}
#endif
