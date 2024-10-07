// Assumes a full UDP message is passed as one
#include "artnet.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "esp_log.h"

#include "common.h"
#include "config.h"
#include "driver/ledc.h"
#include "led_strip.h"
#include "wifi.h"

#define ARTNET_MAGIC_HEADER "Art-Net\0"
#define ARTNET_MAGIC_HEADER_LEN 8

#define PORT 6454

// 16 bit firmware version for reporting
#define VERSION 1

// According to esta.orgs listing 0x7ff0-x7fff are
// reserved for prototyping
#define ESTA_CODE 0x7ff1

// Nice to have, sync packet latches new data
//static boolean synchronous = false;

static const char *TAG = "ART-NET";

led_strip_handle_t led_strip;

//#define LED_STRIP_GPIO 10
enum led_type led_type;

int32_t artnet_universe;
int32_t artnet_first_channel;

led_strip_handle_t led_strip;
static bool leds_initialized = false;

static int listen_sock = -1;

/* LED strip initialization with the GPIO and pixels number*/
led_strip_config_t strip_config = {
    .strip_gpio_num = -1, // The GPIO that connected to the LED strip's data line
    .max_leds = -1, // The number of LEDs in the strip,
    .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
    .led_model = LED_MODEL_WS2812, // LED strip model
    .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
};

led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false, // whether to enable the DMA feature
};

enum {
    ARTNET_OP_POLL = 0x2000,
    ARTNET_OP_POLL_REPLY = 0x2100,
    ARTNET_OP_OUTPUT = 0x5000,
    ARTNET_OP_TOD_REQUEST = 0x8000,
    ARTNET_OP_TOD_DATA = 0x8100,
};



static int init_led_strip()
{
    int32_t val;
    RETURN_ON_ERR(load_strip_led_count(&val));
    strip_config.max_leds = val;
    RETURN_ON_ERR(load_strip_pin(&val));
    strip_config.strip_gpio_num = val;
    if (strip_config.max_leds >= 1 &&
            strip_config.strip_gpio_num >= 0) {
        ESP_LOGI(TAG, "loading led strip");
        RETURN_ON_ERR(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    }
    return 0;
}

#define LEDC_TIMER LEDC_TIMER_2
#define LEDC_FREQ (3000)
#define LEDC_CHANNEL_R LEDC_CHANNEL_1
#define LEDC_CHANNEL_G LEDC_CHANNEL_3
#define LEDC_CHANNEL_B LEDC_CHANNEL_5

static int init_led_rgb()
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num  = LEDC_TIMER,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz         = LEDC_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_r_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_R,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = -1,
        .duty       = 0,
        .hpoint     = 50
    };

    ledc_channel_config_t ledc_g_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_G,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = -1,
        .duty       = 0,
        .hpoint     = 0
    };

    ledc_channel_config_t ledc_b_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_B,
        .timer_sel  = LEDC_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = -1,
        .duty       = 0,
        .hpoint     = 0
    };

    int32_t rpin, gpin, bpin;
    if (load_ledc_pins(&rpin, &gpin, &bpin) == ESP_OK)
    {
        ESP_LOGI(TAG, "loading ledc");
        if (rpin >= 0) {
            ledc_r_channel.gpio_num = rpin;
            ESP_LOGI(TAG, "r pin: %d", ledc_r_channel.gpio_num);
            ledc_channel_config(&ledc_r_channel);
        }
        if (gpin >= 0) {
            ledc_g_channel.gpio_num = gpin;
            ESP_LOGI(TAG, "g pin: %d", ledc_g_channel.gpio_num);
            ledc_channel_config(&ledc_g_channel);
        }
        if (bpin >= 0) {
            ledc_b_channel.gpio_num = bpin;
            ESP_LOGI(TAG, "b pin: %d", ledc_b_channel.gpio_num);
            ledc_channel_config(&ledc_b_channel);
        }
    }

    return 0;
}

