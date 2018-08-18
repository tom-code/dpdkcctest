
#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>

struct eth_header_t {
  uint8_t dst[6];
  uint8_t src[6];
  uint16_t type;
};

void arp_in(uint8_t *data, int len);

void eth_switch(uint8_t *data, int len) {
  eth_header_t *eth = (eth_header_t*)data;
  int type = ntohs(eth->type);
  printf("type %x\n", type);

  switch (type) {
    case 0x806: //arp
      arp_in(data, len);
    break;

    case 0x800:  //ipv4
    case 0x86dd: //ipv6
    break;
  }

}
