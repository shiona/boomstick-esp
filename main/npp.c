/*
 * Naaspeksi Prop Protocol (NPP)
 *
 * Props need to:
 *
 *  1 Send button press events
 *  2 Send battery status
 *  3 Find the server(s?) to communicate with
 *
 * For 1 and 2 MQTT works at least to some extent,
 * although there has been some latency that's not
 * ideal for 1. Lighterweight protocol may help.
 * MQTT also needs the extra broker server to run
 * in addition to the server script that then turns
 * the mqtt events into midi signals.
 *
 * For 3 DNS-SD could maybe work, but that's
 * another rather big protocol to pull into
 * the embedded device that is already running
 * out of code memory.
 *
 * Thus NPP:
 *
 *  - Uses UDP
 *  - All ASCII to make as simple as possible
 *  - Messages:
 *   - D[MAC]
 *    - Discover, sent as broadcast
 *   - R
 *    - Reply, sent as a reply to Discovery using unicast
 *   - B[MAC][button id]
 *    - Button press, button id is 0, but in the future may support more button
 *   - V[MAC][voltage]
 *    - Voltage is non-negative and one+two digits (e.g 3.24)
 *  - in the messages MAC is ascii, formatted as
 *    01:23:45:67:89:AB, or 17 characters
 */

#include <arpa/inet.h>
#include <sys/socket.h>

#include "util.h"

#include "esp_log.h"
#include "freertos/timers.h"


#define PORT 6566

static const char *TAG = "NPP";

static int listen_sock = -1;

static struct sockaddr_in broadcast_addr;

static struct sockaddr server;
static bool server_connected = false;

static char discovery_msg[1+17] = {0};
static char button_press_msg[1+17+1] = {0};
static char voltage_msg[1+17+4] = {0};

static int discovery_timer_id = 1; // Random value, no idea should this be set
static TimerHandle_t discovery_timer = 0;

#define DISCOVERY_INTERVAL_MS 10000

static void handle_npp(const char* rx_buf, ssize_t rx_len, struct sockaddr_in* source_addr)
{
    if (!server_connected) {
        if (rx_len == 1 && rx_buf[0] == 'R') {
            memcpy(&server, source_addr, sizeof(struct sockaddr));

            // TODO: probably want to make sure the connection works,
            // also useful to reset this if a ping timeout or similar
            // happens (after keepalive is implemented)
            server_connected = true;

            xTimerStop(discovery_timer, 0);
            ESP_LOGI(TAG, "Server found (printing ips sucks ass, skipping)");
        }
    }
}


static void npp_send_discovery(void)
{
    //ESP_LOGI(TAG, "Sending discovery");
    sendto(listen_sock, discovery_msg, sizeof(discovery_msg), 0, &broadcast_addr , sizeof(broadcast_addr));
}

void npp_send_button_press(void)
{
    //ESP_LOGI(TAG, "Sending button");
    sendto(listen_sock, button_press_msg, sizeof(button_press_msg), 0, &server, sizeof(server));
}

void npp_send_voltage(int voltage_mv)
{
    if (voltage_mv <= 0) {
        voltage_mv = 0;
    }
    if (voltage_mv >= 9990) {
        voltage_mv = 9990;
    }
    char tmp_voltage[5];
    snprintf(tmp_voltage, 5, "%04d", voltage_mv);
    memcpy(&voltage_msg[18], tmp_voltage, 4);

    sendto(listen_sock, voltage_msg, sizeof(voltage_msg), 0, &server, sizeof(server));
}

static void discovery_timer_callback( TimerHandle_t xTimer )
{
    npp_send_discovery();
}

static void npp_init(void)
{
    char *mac = get_mac();
    discovery_msg[0] = 'D';
    button_press_msg[0] = 'B';
    voltage_msg[0] = 'V';
    memcpy(&discovery_msg[1], mac, strlen(mac));
    memcpy(&button_press_msg[1], mac, strlen(mac));
    memcpy(&voltage_msg[1], mac, strlen(mac));
    button_press_msg[strlen(mac)+1] = '0';
    //snprintf(discovery_msg, sizeof(discovery_msg), "D%s", get_mac());
    //snprintf(button_press_msg, sizeof(button_press_msg), "B%s0", get_mac());
    //snprintf(voltage_msg, sizeof(voltage_msg), "V%s%1.2f", get_mac(), 0.0f);
}

static void npp_worker(void *bogus)
{
    char rx_buffer[256];
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
        vTaskDelete(NULL);
        return;
    }

    memset(&broadcast_addr, '\0', sizeof(struct sockaddr_in));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(PORT);
    broadcast_addr.sin_addr.s_addr = INADDR_BROADCAST; /* This is not correct : htonl(INADDR_BROADCAST); */

    ESP_LOGI(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    // Start timer for sending server discovery
    discovery_timer = xTimerCreate("DiscoveryTimer", pdMS_TO_TICKS(DISCOVERY_INTERVAL_MS), pdTRUE, ( void * )discovery_timer_id, &discovery_timer_callback);
    xTimerStart( discovery_timer, 0 );

    // This task could possibly be killed after a server is found,
    // but maybe we want to keep this listening forever just in
    // case someday we want multiple servers / be able to detect
    // dead server and switch to another one
    while (1) {

        struct sockaddr_storage source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int recv_len = recvfrom(listen_sock, rx_buffer, sizeof(rx_buffer) -1, 0,
                (struct sockaddr*) &source_addr, &addr_len);

        if (recv_len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        handle_npp(rx_buffer, recv_len, (struct sockaddr*) &source_addr);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

bool npp_connected()
{
    return server_connected;
}

#define STACK_SIZE 4000
static StaticTask_t xTaskBuffer;
static StackType_t xStack[ STACK_SIZE ];
static TaskHandle_t task_handle = NULL;

void npp_task_start(void)
{
    npp_init();

    // This task is only responsible for
    // listening to replies from a server,
    // and possibly the sending of battery
    // voltage status.
    // The priority does not affect how the
    // button presses are sent
    task_handle = xTaskCreateStatic(
            npp_worker,
            "npp",
            STACK_SIZE,
            (void*) 0,
            tskIDLE_PRIORITY,
            xStack,
            &xTaskBuffer
            );
}
