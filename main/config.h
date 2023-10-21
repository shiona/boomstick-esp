#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>

#define NVS_NAMESPACE "storage"
#define NVS_NAMESPACE "storage"
#define NVS_KEY_SSID "SSID"
#define NVS_KEY_PASS "PASS"
#define NVS_KEY_BROKER_URI "BROKER"

#define NVS_KEY_ARTNET_UNIVERSE "UNIVERSE"
#define NVS_KEY_ARTNET_FIRST_CHANNEL "CHANNEL"

#define NVS_KEY_LED_TYPE "LED_TYPE"

#define NVS_KEY_STRIP_LED_COUNT "LED_COUNT"
#define NVS_KEY_STRIP_PIN "LED_PIN_1"

#define NVS_KEY_LED_R_PIN "LED_PIN_1"
#define NVS_KEY_LED_G_PIN "LED_PIN_2"
#define NVS_KEY_LED_B_PIN "LED_PIN_3"

#define NVS_KEY_BUTTON_PIN "BUTTON_PIN"

#define MAX_WIFI_SSID_LEN 32
#define MAX_WIFI_PASS_LEN 64
#define MAX_BROKER_URI_LEN 32

enum led_type {
	LED_NONE,
	LED_STRIP,
	LED_RGB
};

/*
 * ssid : max len 32
 * return 0 on success
 */
int save_ssid(const char* ssid);

/*
 * password : max len 64
 * return 0 on success
 */
int save_pass(const char* password);

/*
 * broker : max len 32
 * return 0 on success
 */
int save_broker_uri(const char* broker);

int load_ssid(uint8_t* ssid);
int load_pass(uint8_t* pass);
int load_broker_uri(uint8_t* broker);

//int save_led_strip_type(const enum led_type* led_type);

#define INT_CONFIG(fn_name, key) \
int save_##fn_name(int32_t val); \
int load_##fn_name (int32_t* val);

#include "config_int.x"

#undef INT_CONFIG

//int save_artnet_universe(int32_t universe);
//int save_artnet_first_channel(int32_t channel);
//
//int save_strip_led_count(int32_t led_count);
//int save_strip_pin(int32_t pin);
//
//int save_ledc_pins(int32_t rpin, int32_t gpin, int32_t bpin);
//
//
//////int load_led_strip_type(enum led_type* led_type);
//
//int load_artnet_universe(int32_t* universe);
//int load_artnet_first_channel(int32_t* universe);
//
//int load_strip_led_count(int32_t* led_count);
//int load_strip_pin(int32_t* pin);
//
int load_ledc_pins(int32_t* rpin, int32_t* gpin, int32_t* bpin);

#endif
