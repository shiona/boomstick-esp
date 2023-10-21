idf_component_register(SRCS "boomstick.c" "wifi.c" "console.c" "config.c" "artnet.c"
                    PRIV_REQUIRES esp_wifi esp_netif console mqtt nvs_flash led_strip
                    INCLUDE_DIRS ".")
