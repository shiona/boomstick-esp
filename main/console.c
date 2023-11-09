

#include "esp_console.h"
#include "esp_log.h"
#include "esp_system.h"
//#include "cmd_nvs.h"
#include "argtable3/argtable3.h"

#include "config.h"

#include <string.h>

#include "battery.h"

struct {
    struct arg_str *ssid;
    struct arg_str *pass;
    struct arg_end *end;
} wifi_arg;

struct {
    struct arg_str *uri;
    struct arg_end *end;
} broker_arg;

struct {
    struct arg_str *type;
    struct arg_end *end;
} led_type_arg;

struct {
    struct arg_int *universe;
    struct arg_int *channel;
    struct arg_int *led_count;
    struct arg_int *data_pin;
    struct arg_end *end;
} led_strip_arg;

struct {
    struct arg_int *universe;
    struct arg_int *channel;
    struct arg_int *r_pin;
    struct arg_int *g_pin;
    struct arg_int *b_pin;
    struct arg_end *end;
} led_rgb_arg;

struct {
    struct arg_int *pin;
    struct arg_end *end;
} button_arg;

static const char* TAG = "console";

static int wifi_handler(int argc, char** argv)
{
    if (argc == 1)
    {
        char ssid[32];
        load_ssid((uint8_t*)ssid);
        printf("Current configure SSID: %s\n", ssid);
        return 0;
    }

    int err = arg_parse(argc, argv, (void**) &wifi_arg);
    if (err)
    {
        arg_print_errors(stderr, wifi_arg.end, argv[0]);
        return 1;
    }

    if (strlen(wifi_arg.ssid->sval[0]) > 32) {
        printf("SSID is too long, max length is 32 characters");
        return 1;
    }

    if (strlen(wifi_arg.pass->sval[0]) > 64) {
        printf("Password is too long, max length is 64 characters");
        return 1;
    }

    else
    {
        err = save_ssid(wifi_arg.ssid->sval[0]);
        if (err)
            return err;
        return save_pass(wifi_arg.pass->sval[0]);
    }
}

static int broker_handler(int argc, char** argv)
{
    if (argc == 1)
    {
        char uri[32];
        load_broker_uri((uint8_t*)uri);
        printf("current broker uri: %s\n", uri);
        return 0;
    }

    int err = arg_parse(argc, argv, (void**) &broker_arg);
    if (err)
    {
        arg_print_errors(stderr, broker_arg.end, argv[0]);
        return 1;
    }

    if (strlen(broker_arg.uri->sval[0]) > 32) {
        printf("broker URI is too long, max length is 32 characters\n");
        return 1;
    }

    else
    {
        return save_broker_uri(broker_arg.uri->sval[0]);
    }
}

static int led_strip_handler(int argc, char** argv)
{
    if (argc == 1)
    {
        int32_t universe, first_channel, led_count, led_pin, led_type;
        load_led_type(&led_type);
        load_artnet_universe(&universe);
        load_artnet_first_channel(&first_channel);
        load_strip_pin(&led_pin);
        load_strip_led_count(&led_count);

        if (led_type != LED_STRIP) {
            printf("WARNING! Not in STRIP led mode!\n");
        }
        printf("artnet universe: %ld, pin: %ld, channels: %ld-%ld\n", universe, led_pin, first_channel, first_channel+ 4*led_count -1);
        return 0;
    }

    int err = arg_parse(argc, argv, (void**) &led_strip_arg);
    if (err)
    {
        arg_print_errors(stderr, led_strip_arg.end, argv[0]);
        return 1;
    }

    // TODO: Error checks and prints if needed
    save_led_type(LED_STRIP);
    save_artnet_universe(led_strip_arg.universe->ival[0]);
    save_artnet_first_channel(led_strip_arg.channel->ival[0]);
    save_strip_led_count(led_strip_arg.led_count->ival[0]);
    save_strip_pin(led_strip_arg.data_pin->ival[0]);

    return 0;
}

static int led_rgb_handler(int argc, char** argv)
{
    if (argc == 1)
    {
        int32_t r_pin, g_pin, b_pin, universe, first_channel, led_type;
        // TODO: Print universe, first chanel and r, g and b pins
        load_led_type(&led_type);
        load_artnet_universe(&universe);
        load_artnet_first_channel(&first_channel);
        load_r_pin(&r_pin);
        load_g_pin(&g_pin);
        load_b_pin(&b_pin);
        if (led_type != LED_RGB) {
            printf("WARNING! Not in RGB led mode!\n");
        }
        printf("artnet universe: %ld, channels: %ld-%ld\n", universe, first_channel, first_channel+3);
        printf("r_pin: %ld, g_pin: %ld, b_pin: %ld\n", r_pin, g_pin, b_pin);
        return 0;
    }

    int err = arg_parse(argc, argv, (void**) &led_rgb_arg);
    if (err)
    {
        arg_print_errors(stderr, led_rgb_arg.end, argv[0]);
        return 1;
    }

    // TODO: Error checks and prints if needed
    save_led_type(LED_RGB);
    save_artnet_universe(led_rgb_arg.universe->ival[0]);
    save_artnet_first_channel(led_rgb_arg.channel->ival[0]);
    save_r_pin(led_rgb_arg.r_pin->ival[0]);
    save_g_pin(led_rgb_arg.g_pin->ival[0]);
    save_b_pin(led_rgb_arg.b_pin->ival[0]);

    return 0;
}

