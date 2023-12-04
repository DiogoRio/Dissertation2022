#include <stdio.h>
#include "HttpService.h"

static const char *HTTP_SERVICE_TAG = "HTTP_SERVICE";

int http_post_fingerprint_helper(char *json)
{

    int ret = 0;
    int content_length = 0;

    esp_http_client_config_t config = {
        .url = configs.FINGERPRINTS_SERVER,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL)
    {
        ESP_LOGE(HTTP_SERVICE_TAG, "Failed to initialize HTTP client");
        ret = ESP_FAIL;
        esp_http_client_cleanup(client);
        return ret;
    }

    // Format post data
    ssize_t bufsz = snprintf(NULL, 0, "scanData=%s", json);
    char *post_data = malloc(bufsz + 1);
    snprintf(post_data, bufsz + 1, "scanData=%s", json);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK)
    {
        ESP_LOGE(HTTP_SERVICE_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        ret = -1;
        goto end;
    }
    else
    {
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen < 0)
        {
            ESP_LOGE(HTTP_SERVICE_TAG, "Write failed");
            ret = -2;
            goto end;
        }
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(HTTP_SERVICE_TAG, "HTTP client fetch headers failed");
            ret = -3;
            goto end;
        }
        else
        {
            const int http_status = esp_http_client_get_status_code(client);

            ESP_LOGI(HTTP_SERVICE_TAG, "HTTP POST Status = %d",
                     http_status);
            ret = http_status;
            goto end;
        }
    }

end:
    free(post_data);
    esp_http_client_cleanup(client);
    return ret;
}

esp_err_t http_get_configuration_data(char **data)
{
    esp_err_t ret = ESP_OK;

    char *output_buffer = NULL; // Buffer to store response of http request
    int content_length = 0;
    esp_http_client_config_t config = {
        .url = configs.CONFIGURATION_SERVER,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (client == NULL)
    {
        ESP_LOGE(HTTP_SERVICE_TAG, "Failed to initialize HTTP client");
        ret = ESP_FAIL;
        esp_http_client_cleanup(client);
        return ret;
    }

    // GET MAC
    char *mac = (char *)malloc(sizeof(char) * 18);
    get_mac_address(mac);

    // Format post data
    ssize_t bufsz = snprintf(NULL, 0, "object_={\"BSSID\":\"%s\"}", mac);
    char *post_data = malloc(bufsz + 1);
    snprintf(post_data, bufsz + 1, "object_={\"BSSID\":\"%s\"}", mac);

    ESP_GOTO_ON_ERROR(esp_http_client_set_method(client, HTTP_METHOD_POST), end, HTTP_SERVICE_TAG, "Failed to get remote config");

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    esp_err_t err = esp_http_client_open(client, strlen(post_data));
    if (err != ESP_OK)
    {
        ESP_LOGE(HTTP_SERVICE_TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    }
    else
    {
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen < 0)
        {
            ESP_LOGE(HTTP_SERVICE_TAG, "Write failed");
        }
        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0)
        {
            ESP_LOGE(HTTP_SERVICE_TAG, "HTTP client fetch headers failed");
        }
        else
        {
            const int response_length = esp_http_client_get_content_length(client);
            output_buffer = malloc(response_length + 1);

            int data_read = esp_http_client_read_response(client, output_buffer, response_length);

            output_buffer[response_length] = '\0';

            if (data_read >= 0)
            {
                ESP_LOGI(HTTP_SERVICE_TAG, "HTTP POST Status = %d, content_length = %lli",
                         esp_http_client_get_status_code(client),
                         esp_http_client_get_content_length(client));

                ESP_LOGI(HTTP_SERVICE_TAG, "HTTP POST payload = %s",
                         output_buffer);

                // Store post payload in data
                char *temp = malloc(strlen(output_buffer) + 1);
                strcpy(temp, output_buffer);
                *data = temp;
            }
            else
            {
                ESP_LOGE(HTTP_SERVICE_TAG, "Failed to read response");
            }
        }
    }

    goto end;

end:

    free(mac);
    free(post_data);
    free(output_buffer);

    esp_http_client_cleanup(client);
    return ret;
}

void http_get_remote_configuration(void)
{
    char *json_text;

    if (http_get_configuration_data(&json_text) == ESP_OK)
    {
        read_remote_configuration(json_text);

        free(json_text);
    }
    else
    {
        ESP_LOGE(HTTP_SERVICE_TAG, "Failed to get remote configuration, using default configs");
    }
}

esp_err_t http_post_fingerprints(cJSON *fp_Array)
{
    esp_err_t ret = ESP_OK;
    cJSON *body = cJSON_CreateObject();
    cJSON *copy = cJSON_Duplicate(fp_Array, cJSON_True); // This will be freed when body is freed
    char *mac = (char *)malloc(sizeof(char) * 18);

    char *out = NULL;

    wifi_config_t config;

    ESP_GOTO_ON_ERROR(esp_wifi_get_config(ESP_IF_WIFI_STA, &config), end, HTTP_SERVICE_TAG, "Failed to get wifi config");

    get_mac_address(mac);

    // Construct message
    cJSON_AddStringToObject(body, "tagName", configs.TAG_NAME);
    cJSON_AddStringToObject(body, "tagBSSID", mac);
    cJSON_AddStringToObject(body, "tagNetwork", (char *)config.sta.ssid);
    cJSON_AddStringToObject(body, "scanMode", "auto");
    if (configs.CSI_MODE)
    {
        cJSON_AddStringToObject(body, "dataType", "CSI");
        cJSON_AddItemToObject(body, "CSIdata", copy);
    }
    else
    {
        cJSON_AddStringToObject(body, "dataType", "Wi-Fi");
        cJSON_AddItemToObject(body, "WiFiData", copy);
    }

    // Print json
    out = cJSON_Print(body);
    ESP_LOGI(HTTP_SERVICE_TAG, "Sending FP to server: \n %s", out);

    if (http_post_fingerprint_helper(out) != 200)
    {
        ESP_LOGE(HTTP_SERVICE_TAG, "Failed to send FP to server");
        ret = ESP_FAIL;
    }

    goto end;

end:
    cJSON_Delete(body);
    free(out);
    free(mac);
    return ret;
}
