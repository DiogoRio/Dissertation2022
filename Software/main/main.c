#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
// #include "esp_heap_trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "WifiService.h"
#include "ConfigurationService.h"
#include "HttpService.h"
#include "ScanService.h"
#include "SendService.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "Accelerometer.h"

static const char *TAG = "main_tag_instance";

uint64_t scan_button_last_press_time = 0;

#define DEBOUNCE_DELAY_MS 100
#define ESP_INR_FLAG_DEFAULT 0
#define SEND_LED_PIN 26
#define SCAN_LED_PIN 27
#define PUSH_BUTTON_PIN 33

#define SEMAPHORE_WAIT_TIME portMAX_DELAY

SemaphoreHandle_t xSemaphore = NULL;

TaskHandle_t scanTaskHandle = NULL;
TaskHandle_t sendTaskHandle = NULL;
TaskHandle_t setApModeConfigTaskHandle = NULL;

QueueHandle_t queue;

void IRAM_ATTR scan_button_isr_handler(void *arg)
{

    uint64_t current_time = esp_timer_get_time();
    uint64_t time_diff = current_time - scan_button_last_press_time;
    scan_button_last_press_time = current_time;

    if (time_diff >= (DEBOUNCE_DELAY_MS * 1000))
    {
        int gpio_level = gpio_get_level(PUSH_BUTTON_PIN);
        if (gpio_level == 0)
            return;

        xTaskResumeFromISR(setApModeConfigTaskHandle);
    }
}

void setApModeConfigTask(void *params)
{
    while (1)
    {
        vTaskSuspend(NULL);
        vTaskSuspend(sendTaskHandle);
        vTaskSuspend(scanTaskHandle);
        wifi_init_softap();
        ESP_LOGI(TAG, "Starting Configuration server...\n");
        setup_server();
    }
}

int app_main(void)
{
    ESP_LOGI(TAG, "Initializing...");

    // Initialize and setup board pins
    esp_rom_gpio_pad_select_gpio(SCAN_LED_PIN);
    esp_rom_gpio_pad_select_gpio(SEND_LED_PIN);

    gpio_set_direction(SCAN_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(SEND_LED_PIN, GPIO_MODE_OUTPUT);

    gpio_set_direction(PUSH_BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(PUSH_BUTTON_PIN, GPIO_INTR_POSEDGE);
    gpio_install_isr_service(ESP_INR_FLAG_DEFAULT);
    gpio_isr_handler_add(PUSH_BUTTON_PIN, scan_button_isr_handler, NULL);

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize SPIFFS
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
    };
    esp_vfs_spiffs_register(&config);

    // Initialize WiFi interface, TCP/IP stack and default event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    if (read_local_configuration() != ESP_OK)
    {
        // Initiate AP mode and web server
        ESP_LOGE(TAG, "Failed to read local configuration");
        wifi_init_softap();
        ESP_LOGI(TAG, "Starting Configuration server...\n");
        setup_server();
        return 1;
    }

    ESP_ERROR_CHECK(wifi_connect_from_config());

    http_get_remote_configuration();

    // Create queue
    queue = xQueueCreate(configs.QUEUE_SIZE, sizeof(cJSON *));
    if (queue == 0)
    {
        ESP_LOGE(TAG, "Failed to create queue= %p\n", queue);
        return -1;
    }

    // Create semaphore
    xSemaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(xSemaphore);

    start_i2c();
    mpu6050_init();

    scan_parameters_t *scanTaskParams = malloc(sizeof(scan_parameters_t));
    scanTaskParams->xQueue = queue;
    scanTaskParams->xSemaphore = xSemaphore;
    scanTaskParams->scan_led_pin = SCAN_LED_PIN;
    scanTaskParams->semaphore_wait_time = SEMAPHORE_WAIT_TIME;

    send_parameters_t *sendTaskParams = malloc(sizeof(scan_parameters_t));
    sendTaskParams->xQueue = queue;
    sendTaskParams->xSemaphore = xSemaphore;
    sendTaskParams->send_led_pin = SEND_LED_PIN;
    sendTaskParams->semaphore_wait_time = SEMAPHORE_WAIT_TIME;

    // Create tasks
    xTaskCreatePinnedToCore(scan_task, "Scan task", 4096, scanTaskParams, 10, &scanTaskHandle, 0);
    xTaskCreatePinnedToCore(send_task, "Send task", 4096, sendTaskParams, 10, &sendTaskHandle, 1);
    xTaskCreate(setApModeConfigTask, "Ap mode task", 4096, NULL, 1, &setApModeConfigTaskHandle);

    return 0;
}