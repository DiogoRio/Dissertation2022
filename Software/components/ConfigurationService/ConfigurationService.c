#include "ConfigurationService.h"

static const char *CONFIG_SERVICE_TAG = "CONFIG_SERVICE";

struct ConfigurationVariables configs = {0};

char *html = NULL;
char *configJSON = NULL;

esp_err_t write_configs_to_file(char *filename, char *data)
{
    FILE *fptr;
    fptr = fopen(filename, "w");

    if (fptr == NULL)
    {
        return ESP_FAIL;
    }

    fwrite(data, sizeof(char), strlen(data), fptr);

    fclose(fptr);
    return ESP_OK;
}

esp_err_t parse_JSON_and_store_in_configs(char *text)
{

    // Logs if the tag is already self configured
    if (self_configured)
        ESP_LOGI(CONFIG_SERVICE_TAG, "Tag is self-configured");
    else
    {
        ESP_LOGI(CONFIG_SERVICE_TAG, "SELF-CONFIGURING for the first time using stored file...");
    }

    esp_err_t err = ESP_OK;

    char *out = NULL;
    cJSON *json = NULL;

    const cJSON *tag_name = NULL;
    const cJSON *fp_server = NULL;
    const cJSON *config_server = NULL;
    const cJSON *fp_sleep = NULL;
    const cJSON *fp_collect = NULL;
    const cJSON *msg_sleep = NULL;
    const cJSON *queue_size = NULL;
    const cJSON *max_msg_attempts = NULL;
    const cJSON *max_msg_retries = NULL;
    const cJSON *max_wifi_retries = NULL;
    const cJSON *csi_mode = NULL;
    const cJSON *wifi_details = NULL;

    json = cJSON_Parse(text);
    if (!json)
    {
        // Error, log error and set err to ESP_FAIL
        ESP_LOGE(CONFIG_SERVICE_TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        err = ESP_FAIL;
    }
    else
    {
        // Parsing successful, get values and store in configs
        tag_name = cJSON_GetObjectItem(json, "TAG_NAME");
        if (cJSON_IsString(tag_name) && (tag_name->valuestring != NULL))
        {
            configs.TAG_NAME = strdup(tag_name->valuestring);
        }
        else
        {
            // If not self configured, set err to ESP_FAIL (as it is a critical error)
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read TAG_NAME");
        }

        fp_server = cJSON_GetObjectItem(json, "FINGERPRINTS_SERVER");
        if (cJSON_IsString(fp_server) && (fp_server->valuestring != NULL))
        {
            configs.FINGERPRINTS_SERVER = strdup(fp_server->valuestring);
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read FINGERPRINTS_SERVER");
        }

        config_server = cJSON_GetObjectItem(json, "CONFIG_SERVER");
        if (cJSON_IsString(config_server) && (config_server->valuestring != NULL))
        {
            configs.CONFIGURATION_SERVER = strdup(config_server->valuestring);
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read CONFIG_SERVER");
        }

        fp_sleep = cJSON_GetObjectItem(json, "FINGERPRINT_SERVICE_SLEEP");
        if (cJSON_IsNumber(fp_sleep))
        {
            configs.FINGERPRINT_SERVICE_SLEEP = fp_sleep->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read FINGERPRINT_SERVICE_SLEEP");
        }

        fp_collect = cJSON_GetObjectItem(json, "FINGERPRINT_SERVICE_COLLECT");
        if (cJSON_IsNumber(fp_collect))
        {
            configs.FINGERPRINT_SERVICE_COLLECT = fp_collect->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read FINGERPRINT_SERVICE_COLLECT");
        }

        msg_sleep = cJSON_GetObjectItem(json, "MESSAGE_SERVICE_SLEEP");
        if (cJSON_IsNumber(msg_sleep))
        {
            configs.MESSAGE_SERVICE_SLEEP = msg_sleep->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MESSAGE_SERVICE_SLEEP");
        }

        queue_size = cJSON_GetObjectItem(json, "QUEUE_SIZE");
        if (cJSON_IsNumber(queue_size))
        {
            configs.QUEUE_SIZE = queue_size->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read QUEUE_SIZE");
        }

        max_msg_attempts = cJSON_GetObjectItem(json, "MAX_MESSAGE_ATTEMPTS");
        if (cJSON_IsNumber(max_msg_attempts))
        {
            configs.MAX_MESSAGE_ATTEMPTS = max_msg_attempts->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MAX_MESSAGE_ATTEMPTS");
        }

        max_msg_retries = cJSON_GetObjectItem(json, "MAX_MESSAGE_RETRIES");
        if (cJSON_IsNumber(max_msg_retries))
        {
            configs.MAX_MESSAGE_RETRIES = max_msg_retries->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MAX_MESSAGE_RETRIES");
        }

        max_wifi_retries = cJSON_GetObjectItem(json, "MAX_WIFI_CONNECT_RETRIES");
        if (cJSON_IsNumber(max_wifi_retries))
        {
            configs.MAX_WIFI_CONNECT_RETRIES = max_wifi_retries->valuedouble;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MAX_WIFI_CONNECT_RETRIES");
        }

        csi_mode = cJSON_GetObjectItem(json, "CSI_MODE");
        if (cJSON_IsBool(csi_mode))
        {
            if (cJSON_IsTrue(csi_mode))
                configs.CSI_MODE = true;
            else
                configs.CSI_MODE = false;
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read CSI_MODE");
        }

        wifi_details = cJSON_GetObjectItem(json, "WIFI_DETAILS");
        if (cJSON_IsArray(wifi_details))
        {
            const int array_size = cJSON_GetArraySize(wifi_details);

            configs.WIFI_SSID = malloc(array_size * sizeof(char *));
            configs.WIFI_PWD = malloc(array_size * sizeof(char *));
            configs.WIFI_ARRAY_SIZE = array_size;

            for (int i = 0; i < array_size; i++)
            {
                cJSON *detail = cJSON_GetArrayItem(wifi_details, i);
                cJSON *pwd = cJSON_GetObjectItem(detail, "PWD");
                cJSON *ssid = cJSON_GetObjectItem(detail, "SSID");

                if (cJSON_IsString(pwd) && (pwd->valuestring != NULL))
                {
                    configs.WIFI_PWD[i] = strdup(pwd->valuestring);
                }
                else
                {
                    if (!self_configured)
                        err = ESP_FAIL;
                    ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read wifi password at position %d", i);
                }

                if (cJSON_IsString(ssid) && (ssid->valuestring != NULL))
                {
                    configs.WIFI_SSID[i] = strdup(ssid->valuestring);
                }
                else
                {
                    if (!self_configured)
                        err = ESP_FAIL;
                    ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read wifi ssid at position %d", i);
                }
            }
        }
        else
        {
            if (!self_configured)
                err = ESP_FAIL;
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read WIFI_DETAILS");
        }

        // Print json
        out = cJSON_Print(json);
        ESP_LOGI(CONFIG_SERVICE_TAG, "JSON: %s\n", out);
    }

    // Set self configured flag to true
    if (!self_configured)
        self_configured = 1;

    // Free memory
    cJSON_Delete(json);
    free(out);
    return err;
}

esp_err_t parse_JSON_and_store_in_spiffs(char *text)
{

    esp_err_t ret = ESP_OK;

    char *out = NULL;
    cJSON *json = NULL;

    const cJSON *tag_name = NULL;
    const cJSON *fp_server = NULL;
    const cJSON *config_server = NULL;
    const cJSON *fp_sleep = NULL;
    const cJSON *fp_collect = NULL;
    const cJSON *msg_sleep = NULL;
    const cJSON *queue_size = NULL;
    const cJSON *max_msg_attempts = NULL;
    const cJSON *max_msg_retries = NULL;
    const cJSON *max_wifi_retries = NULL;
    const cJSON *csi_mode = NULL;
    const cJSON *wifi_details = NULL;

    json = cJSON_Parse(text);
    if (!json)
    {
        ESP_LOGE(CONFIG_SERVICE_TAG, "Error before: [%s]\n", cJSON_GetErrorPtr());
        ret = ESP_FAIL;
    }
    else
    {
        tag_name = cJSON_GetObjectItem(json, "TAG_NAME");
        if (!(cJSON_IsString(tag_name) && (tag_name->valuestring != NULL)))
        {
            cJSON_AddStringToObject(json, "TAG_NAME", strdup(configs.TAG_NAME));
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read TAG_NAME, using latest valid value...");
        }

        fp_server = cJSON_GetObjectItem(json, "FINGERPRINTS_SERVER");
        if (!(cJSON_IsString(fp_server) && (fp_server->valuestring != NULL)))
        {
            cJSON_AddStringToObject(json, "FINGERPRINTS_SERVER", strdup(configs.FINGERPRINTS_SERVER));
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read FINGERPRINTS_SERVER, using latest valid value...");
        }

        config_server = cJSON_GetObjectItem(json, "CONFIG_SERVER");
        if (!(cJSON_IsString(config_server) && (config_server->valuestring != NULL)))
        {
            cJSON_AddStringToObject(json, "CONFIGURATION_SERVER", strdup(configs.CONFIGURATION_SERVER));
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read CONFIGURATION_SERVER, using latest valid value...");
        }

        fp_sleep = cJSON_GetObjectItem(json, "FINGERPRINT_SERVICE_SLEEP");
        if (!(cJSON_IsNumber(fp_sleep)))
        {
            cJSON_AddNumberToObject(json, "FINGERPRINT_SERVICE_SLEEP", configs.FINGERPRINT_SERVICE_SLEEP);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read FINGERPRINT_SERVICE_SLEEP, using latest valid value...");
        }

        fp_collect = cJSON_GetObjectItem(json, "FINGERPRINT_SERVICE_COLLECT");
        if (!cJSON_IsNumber(fp_collect))
        {
            cJSON_AddNumberToObject(json, "FINGERPRINT_SERVICE_COLLECT", configs.FINGERPRINT_SERVICE_COLLECT);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read FINGERPRINT_SERVICE_COLLECT, using latest valid value...");
        }

        msg_sleep = cJSON_GetObjectItem(json, "MESSAGE_SERVICE_SLEEP");
        if (!cJSON_IsNumber(msg_sleep))
        {
            cJSON_AddNumberToObject(json, "MESSAGE_SERVICE_SLEEP", configs.MESSAGE_SERVICE_SLEEP);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MESSAGE_SERVICE_SLEEP, using latest valid value...");
        }

        queue_size = cJSON_GetObjectItem(json, "QUEUE_SIZE");
        if (!cJSON_IsNumber(queue_size))
        {
            cJSON_AddNumberToObject(json, "QUEUE_SIZE", configs.QUEUE_SIZE);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read QUEUE_SIZE, using latest valid value...");
        }

        max_msg_attempts = cJSON_GetObjectItem(json, "MAX_MESSAGE_ATTEMPTS");
        if (!cJSON_IsNumber(max_msg_attempts))
        {
            cJSON_AddNumberToObject(json, "MAX_MESSAGE_ATTEMPTS", configs.MAX_MESSAGE_ATTEMPTS);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MAX_MESSAGE_ATTEMPTS, using latest valid value...");
        }

        max_msg_retries = cJSON_GetObjectItem(json, "MAX_MESSAGE_RETRIES");
        if (!cJSON_IsNumber(max_msg_retries))
        {
            cJSON_AddNumberToObject(json, "MAX_MESSAGE_RETRIES", configs.MAX_MESSAGE_RETRIES);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MAX_MESSAGE_RETRIES, using latest valid value...");
        }

        max_wifi_retries = cJSON_GetObjectItem(json, "MAX_WIFI_CONNECT_RETRIES");
        if (!cJSON_IsNumber(max_wifi_retries))
        {
            cJSON_AddNumberToObject(json, "MAX_WIFI_CONNECT_RETRIES", configs.MAX_WIFI_CONNECT_RETRIES);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read MAX_WIFI_CONNECT_RETRIES, using latest valid value...");
        }

        csi_mode = cJSON_GetObjectItem(json, "CSI_MODE");
        if (!cJSON_IsBool(csi_mode))
        {
            cJSON_AddBoolToObject(json, "CSI_MODE", configs.CSI_MODE);
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read CSI_MODE, using latest valid value...");
        }

        wifi_details = cJSON_GetObjectItem(json, "WIFI_DETAILS");
        if (!cJSON_IsArray(wifi_details))
        {
            ESP_LOGE(CONFIG_SERVICE_TAG, "Can't read WIFI_DETAILS, using latest valid value...");

            const int array_size = configs.WIFI_ARRAY_SIZE;
            wifi_details = cJSON_CreateArray();
            if (wifi_details == NULL)
            {
                ret = ESP_FAIL;
                goto end;
            }

            cJSON_AddItemToObject(json, "WIFI_DETAILS", wifi_details);

            for (int i = 0; i < array_size; i++)
            {
                cJSON *detail = cJSON_GetArrayItem(wifi_details, i);

                detail = cJSON_CreateObject();
                if (detail == NULL)
                {
                    ret = ESP_FAIL;
                    goto end;
                }
                cJSON_AddItemToArray(wifi_details, detail);

                cJSON_AddItemToObject(detail, "PWD", configs.WIFI_PWD[i]);
                cJSON_AddItemToObject(detail, "SSID", configs.WIFI_SSID[i]);
            }
        }

        // Print json
        out = cJSON_Print(json);

        // update configs
        parse_JSON_and_store_in_configs(out);

        // Store json in defaults file
        ESP_GOTO_ON_ERROR(write_configs_to_file("/spiffs/defaults.json", out), end, CONFIG_SERVICE_TAG, "Can't write remote configuration to file...");
    }

    goto end;

end:
    // Free memory
    cJSON_Delete(json);
    free(out);
    return ret;
}

esp_err_t read_configs_from_file(char *filename)
{
    esp_err_t ret = ESP_OK;

    FILE *f = NULL;
    long len = 0;
    char *data = NULL;

    /* open in read binary mode */
    f = fopen(filename, "rb");
    if (f == NULL)
    {
        ESP_LOGE(CONFIG_SERVICE_TAG, "File does not exist!");
        return ESP_ERR_NOT_FOUND;
    }
    /* get the length */
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    data = (char *)malloc(len + 1);

    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    ESP_GOTO_ON_ERROR(parse_JSON_and_store_in_configs(data), end, CONFIG_SERVICE_TAG, "Failed to parse local configurations!");

    goto end;

end:
    free(data);
    return ret;
}

esp_err_t read_local_configuration(void)
{
    return read_configs_from_file("/spiffs/defaults.json");
}

esp_err_t read_remote_configuration(char *jsonText)
{
    esp_err_t ret = ESP_OK;

    ESP_GOTO_ON_ERROR(parse_JSON_and_store_in_spiffs(jsonText), end, CONFIG_SERVICE_TAG, "Can't read remote configuration! Using defaults...");

    goto end;

end:
    return ret;
}

esp_err_t get_req_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

esp_err_t get_configs_handler(httpd_req_t *req)
{
    return httpd_resp_send(req, configJSON, HTTPD_RESP_USE_STRLEN);
}

esp_err_t submit_handler(httpd_req_t *req)
{
    char content[req->content_len + 1];

    int ret = httpd_req_recv(req, content, req->content_len);
    content[req->content_len] = '\0';

    if (ret <= 0)
    { /* 0 return value indicates connection closed */
        /* Check if timeout occurred */
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    ESP_LOGI(CONFIG_SERVICE_TAG, "content: %s\n", content);
    parse_JSON_and_store_in_spiffs(content);

    /* Send a simple response */
    const char resp[] = "Configuration was sucessfull!";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    esp_restart();
    return ESP_OK;
}

char *readFromFile(char *filename)
{
    FILE *f = NULL;
    long len = 0;
    char *data = NULL;

    /* open in read binary mode */
    f = fopen(filename, "rb");
    if (f == NULL)
    {
        ESP_LOGE(CONFIG_SERVICE_TAG, "File does not exist!");
        return ESP_ERR_NOT_FOUND;
    }
    /* get the length */
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    data = (char *)malloc(len + 1);

    fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    return data;
}

httpd_handle_t setup_server(void)
{

    html = readFromFile("/spiffs/index.html");
    configJSON = readFromFile("/spiffs/defaults.json");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = get_req_handler,
            .user_ctx = NULL};

        httpd_uri_t submit = {
            .uri = "/submit",
            .method = HTTP_POST,
            .handler = submit_handler,
            .user_ctx = NULL};

        httpd_uri_t uri_get_configs = {
            .uri = "/configs",
            .method = HTTP_GET,
            .handler = get_configs_handler,
            .user_ctx = NULL};

        httpd_register_uri_handler(server, &uri_get);
        httpd_register_uri_handler(server, &submit);
        httpd_register_uri_handler(server, &uri_get_configs);
    }

    return server;
}
