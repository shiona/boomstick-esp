#include <stdint.h>

struct artnet_packet_poll_reply
{
    uint8_t magic[8];
    uint8_t op[2]; // LSB
    uint8_t ip[4]; // MSB
    uint8_t port[2]; // LSB
    uint8_t fw_version[2]; // MSB
    uint8_t net_switch; // bytes 14-8 or universe
    uint8_t sub_switch; // bytes 7-4 or universe
    uint8_t oem[2]; // MSB
    uint8_t ubea;
    uint8_t status1;
    uint8_t esta[2]; // LSB
    uint8_t port_name[18]; // Null terminated
    uint8_t long_name[64]; // Null terminated
    uint8_t node_report[64]; // Null terminated
};
