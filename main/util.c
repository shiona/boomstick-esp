#include "util.h"

#include "string.h"

#include "esp_log.h"
#include "esp_mac.h"

static char MACHEX[17];

static const char *TAG = "UTIL";

static void mac_init(void)
{
    unsigned char MAC[8];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(MAC));
    sprintf(MACHEX, "%X", MAC[0]);
    unsigned int len = strlen((char*)MAC);
    for(int i = 1; i < len; i++)
    {
        MACHEX[3*i-1] = ':';
        sprintf(&MACHEX[3*i], "%X", MAC[i]);
    }
    ESP_LOGI(TAG, "MACHEX: %s", MACHEX);
}

void util_init(void)
{
    mac_init();
}

const char* get_mac(void)
{
    return MACHEX;
}
