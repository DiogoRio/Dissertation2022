idf_component_register(SRCS "WifiService.c"
                    INCLUDE_DIRS "include" REQUIRES "ConfigurationService" "esp_wifi" "freertos" "lwip")
