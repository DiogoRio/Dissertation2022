idf_component_register(SRCS "SendService.c"
                    INCLUDE_DIRS "include" REQUIRES "ConfigurationService" "WifiService" "HttpService" "driver")
