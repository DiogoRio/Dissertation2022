#include "WifiService.h"

static const char *WIFI_SERVICE_TAG = "WIFI_SERVICE";

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

static int connected = 0;
int s_retry_num = 0;

bool is_wifi_connected()
{
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);

    if (err == ESP_OK)
    {
        // WiFi connection is established
        return true;
    }
    else
    {
        // WiFi connection is not established
        return false;
    }
}

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGE(WIFI_SERVICE_TAG, "Connection to AP failed!");
        if (s_retry_num < configs.MAX_WIFI_CONNECT_RETRIES)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(WIFI_SERVICE_TAG, "Retrying to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_retry_num = 0;
            connected = 0;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(WIFI_SERVICE_TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        connected = 1;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

char *get_mac_address(char *mac)
{
    unsigned char mac_base[6] = {0};

    esp_read_mac(mac_base, ESP_MAC_WIFI_STA);

    sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X", mac_base[0], mac_base[1], mac_base[2], mac_base[3], mac_base[4], mac_base[5]);

    return mac;
}

void wifi_connect(char *ssid, char *pwd)
{
    s_wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    strncpy((char *)wifi_config.sta.ssid, ssid, 32);
    strncpy((char *)wifi_config.sta.password, pwd, 64);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(WIFI_SERVICE_TAG, "Connecting...");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(WIFI_SERVICE_TAG, "Connected to ap SSID: %s ",
                 ssid);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(WIFI_SERVICE_TAG, "Failed to connect to SSID: %s",
                 ssid);
        esp_wifi_stop();
    }
    else
    {
        ESP_LOGE(WIFI_SERVICE_TAG, "UNEXPECTED EVENT");
        esp_wifi_stop();
    }

    // CLEAR MEMORY
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id);
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t wifi_connect_from_config()
{

    for (int i = 0; i < configs.WIFI_ARRAY_SIZE; i++)
    {
        ESP_LOGI(WIFI_SERVICE_TAG, "Attempting to connect to %s...", configs.WIFI_SSID[i]);
        wifi_connect(configs.WIFI_SSID[i], configs.WIFI_PWD[i]);
        if (connected)
            break;
    }

    if (connected)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/* Initialize soft AP */
esp_err_t wifi_init_softap(void)
{

    esp_err_t ret = ESP_OK;

    esp_wifi_disconnect();
    esp_wifi_stop();

    ESP_GOTO_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_AP), end, WIFI_SERVICE_TAG, "esp_wifi_set_mode(WIFI_MODE_AP) failed");

    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = "ESP32-AP",
            .ssid_len = strlen("ESP32-AP"),
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);

    esp_netif_ip_info_t ipInfo;
    IP4_ADDR(&ipInfo.ip, 192, 168, 1, 1);
    IP4_ADDR(&ipInfo.gw, 192, 168, 1, 1);
    IP4_ADDR(&ipInfo.netmask, 255, 255, 255, 0);
    esp_netif_dhcps_stop(esp_netif_ap);
    esp_netif_set_ip_info(esp_netif_ap, &ipInfo);
    esp_netif_dhcps_start(esp_netif_ap);

    ESP_LOGI(WIFI_SERVICE_TAG, "wifi_init_softap finished.");

    ESP_GOTO_ON_ERROR(esp_wifi_start(), end, WIFI_SERVICE_TAG, "esp_wifi_start() failed");

    goto end;

end:
    return ret;
}

void wifi_disconnect()
{
    ESP_LOGI(WIFI_SERVICE_TAG, "Disconnecting...");
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    ESP_ERROR_CHECK(esp_wifi_stop());
}
