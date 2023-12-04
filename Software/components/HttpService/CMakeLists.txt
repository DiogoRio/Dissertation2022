idf_component_register(SRCS "HttpService.c"
                    INCLUDE_DIRS "include" REQUIRES "esp_http_client" "ConfigurationService" "json" "WifiService")
