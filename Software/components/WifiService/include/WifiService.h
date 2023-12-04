#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "ConfigurationService.h"
#include "lwip/inet.h"
#include "ping/ping_sock.h"

#ifndef WIFI_SERVICE_H
#define WIFI_SERVICE_H

/* - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

bool is_wifi_connected();
char *get_mac_address(char *mac);
void wifi_connect(char *ssid, char *pwd);
esp_err_t wifi_connect_from_config();
esp_err_t wifi_init_softap();
void wifi_disconnect();

#endif
