#include "SendService.h"

static const char *SEND_SERVICE_TAG = "SEND_SERVICE";

void send_task(send_parameters_t *params)
{
    const char *pcTaskName = "Send task is running\r\n";
    ESP_LOGI(SEND_SERVICE_TAG, "%s", pcTaskName);

    bool semaphore_taken = false;
    unsigned short attempts_counter = 0;
    unsigned short retries_counter = 0;

    while (1)
    {

        cJSON *jsonValue = NULL;

        if (uxQueueMessagesWaiting(params->xQueue) > 0)
        {
            // There is fingerprints to send
            gpio_set_level(params->send_led_pin, true);
            ESP_LOGI(SEND_SERVICE_TAG, "LOOP send task");

            if (semaphore_taken || xSemaphoreTake(params->xSemaphore, params->semaphore_wait_time))
            {
                semaphore_taken = true;

                xQueuePeek(params->xQueue, &(jsonValue), (TickType_t)0);
                ESP_LOGI(SEND_SERVICE_TAG, "Sending data to server...");
                if (http_post_fingerprints(jsonValue) == ESP_OK)
                {
                    // Remove from queue
                    xQueueReceive(params->xQueue, &(jsonValue), (TickType_t)0);

                    // Set counter to 0 in case they were incremented
                    attempts_counter = 0;
                    retries_counter = 0;
                    cJSON_Delete(jsonValue);
                }
                else
                {
                    if (is_wifi_connected())
                    {
                        if (retries_counter >= configs.MAX_MESSAGE_RETRIES)
                        {
                            retries_counter = 0;
                            attempts_counter++;
                        }
                        else
                        {
                            retries_counter++;
                            ESP_LOGE(SEND_SERVICE_TAG, "Failed to send fingerprints to server... Retrying");
                            ESP_LOGE(SEND_SERVICE_TAG, "Retry nº %d", retries_counter);
                            continue;
                        }

                        if (attempts_counter >= configs.MAX_MESSAGE_ATTEMPTS)
                        {
                            ESP_LOGE(SEND_SERVICE_TAG, "Failed to send fingerprints to server");
                            esp_system_abort("Failed to send fingerprints to server (MAX_MESSAGE_ATTEMPTS reached)");
                        }

                        ESP_LOGE(SEND_SERVICE_TAG, "Retry nº %d, Attempt nº %d", retries_counter, attempts_counter);
                    }
                    else
                    {
                        ESP_LOGE(SEND_SERVICE_TAG, "No Wifi connection... Reconnecting");
                        wifi_disconnect();
                        esp_err_t connected = wifi_connect_from_config();
                        if (connected == ESP_OK)
                        {
                            ESP_LOGI(SEND_SERVICE_TAG, "Reconnected!");
                            continue;
                        }
                        else
                        {
                            ESP_LOGE(SEND_SERVICE_TAG, "Failed to reconnect");
                            esp_system_abort("Failed to reconnect");
                        }
                    }
                }

                ESP_LOGE(SEND_SERVICE_TAG, "Queue size: %d", uxQueueMessagesWaiting(params->xQueue));

                // if queue is not empty continue sending fingerprints
                if (uxQueueMessagesWaiting(params->xQueue) > 0 && attempts_counter <= 0)
                {
                    continue;
                }
            }
            else
            {
                ESP_LOGE(SEND_SERVICE_TAG, "Failed to take semaphore for sending");
                gpio_set_level(params->send_led_pin, false);
                continue;
            }

            xSemaphoreGive(params->xSemaphore);
            semaphore_taken = false;

            gpio_set_level(params->send_led_pin, false);
        }

        vTaskDelay(configs.MESSAGE_SERVICE_SLEEP / portTICK_PERIOD_MS);
    }
}