#ifndef Accelerometer_H
#define Accelerometer_H

#include <stdio.h>
#include "esp_log.h"
#include "mpu6050.h"
#include "esp_timer.h"
#include <math.h>
#include "ConfigurationService.h"

void mpu6050_init();
void start_i2c();
esp_err_t isMoving(float x, float y, float z);
void mpu6050_read(void *pvParameters);


#endif