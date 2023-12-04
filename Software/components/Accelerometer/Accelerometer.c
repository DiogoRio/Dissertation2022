#include "Accelerometer.h"

mpu6050_handle_t mpu6050_dev = NULL;
mpu6050_acce_value_t acce;
esp_timer_handle_t cal_timer = NULL;

unsigned short ORIGINAL_FINGERPRINT_SERVICE_SLEEP;
unsigned short ORIGINAL_MESSAGE_SERVICE_SLEEP;

bool inLazyMode = false;
uint64_t lastLazyModeSwitchTime = 0;

static const char *TAG = "ACCELEROMETER: ";

const float modifier = 2;

void mpu6050_init()
{
    mpu6050_dev = mpu6050_create(0, MPU6050_I2C_ADDRESS);
    mpu6050_config(mpu6050_dev, ACCE_FS_2G, GYRO_FS_500DPS);
    mpu6050_wake_up(mpu6050_dev);

    const esp_timer_create_args_t cal_timer_config = {
        .callback = mpu6050_read,
        .arg = NULL,
        .name = "MPU6050 timer",
        .skip_unhandled_events = true,
        .dispatch_method = ESP_TIMER_TASK};
    esp_timer_create(&cal_timer_config, &cal_timer);
    esp_timer_start_periodic(cal_timer, 5000); // 5ms

    ORIGINAL_FINGERPRINT_SERVICE_SLEEP = configs.FINGERPRINT_SERVICE_SLEEP;
    ORIGINAL_MESSAGE_SERVICE_SLEEP = configs.MESSAGE_SERVICE_SLEEP;
}

void start_i2c(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)13;
    conf.scl_io_num = (gpio_num_t)14;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    conf.clk_flags = 0;
    i2c_param_config(I2C_NUM_0, &conf);
    i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

esp_err_t isMoving(float x, float y, float z)
{
    const float threshold = 1.15;

    const float vectorMagnitude = sqrt(x * x + y * y + z * z);

    if (vectorMagnitude > threshold)
    {
        return ESP_OK;
    }
    else
    {
        return ESP_FAIL;
    }
}

void setLazyModeConfigs()
{
    configs.FINGERPRINT_SERVICE_SLEEP = configs.FINGERPRINT_SERVICE_SLEEP * modifier;
    configs.MESSAGE_SERVICE_SLEEP = configs.MESSAGE_SERVICE_SLEEP * modifier;
}

void revertLazyModeConfigs()
{
    configs.FINGERPRINT_SERVICE_SLEEP = ORIGINAL_FINGERPRINT_SERVICE_SLEEP;
    configs.MESSAGE_SERVICE_SLEEP = ORIGINAL_MESSAGE_SERVICE_SLEEP;
}

void mpu6050_read(void *pvParameters)
{
    mpu6050_get_acce(mpu6050_dev, &acce);

    uint64_t current_time = esp_timer_get_time();
    uint64_t time_diff = current_time - lastLazyModeSwitchTime;

    if (isMoving(acce.acce_x, acce.acce_y, acce.acce_z) == ESP_OK && inLazyMode == true)
    {
        ESP_LOGI(TAG, "ITS MOVING!!! Detected G force: %f , Reverting lazy mode configs...", sqrt(acce.acce_x * acce.acce_x + acce.acce_y * acce.acce_y + acce.acce_z * acce.acce_z));

        revertLazyModeConfigs();
        inLazyMode = false;
        lastLazyModeSwitchTime = current_time;
    }
    else if (inLazyMode == false && (time_diff >= (60 * 1000000) || lastLazyModeSwitchTime == 0))
    {
        ESP_LOGI(TAG, "No Movement Detected. Detected G force: %f , Setting lazy mode configs...", sqrt(acce.acce_x * acce.acce_x + acce.acce_y * acce.acce_y + acce.acce_z * acce.acce_z));
        inLazyMode = true;
        setLazyModeConfigs();
        lastLazyModeSwitchTime = current_time;
    }
}