#include "ScanService.h"

static const char *SCAN_SERVICE_TAG = "SCAN_SERVICE";

void scan_csi_cb(QueueHandle_t queue, wifi_csi_info_t *info)
{

    const wifi_pkt_rx_ctrl_t *rx_ctrl = &info->rx_ctrl;

    cJSON *jsonValue = NULL;
    jsonValue = cJSON_CreateObject();

    char *apMac = (char *)malloc(sizeof(char) * 18);
    sprintf(apMac, MACSTR, MAC2STR(info->mac));

    cJSON_AddStringToObject(jsonValue, "apMAC", apMac);
    cJSON_AddNumberToObject(jsonValue, "rssi", rx_ctrl->rssi);
    cJSON_AddNumberToObject(jsonValue, "channel", rx_ctrl->channel);
    cJSON_AddNumberToObject(jsonValue, "secondaryChannel", rx_ctrl->secondary_channel);
    cJSON_AddNumberToObject(jsonValue, "timestamp", rx_ctrl->timestamp);

    cJSON *dataArray = cJSON_CreateArray();

    for (int i = 1; i < info->len; i++)
    {
        cJSON *number = cJSON_CreateNumber(info->buf[i]);
        cJSON_AddItemToArray(dataArray, number);
    }

    cJSON_AddItemToObject(jsonValue, "data", dataArray);

    if (uxQueueSpacesAvailable(queue) <= 0)
    {
        ESP_LOGE(SCAN_SERVICE_TAG, "No space, removing first element...");
        cJSON *jsonValue = NULL;

        xQueueReceive(queue, &(jsonValue), (TickType_t)5);
        cJSON_Delete(jsonValue);
    }

    xQueueSend(queue, &jsonValue, (TickType_t)0);

    char *out = cJSON_Print(jsonValue);
    ESP_LOGI(SCAN_SERVICE_TAG, "CSIdata: %s\n", out);

    free(out);
    free(apMac);
}

void scan_csi_init(QueueHandle_t queue)
{
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = true,
        .manu_scale = false,
        .shift = false,
    };

    esp_wifi_set_csi_config(&csi_config);
    esp_wifi_set_csi_rx_cb(scan_csi_cb, queue);
}

esp_ping_handle_t scan_ping_router_init()
{
    esp_ping_handle_t ping_handle = NULL;

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.count = ESP_PING_COUNT_INFINITE;
    ping_config.interval_ms = 1000;
    ping_config.data_size = 1;

    esp_netif_ip_info_t local_ip;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &local_ip);
    ping_config.target_addr.u_addr.ip4.addr = ip4_addr_get_u32(&local_ip.gw);
    ping_config.target_addr.type = ESP_IPADDR_TYPE_V4;

    esp_ping_callbacks_t cbs = {0};
    esp_ping_new_session(&ping_config, &cbs, &ping_handle);

    return ping_handle;
}

void scan_csi(esp_ping_handle_t ping_handle)
{

    esp_ping_start(ping_handle);

    esp_wifi_set_csi(true);

    ESP_LOGI(SCAN_SERVICE_TAG, "Starting to collect CSI data...");

    vTaskDelay(configs.FINGERPRINT_SERVICE_COLLECT / portTICK_PERIOD_MS);

    ESP_LOGI(SCAN_SERVICE_TAG, "Stopping to collect CSI data...");

    esp_ping_stop(ping_handle);
    esp_wifi_set_csi(false);
}

void scan_rssi(QueueHandle_t queue)
{

    cJSON *jsonArray = cJSON_CreateArray();
    char *out = NULL;

    uint16_t ap_count = 0;

    const uint32_t scanTime = configs.FINGERPRINT_SERVICE_COLLECT / 13;

    wifi_scan_config_t scan_config = {
        .ssid = NULL,                         // Set to NULL to scan all SSIDs
        .bssid = NULL,                        // Set to NULL to scan all BSSIDs
        .channel = 0,                         // Set to 0 to scan all channels
        .show_hidden = true,                  // Set to true to scan hidden SSIDs
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,   // Set the scan type (active or passive)
        .scan_time.active.min = scanTime / 2, // Minimum active scan time per channel
        .scan_time.active.max = scanTime      // Maximum active scan time per channel
    };

    ESP_LOGI(SCAN_SERVICE_TAG, "Starting RSSI Scan...");
    esp_wifi_scan_start(&scan_config, true);

    esp_wifi_scan_get_ap_num(&ap_count);

    wifi_ap_record_t ap_info[ap_count];
    memset(ap_info, 0, sizeof(ap_info));

    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    for (int i = 0; (i < ap_count); i++)
    {
        cJSON *jsonObject = cJSON_CreateObject();
        char *mac = (char *)malloc(sizeof(char) * 18);
        sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", MAC2STR(ap_info[i].bssid));
        cJSON_AddStringToObject(jsonObject, "bssid", mac);
        cJSON_AddNumberToObject(jsonObject, "rssi", ap_info[i].rssi);
        cJSON_AddStringToObject(jsonObject, "name", (char *)ap_info[i].ssid);
        cJSON_AddItemToArray(jsonArray, jsonObject);

        free(mac);
    }

    if (uxQueueSpacesAvailable(queue) <= 0)
    {
        ESP_LOGE(SCAN_SERVICE_TAG, "No space, removing first element...");
        cJSON *jsonValue = NULL;

        xQueueReceive(queue, &(jsonValue), (TickType_t)5);
        cJSON_Delete(jsonValue);
    }

    xQueueSend(queue, &jsonArray, (TickType_t)0);

    // Print json
    out = cJSON_Print(jsonArray);
    ESP_LOGI(SCAN_SERVICE_TAG, "WifiData: %s\n", out);

    // Clear AP list and memory
    esp_wifi_clear_ap_list();
    free(out);
}

void scan_task(scan_parameters_t *params)
{
    ESP_LOGI(SCAN_SERVICE_TAG, "%s", "Scan task is running\r\n");

    esp_ping_handle_t ping_handle = NULL;

    if (configs.CSI_MODE)
    {
        ESP_LOGI(SCAN_SERVICE_TAG, "CSI MODE ENABLED");
        scan_csi_init(params->xQueue);
        ping_handle = scan_ping_router_init();
    }

    while (1)
    {
        gpio_set_level(params->scan_led_pin, true);
        ESP_LOGI(SCAN_SERVICE_TAG, "LOOP scan task");

        if (xSemaphoreTake(params->xSemaphore, params->semaphore_wait_time))
        {

            if (configs.CSI_MODE)
                scan_csi(ping_handle);
            else
                scan_rssi(params->xQueue);

            xSemaphoreGive(params->xSemaphore);

            gpio_set_level(params->scan_led_pin, false);
            vTaskDelay(configs.FINGERPRINT_SERVICE_SLEEP / portTICK_PERIOD_MS);
        }
        else
        {
            ESP_LOGE(SCAN_SERVICE_TAG, "Failed to take semaphore for scaning");
            gpio_set_level(params->scan_led_pin, false);
        }
    }
}
