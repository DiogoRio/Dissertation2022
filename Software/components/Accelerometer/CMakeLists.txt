idf_component_register(SRCS "Accelerometer.c"
                    INCLUDE_DIRS "include" REQUIRES "mpu6050" "esp_timer" "ConfigurationService")
