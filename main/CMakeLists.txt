idf_component_register(SRCS "boomstick.c" "wifi.c" "console.c" "config.c" "artnet.c" "battery.c"
                    PRIV_REQUIRES esp_wifi esp_netif console mqtt nvs_flash led_strip esp_adc
                    INCLUDE_DIRS ".")
