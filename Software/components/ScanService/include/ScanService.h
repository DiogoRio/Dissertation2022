#include <stdio.h>
#include "ConfigurationService.h"
#include "WifiService.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "driver/gpio.h"


#ifndef SCAN_SERVICE_H
#define SCAN_SERVICE_H

struct scan_parameters {
    SemaphoreHandle_t xSemaphore;
    QueueHandle_t xQueue;
    int scan_led_pin;
    int semaphore_wait_time;
};

typedef struct scan_parameters scan_parameters_t;

void scan_task(scan_parameters_t *params);

#endif