int init(uint8_t *light_data_buf)
{
    int32_t val;
    int err;

    if (load_led_type(&val) == ESP_OK)
    {
        err = load_artnet_universe(&artnet_universe);
        if (err) {
            ESP_LOGW(TAG, "No artnet universe configured, defaulting to 0");
            artnet_universe = 0;
        }
        err = load_artnet_first_channel(&artnet_first_channel);
        if (err) {
            ESP_LOGW(TAG, "No artnet first channel configured, bailing");
            return err;
        }

        led_type = (enum led_type) val;
        if (led_type == LED_STRIP)
        {
            ESP_LOGI(TAG, "Initializing a led strip");
            return init_led_strip();
        }
        else if (led_type == LED_RGB)
        {
            ESP_LOGI(TAG, "Initializing a single rgb led");
            return init_led_rgb();
        }
        else
        {
            ESP_LOGI(TAG, "No leds");
        }
    }
    else
    {
        led_type = LED_NONE;
    }
    return 0;
}

struct led_rgbi {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t i;
};
static void artnet_send_tod_data(
        struct sockaddr *source_addr,
        socklen_t addr_len)
{
    char outbuf[34] = {0};

    strcpy(&outbuf[0], "Art-Net");
    outbuf[8] = ARTNET_OP_TOD_DATA & 0xff;
    outbuf[9] = (ARTNET_OP_TOD_DATA >> 8) & 0xff;

    outbuf[10] = (VERSION >> 8) & 0xff;
    outbuf[11] = VERSION & 0xff;

    // rdm version 1
    outbuf[12] = 0x01;

    // physical port 1
    outbuf[13] = 0x01;

    // 6 spares

    // bind index (???)
    outbuf[20] = 0x01;

    // net
    // "top 7 bits of the port-address of the output"
    outbuf[21] = (artnet_universe >> 8) & 0x7f;

    // command response
    outbuf[22] = 0x00; // contains the full list of devices

    // address
    // "low 8 bits of the port-address of the output",
    outbuf[23] = artnet_universe & 0xff;

    // UidTotalHi
    outbuf[24] = 0x00;
    // UidTotalLo
    outbuf[25] = 0x01;

    // BlockCount
    outbuf[26] = 0x00;

    // UidCount
    outbuf[27] = 0x01;

    // No idea what uid I should put here
    // This seems to be used as unique identifier for lights,
    // this value specifies which light is managed using rdm commands
    // from the desk.
    memcpy(&outbuf[28], "\xff\xff\x12\x34\56\x78", 6);

    sendto(listen_sock, outbuf, 34, -1,
                source_addr, addr_len);
}

