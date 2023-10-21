#include "common.h"
#include "config.h"
#include "nvs.h"

static int nvs_set_key_value_str(const char* key, const char* val)
{
    nvs_handle_t nvs;
    int err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err)
    {
        return -1;
    }

    err = nvs_set_str(nvs, key, val);
    if (err)
    {
        return -2;
    }

    nvs_close(nvs);
    return 0;
}

static int nvs_set_key_value_i32(const char* key, int32_t val)
{
    nvs_handle_t nvs;
    int err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs);
    if (err)
    {
        return -1;
    }

    err = nvs_set_i32(nvs, key, val);
    if (err)
    {
        return -2;
    }

    nvs_close(nvs);
    return 0;
}

static int nvs_get_key_value_str(const char* key, char* val, size_t* len)
{
    nvs_handle_t nvs;
    int err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err)
    {
        return -1;
    }

    //nvs_set_str(nvs, key, str_arg.val->sval[0]);
    err = nvs_get_str(nvs, key, val, len);
    if (err)
    {
        return -2;
    }

    nvs_close(nvs);
    return 0;
}

static int nvs_get_key_value_i32(const char* key, int32_t *val)
{
    nvs_handle_t nvs;
    int err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err)
    {
        return -1;
    }

    //nvs_set_str(nvs, key, str_arg.val->sval[0]);
    err = nvs_get_i32(nvs, key, val);
    if (err)
    {
        return -2;
    }

    nvs_close(nvs);
    return 0;
}

int save_ssid(const char* ssid)
{
    return nvs_set_key_value_str(NVS_KEY_SSID, ssid);
}

int save_pass(const char* password)
{
    return nvs_set_key_value_str(NVS_KEY_PASS, password);
}

int save_broker_uri(const char* broker)
{
    return nvs_set_key_value_str(NVS_KEY_BROKER_URI, broker);
}

int load_ssid(uint8_t* ssid)
{
    size_t len = MAX_WIFI_SSID_LEN;
    return nvs_get_key_value_str(NVS_KEY_SSID, (char*) ssid, &len);
}

int load_pass(uint8_t* pass)
{
    size_t len = MAX_WIFI_PASS_LEN;
    return nvs_get_key_value_str(NVS_KEY_PASS, (char*) pass, &len);
}

int load_broker_uri(uint8_t* broker)
{
    size_t len = MAX_BROKER_URI_LEN;
    return nvs_get_key_value_str(NVS_KEY_BROKER_URI, (char*) broker, &len);
}


/*
int save_artnet_universe(int32_t universe)
{
    return nvs_set_key_value_i32(NVS_KEY_ARTNET_UNIVERSE, universe);
}

int save_artnet_first_channel(int32_t channel)
{
    return nvs_set_key_value_i32(NVS_KEY_ARTNET_FIRST_CHANNEL, channel);
}

int save_strip_led_count(int32_t led_count)
{
    return nvs_set_key_value_i32(NVS_KEY_STRIP_LED_COUNT, led_count);
}

int load_artnet_universe(int32_t *universe)
{
    return nvs_get_key_value_i32(NVS_KEY_ARTNET_UNIVERSE, universe);
}

int load_artnet_first_channel(int32_t *channel)
{
    return nvs_get_key_value_i32(NVS_KEY_ARTNET_FIRST_CHANNEL, channel);
}

int load_strip_led_count(int32_t* led_count)
{
    return nvs_get_key_value_i32(NVS_KEY_STRIP_LED_COUNT, led_count);
}
*/

#define INT_CONFIG(fn_name, key) \
int save_##fn_name(int32_t val) \
{ \
    return nvs_set_key_value_i32( key, val ); \
} \
int load_##fn_name (int32_t* val) \
{ \
    return nvs_get_key_value_i32( key, val ); \
}

#include "config_int.x"

#undef INT_CONFIG

int load_ledc_pins(int32_t* rpin, int32_t* gpin, int32_t* bpin)
{
    int ret = -1;
    int32_t tmp = -1;
    if (load_r_pin(&tmp) == ESP_OK)
    {
        ret = 0;
        *rpin = tmp;
    }
    else {
        *rpin = -1;
    }

    if (load_g_pin(&tmp) == ESP_OK)
    {
        ret = 0;
        *gpin = tmp;
    }
    else {
        *gpin = -1;
    }

    if (load_b_pin(&tmp) == ESP_OK)
    {
        ret = 0;
        *bpin = tmp;
    }
    else {
        *bpin = -1;
    }

    return ret;
}