static int button_handler(int argc, char** argv)
{
    if (argc == 1)
    {
        int32_t tmp;
        load_button_pin(&tmp);

        // TODO: Print set gpio pin
        printf("button configure on GPIO pin %"PRId32"\n", tmp);
        return 0;
    }

    int err = arg_parse(argc, argv, (void**) &button_arg);
    if (err)
    {
        arg_print_errors(stderr, button_arg.end, argv[0]);
        return 1;
    }

    save_button_pin(button_arg.pin->ival[0]);

    return 0;
}

static int reboot(int argc, char** argv)
{
    esp_restart();
}

static int voltage_handler(int argc, char** argv)
{
    printf("%d\n", battery_voltage_mv());
    return 0;
}


void register_custom_cmds()
{
    wifi_arg.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of the wifi to connect to");
    wifi_arg.pass = arg_str1(NULL, NULL, "<pass>", "Password to use for wifi");
    wifi_arg.end = arg_end(2);

    const esp_console_cmd_t wifi_cmd = {
        .command = "wifi",
        .help = "Set the ssid and the password to use for connecting to wifi",
        .hint = NULL,
        .func = &wifi_handler,
        .argtable = &wifi_arg
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_cmd));

    broker_arg.uri = arg_str1(NULL, NULL, "<uri>", "URI of MQTT broker");
    broker_arg.end = arg_end(1);
    const esp_console_cmd_t broker_cmd = {
        .command = "broker",
        .help = "Set the URI of the MQTT broker. For example mqtt://hostname:port",
        .hint = NULL,
        .func = &broker_handler,
        .argtable = &broker_arg
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&broker_cmd));

    led_strip_arg.universe = arg_int1(NULL, NULL, "<universe>", "Artnet universe");
    led_strip_arg.channel = arg_int1(NULL, NULL, "<channel>", "First channel to use");
    led_strip_arg.led_count = arg_int1(NULL, NULL, "<led count>", "Number of leds in the strip");
    led_strip_arg.data_pin = arg_int1(NULL, NULL, "<data pin>", "Led strip data pin");
    led_strip_arg.end = arg_end(4);

    const esp_console_cmd_t led_strip_cmd = {
        .command = "strip",
        .help = "Set the device to drive a ws2812 led strip",
        .hint = NULL,
        .func = &led_strip_handler,
        .argtable = &led_strip_arg
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&led_strip_cmd));

    led_rgb_arg.universe = arg_int1(NULL, NULL, "<universe>", "Artnet universe");
    led_rgb_arg.channel = arg_int1(NULL, NULL, "<channel>", "First channel to use");
    led_rgb_arg.r_pin = arg_int1(NULL, NULL, "<r pin>", "Data pin for red color channel, -1 to disable");
    led_rgb_arg.g_pin = arg_int1(NULL, NULL, "<g pin>", "Data pin for green color channel, -1 to disable");
    led_rgb_arg.b_pin = arg_int1(NULL, NULL, "<b pin>", "Data pin for blue color channel, -1 to disable");
    led_rgb_arg.end = arg_end(5);

    const esp_console_cmd_t led_rgb_cmd = {
        .command = "rgb",
        .help = "Set the device to drive a single rgb led with PWM",
        .hint = NULL,
        .func = &led_rgb_handler,
        .argtable = &led_rgb_arg
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&led_rgb_cmd));

    button_arg.pin = arg_int1(NULL, NULL, "<gpio pin>", "GPIO pin used for press button");
    button_arg.end = arg_end(1);

    const esp_console_cmd_t button_cmd = {
        .command = "button",
        .help = "Set the press button settings",
        .hint = NULL,
        .func = &button_handler,
        .argtable = &button_arg
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&button_cmd));

    const esp_console_cmd_t voltage_cmd = {
        .command = "voltage",
        .help = "Query current battery voltage",
        .hint = NULL,
        .func = &voltage_handler,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&voltage_cmd));

    const esp_console_cmd_t reboot_cmd = {
        .command = "reboot",
        .help = "Reboot the device, useful for applying new settings",
        .hint = NULL,
        .func = &reboot,
        .argtable = NULL
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&reboot_cmd));
}


void console_start()
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = "boomstick>";
    repl_config.max_cmdline_length = 80;

#if CONFIG_CONSOLE_STORE_HISTORY
    initialize_filesystem();
    repl_config.history_save_path = HISTORY_PATH;
    ESP_LOGI(TAG, "Command history enabled");
#else
    ESP_LOGI(TAG, "Command history disabled");
#endif

    /* Register commands */
    esp_console_register_help_command();
    register_custom_cmds();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
