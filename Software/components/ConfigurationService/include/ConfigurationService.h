#ifndef CONFIGURATION_SERVICE_H
#define CONFIGURATION_SERVICE_H

#include <stdio.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_spiffs.h"
#include <string.h>
#include <cJSON.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include <math.h>

// int to be set to 1 when running self-configuration (reading from stored file)
static int self_configured = 0;

struct ConfigurationVariables
{
  char *TAG_NAME;
  char *FINGERPRINTS_SERVER;
  char *CONFIGURATION_SERVER;
  unsigned short FINGERPRINT_SERVICE_SLEEP;
  unsigned short FINGERPRINT_SERVICE_COLLECT;
  unsigned short MESSAGE_SERVICE_SLEEP;
  unsigned short QUEUE_SIZE;
  unsigned short MAX_MESSAGE_ATTEMPTS;
  unsigned short MAX_MESSAGE_RETRIES;
  unsigned short MAX_WIFI_CONNECT_RETRIES;
  bool CSI_MODE;
  char **WIFI_SSID;
  char **WIFI_PWD;
  unsigned short WIFI_ARRAY_SIZE;
};

extern struct ConfigurationVariables configs;

esp_err_t read_local_configuration(void);
esp_err_t read_remote_configuration(char *jsonText);
httpd_handle_t setup_server(void);

#endif
