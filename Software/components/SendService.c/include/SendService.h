#include <stdio.h>
#include "ConfigurationService.h"
#include "HttpService.h"
#include "WifiService.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"


#ifndef SEND_SERVICE_H
#define SEND_SERVICE_H

struct send_parameters {
    SemaphoreHandle_t xSemaphore;
    QueueHandle_t xQueue;
    int send_led_pin;
    int semaphore_wait_time;
};

typedef struct send_parameters send_parameters_t;

void send_task(send_parameters_t *params);

#endif

