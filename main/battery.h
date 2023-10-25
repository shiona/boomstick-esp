#ifndef _BATTERY_H
#define _BATTERY_H

#include "esp_err.h"

esp_err_t battery_init(void);

int battery_voltage_mv(void);

#endif
