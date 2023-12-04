#include "esp_http_client.h"
#include "ConfigurationService.h"
#include "WifiService.h"

#ifndef HttpService_H
#define HttpService_H

void http_get_remote_configuration(void);
esp_err_t http_post_fingerprints(cJSON *fp_Array);

#endif