static void artnet_send_poll_reply(
        struct sockaddr *source_addr,
        socklen_t addr_len)
{
    // TODO: Increase size and send optional fields

    char outbuf[207] = {0};

    strcpy(&outbuf[0], "Art-Net");
    outbuf[8] = ARTNET_OP_POLL_REPLY & 0xff;
    outbuf[9] = (ARTNET_OP_POLL_REPLY >> 8) & 0xff;
    // TODO: Test endianess
    const esp_ip4_addr_t ip = wifi_get_ip();
    memcpy(&(outbuf[10]), &ip, 4);
    outbuf[14] = PORT & 0xff;
    outbuf[15] = (PORT >> 8) & 0xff;

    outbuf[16] = (VERSION >> 8) & 0xff;
    outbuf[17] = VERSION & 0xff;

    /* Bits 14-8 of the 15 bit Port-Address are encoded
       into the bottom 7 bits of this field. This is used in
       combination with SubSwitch and SwIn[] or
       SwOut[] to produce the full universe address.
    */
    outbuf[18] = (artnet_universe >> 8) & 0x7f;
    /* Bits 7-4 of the 15 bit Port-Address are encoded
       into the bottom 4 bits of this field. This is used in
       combination with NetSwitch and SwIn[] or
       SwOut[] to produce the full universe address.
    */
    outbuf[19] = (artnet_universe >> 4) & 0x0f;

    // OEM Hi
    outbuf[20] = 0;

    // OEM
    outbuf[21] = 0;

    // Ubea Version, "if not programmed, contains zero"
    outbuf[22] = 0;

    // status1
    outbuf[23] = 0b11000000 // indicators in normal mode
               | 0b00110000 // Port-Address Programming Authority not used
                            // no dual boot,
               | 0b00000010 // capable of remove device management,
                            // no ubea
               ;

    // EstaManLo
    outbuf[24] = ESTA_CODE & 0xff;
    // EstaManHi
    outbuf[25] = (ESTA_CODE >> 8) & 0xff;

    // PortName[18]
    strcpy(&outbuf[26], "Boomstick");

    // LongName[64]
    strcpy(&outbuf[44], "Boomstick");
    // NodeReport[64]
    // format is "#xxxx [yyyy..] zzzz.."
    // where xxxx is a hex code defined in the spec
    // 0001 = RcPowerOk,
    // yyyy.. is the count of sent artnet poll replies
    // zzzz.. is any text that one wants to report back
    strcpy(&outbuf[108], "#0001 [1] bogus");

    // NumPortsHi
    outbuf[172] = 0;
    // NumPortsLo
    outbuf[173] = 1;

    // PortTypes[4]
    outbuf[174] = 0b10000000; // one input port
    outbuf[175] = 0; // others disabled
    outbuf[176] = 0; // others disabled
    outbuf[177] = 0; // others disabled
                     //
    // GoodInput[4]
    outbuf[178] = 0b10000000; // data received (?)
    outbuf[179] = 0b00001000; // input is disabled
    outbuf[180] = 0b00001000; // input is disabled
    outbuf[181] = 0b00001000; // input is disabled

    // GoodOutputA[4]
    // all zeroes

    // SwIn[4]
    outbuf[186] = 0; // .. no idea what the 15 bit port-address
                 // is supposed to be
    outbuf[187] = 0;
    outbuf[188] = 0;
    outbuf[189] = 0;

    // SwOut[4]
    outbuf[190] = 0; // .. no idea what the 15 bit port-address
                 // is supposed to be
    outbuf[191] = 0;
    outbuf[192] = 0;
    outbuf[193] = 0;

    // AcnPriority
    outbuf[194] = 0;

    // SwMacro
    outbuf[195] = 0; // No macros

    // SwRemote
    outbuf[196] = 0; // No remote

//static inline ssize_t recvfrom(int s,void *mem,size_t len,int flags,struct sockaddr *from,socklen_t *fromlen)
//static inline ssize_t sendto(int s,const void *dataptr,size_t size,int flags,const struct sockaddr *to,socklen_t tolen)
    //int send_len =
    sendto(listen_sock, outbuf, 207, -1,
                source_addr, addr_len);
}

