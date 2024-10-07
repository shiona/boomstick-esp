#ifndef _WIFI_H
#define _WIFI_H

#include "esp_err.h"
#include "esp_wifi.h"

#include <stdbool.h>

bool wifi_ready();
esp_err_t wifi_init_sta(void);
esp_ip4_addr_t wifi_get_ip(void);

#endif
