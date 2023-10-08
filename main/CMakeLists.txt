idf_component_register(SRCS "boomstick.c" "wifi.c"
                    PRIV_REQUIRES esp_wifi esp_netif mqtt nvs_flash led_strip
                    INCLUDE_DIRS ".")