bool handle_artnet(uint8_t *artnet_buf, size_t artnet_buf_len,
        struct sockaddr* source_addr,
        socklen_t addr_len)
{
    //ESP_LOGI(TAG, "received something on artnet");
    if (artnet_buf_len < 13)
    {
        ESP_LOGW(TAG, "packet too short");
        // Shortest packet is probably ArtPoll
        return false;
    }

    if (memcmp(artnet_buf, ARTNET_MAGIC_HEADER, ARTNET_MAGIC_HEADER_LEN) != 0)
    {
        ESP_LOGW(TAG, "incorrect magic value");
        // Not an artnet packet
        return false;
    }


    uint16_t opcode = artnet_buf[8] | artnet_buf[9] << 8;
    uint16_t protver = artnet_buf[10] << 8 | artnet_buf[11];

    // Common header is 12 bytes long
    uint8_t *payload = &artnet_buf[12];
    size_t payload_len = artnet_buf_len - 12;

    if (protver != 14)
    {
        ESP_LOGW(TAG, "Protocol version is not 14, is %d", protver);
        return false;
    }

    if (opcode == ARTNET_OP_OUTPUT)
    {
        //ESP_LOGI(TAG, "was light opcode");
        //uint8_t seq = (uint8_t)artnet_buf[12];
        //uint8_t phys = (uint8_t)artnet_buf[13];
        uint16_t universe = 0x7fff & (payload[2] | payload[3] << 8);
        uint16_t datalen = payload[4] << 8 | payload[5];

        if (datalen != payload_len - 6)
        {
            // Header is 18 bytes
            ESP_LOGW(TAG, "packet content length does not match header data");
            return false;
        }

        if (universe == artnet_universe)
        {
            //ESP_LOGI(TAG, "correct universe");
            if (led_type == LED_STRIP)
            {
                ESP_LOGI(TAG, "doing ledstrip with count %"PRIu32, strip_config.max_leds);
                for (int i = 0; i < strip_config.max_leds; i++)
                {
                    struct led_rgbi *led = i + ((struct led_rgbi*) &payload[6 + artnet_first_channel]);
                    uint8_t r = (led->r * led->i) >> 8;
                    uint8_t g = (led->g * led->i) >> 8;
                    uint8_t b = (led->b * led->i) >> 8;
                    led_strip_set_pixel(led_strip, i, r, g, b);
                }
                led_strip_refresh(led_strip);
            }
            else if (led_type == LED_RGB)
            {
                struct led_rgbi *led = ((struct led_rgbi*) &payload[6 + artnet_first_channel]);
                uint32_t r = led->r * led->i;
                uint32_t g = led->g * led->i;
                uint32_t b = led->b * led->i;
                r >>= 8;
                g >>= 8;
                b >>= 8;
                //ESP_LOGI(TAG, "doing single led, %.2"PRId32" %.2"PRId32" %.2"PRId32, r, g, b);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_G, g);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_B, b);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_R, r);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_G);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_B);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_R);
            }
        }
    }
    else if (opcode == ARTNET_OP_POLL)
    {
        uint8_t flags = payload[0];
        uint8_t priority = payload[1];
        // TODO: Understand the rest of the message and handle it.
        // TODO: Queue sending the reply instead of spending time here now
        artnet_send_poll_reply(source_addr, addr_len);
    }
    else if (opcode == ARTNET_OP_TOD_REQUEST) {
        artnet_send_tod_data(source_addr, addr_len);
    }
    else
    {
        ESP_LOGD(TAG, "Unknown packet opcode %04x", opcode);
    }
    vTaskDelay(5);
    return true;
};

// This will block for one second
static void show_ready(void)
{
    if (led_type == LED_STRIP)
    {
        led_strip_set_pixel(led_strip, 0, 0, 200, 0);
        led_strip_refresh(led_strip);
        vTaskDelay(pdMS_TO_TICKS(500));
        led_strip_set_pixel(led_strip, 0, 0, 0, 0);
        led_strip_refresh(led_strip);
    }
    else
    {
        ESP_LOGW(TAG, "No ledstrip for showing ready state");
    }
}


static void artnet_worker(void *bogus)
{
    show_ready();

    // IPv4 isn't too long for this
    //char addr_str[32];
    uint8_t rx_buffer[2048];
    int addr_family = AF_INET;
    int ip_protocol = 0;
    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(PORT);
    ip_protocol = IPPROTO_IP;

    listen_sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        //state = STATE_ERROR;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        //state = STATE_ERROR;
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    while (1) {

        //ESP_LOGI(TAG, "Waiting for UDP data");

        //assert(state == STATE_IDLE);

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int recv_len = recvfrom(listen_sock, rx_buffer, sizeof(rx_buffer) -1, 0,
                (struct sockaddr*) &source_addr, &addr_len);

        if (recv_len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }


        //if (source_addr.ss_family == PF_INET) {
        //    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
        //}

        //ESP_LOGI(TAG, "data from address: %s", addr_str);


        handle_artnet(rx_buffer, recv_len, (struct sockaddr*) &source_addr, addr_len);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

#define STACK_SIZE 4000
static StaticTask_t xTaskBuffer;
static StackType_t xStack[ STACK_SIZE ];
static TaskHandle_t task_handle = NULL;

void artnet_task_start(void)
{
    init(NULL);
    /*while(state != STATE_IDLE) {
        // Wait for wifi task to connect to a network
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    */

    task_handle = xTaskCreateStatic(
            artnet_worker,
            "arnet",
            STACK_SIZE,
            (void*) 0,
            //tskIDLE_PRIORITY,
            5,
            xStack,
            &xTaskBuffer
            );
}
