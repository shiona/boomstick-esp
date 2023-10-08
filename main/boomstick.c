

#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_sntp.h"
#include "driver/gpio.h"
#include "led_strip.h"

//#define CONFIG_SNTP_SERVER "192.168.0.1"

static const char* TAG = "main";

static unsigned char MAC[8];
static char MACHEX[21];

#define BUTTON_PIN 9

gpio_config_t boot_pin_io_config = {
    .pin_bit_mask = 1 << BUTTON_PIN,
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
};

#define LED_STRIP_GPIO 10

led_strip_handle_t led_strip;
bool leds_initialized = false;

/* LED strip initialization with the GPIO and pixels number*/
led_strip_config_t strip_config = {
    .strip_gpio_num = LED_STRIP_GPIO, // The GPIO that connected to the LED strip's data line
    .max_leds = 3, // The number of LEDs in the strip,
    .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
    .led_model = LED_MODEL_WS2812, // LED strip model
    .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
};

led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false, // whether to enable the DMA feature
};

esp_mqtt_client_handle_t mqtt_client; // = esp_mqtt_client_init(&mqtt_cfg);
esp_err_t wifi_init_sta(void);

/*
void sntp(void)
{
    sntp_setservername(0, CONFIG_SNTP_SERVER);
    sntp_init();
}
*/

static void led_effect()
{
    if (!leds_initialized)
        return;

    static uint32_t r = 0, g = 0, b = 0;

    r = (r + 64) & 0xff;
    g = (g + 32) & 0xff;
    led_strip_set_pixel(led_strip, 1, r, g, b);
    led_strip_refresh(led_strip);
}

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(mqtt_client, "device/EC:DA:3B:AA:C1:60/1", 2);
            esp_mqtt_client_subscribe(mqtt_client, "#", 2);
            //ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
            //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            //msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
            //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            //msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
            //ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            led_effect();
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int) event_id);
    mqtt_event_handler_cb(event_data);
}


esp_err_t mqtt_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .broker.address.uri = CONFIG_MQTT_BROKER_URI,
        .credentials.username = CONFIG_MQTT_USERNAME,
        .credentials.authentication.password = CONFIG_MQTT_PASSWORD
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    //esp_mqtt_client_subscribe(mqtt_client, "+", 2);
    //esp_mqtt_client_subscribe(mqtt_client, "#", 2);

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client));
    return esp_mqtt_client_start(mqtt_client);

}

void gpio_init(void)
{
    ESP_ERROR_CHECK(gpio_config(&boot_pin_io_config));
}

void get_mac(void)
{
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


void app_main(void)
{

    ESP_ERROR_CHECK(nvs_flash_init());
    get_mac();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(wifi_init_sta());
    //sntp();
    gpio_init();
    ESP_ERROR_CHECK(mqtt_start());
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    leds_initialized = true;

    unsigned int last_time_button_sent = 0;

    esp_mqtt_client_publish(mqtt_client, "conn", MACHEX, 0, 1, 1);
    while(true)
    {
        if (gpio_get_level(BUTTON_PIN) == 0)
        {
            unsigned int currTime = xTaskGetTickCount();
            if (currTime >= last_time_button_sent + pdMS_TO_TICKS(CONFIG_BUTTON_REPEAT_DELAY))
            {
                last_time_button_sent = currTime;
                char topic[64];
                snprintf(topic, 64, "device/%s/%d", MACHEX, 1);
                int res = esp_mqtt_client_publish(mqtt_client, topic, NULL, 0, 1, 1);
                ESP_LOGI(TAG, "mqtt publish returned %d", res);
            }
        }
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

}
