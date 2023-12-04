idf_component_register(SRCS "ScanService.c"
                    INCLUDE_DIRS "include" REQUIRES "ConfigurationService" "WifiService" "esp_timer" "driver")
