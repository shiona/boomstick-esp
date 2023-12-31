#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "esp_sntp.h"
#include "driver/gpio.h"
#include "freertos/timers.h"

#include "artnet.h"
#include "battery.h"
#include "config.h"
#include "console.h"
#include "npp.h"
#include "util.h"
#include "wifi.h"

#define LED_COUNT 5

static const char* TAG = "main";


static int32_t button_pin;

static bool mqtt_connected = false;

esp_mqtt_client_handle_t mqtt_client; // = esp_mqtt_client_init(&mqtt_cfg);
esp_err_t wifi_init_sta(void);

static TimerHandle_t battery_timer;
static StaticTimer_t battery_timer_buffer;
static bool battery_timer_initialized = false;

static void battery_timer_callback(TimerHandle_t xTimer)
{
    if (npp_connected()) {
        npp_send_voltage(battery_voltage_mv());
    }
}

static void battery_timer_init(void)
{
        battery_timer = xTimerCreateStatic
                  ( /* Just a text name, not used by the RTOS
                    kernel. */
                    "Battery timer",
                    /* The timer period in ticks. 5 minutes */
                    //pdMS_TO_TICKS(5 * 60 * 1000),
                    pdMS_TO_TICKS(5 * 1000),
                    /* The timer will auto-reload */
                    pdTRUE,
                    /* The ID is used to store a count of the
                    number of times the timer has expired, which
                    is initialised to 0. */
                    ( void * ) 0,
                    /* Each timer calls the same callback when
                    it expires. */
                    battery_timer_callback,
                    /* Pass in the address of a StaticTimer_t
                    variable, which will hold the data associated with
                    the timer being created. */
                    &( battery_timer_buffer )
                  );
        battery_timer_initialized = true;
}


static void battery_timer_start(void)
{
    if (battery_timer_initialized) {
        xTimerStart(battery_timer, 0);
    }
}

static void battery_timer_stop(void)
{
    xTimerStop(battery_timer, 0);
}


static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    char topic[30];
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            sprintf(topic, "device/%s/color", get_mac());
            ESP_LOGI(TAG, "Subscribing to \"%s\"", topic);
            esp_mqtt_client_subscribe(mqtt_client, topic, 1);
            esp_mqtt_client_publish(mqtt_client, "conn", get_mac(), 0, 1, 1);

            break;
        case MQTT_EVENT_DISCONNECTED:
            mqtt_connected = false;
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            battery_timer_stop();
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
            ESP_LOGI(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
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
    static char mqtt_broker_uri[MAX_BROKER_URI_LEN];
    esp_mqtt_client_config_t mqtt_cfg = {
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        .broker.address.uri = mqtt_broker_uri,
    };
    load_broker_uri((uint8_t*)mqtt_broker_uri);

    printf("loaded mqtt uri: %s\n", mqtt_cfg.broker.address.uri);

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);

    // Fails if broker uri is invalid, must not crash
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, mqtt_client);

    return esp_mqtt_client_start(mqtt_client);

}

void gpio_init(void)
{
    if (load_button_pin(&button_pin) == ESP_OK && button_pin >= 0)
    {
        ESP_LOGI(TAG, "button pin: %d", (int)button_pin);
        gpio_config_t boot_pin_io_config = {
            .pin_bit_mask = 1 << button_pin,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        ESP_ERROR_CHECK(gpio_config(&boot_pin_io_config));
    }
    else
    {
        button_pin = -1;
    }
}

void app_main(void)
{

    ESP_ERROR_CHECK(nvs_flash_init());
    util_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    console_start();
    battery_init();
    battery_timer_init();

    // Fails if can't connect. Must not crash, so user
    // can configure the creds
    wifi_init_sta();

    while (!wifi_ready())
    {
        vTaskDelay(500);
    }

    gpio_init();
    // Fails if can't connect, argument is incorrect or other problems.
    // Must not crash so user can configure the broker uri
    //mqtt_start();
    npp_task_start();

    battery_timer_start();

    artnet_task_start();

    unsigned int last_time_button_sent = 0;

    while(true)
    {
        if (button_pin >= 0 && gpio_get_level(button_pin) == 0)
        {
            if (npp_connected()) {
                unsigned int currTime = xTaskGetTickCount();
                if (currTime >= last_time_button_sent + pdMS_TO_TICKS(CONFIG_BUTTON_REPEAT_DELAY))
                {
                    last_time_button_sent = currTime;
                    npp_send_button_press();
                }
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

}
