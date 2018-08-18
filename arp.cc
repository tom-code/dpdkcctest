#include <stdint.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>



struct eth_header_t {
  uint8_t dst[6];
  uint8_t src[6];
  uint16_t type;
};

struct arp_t {
  uint16_t hw_type;
  uint16_t pr_type;
  uint8_t  hw_size;
  uint8_t  pr_size;
  uint16_t opcode;
  uint8_t sender_mac[6];
  uint8_t sender_ip[4];
  uint8_t target_mac[6];
  uint8_t target_ip[4];
  uint8_t pad[18];
};

void send_data(uint8_t *data, int len);


uint8_t my_mac[] = {0x94, 0xde, 0x80, 0x6c, 0xef, 0x4a};
uint8_t my_ip[] = {192, 168, 1, 201};
void reply_arp(uint8_t *data, int len) {
  eth_header_t *eth = (eth_header_t*)data;
  arp_t *arp = (arp_t*)(data+sizeof(eth_header_t));

  if (memcmp(arp->target_ip, my_ip, 4)!= 0) {
    printf("not for me\n");
    return;
  }

  uint8_t buf[1024];
  eth_header_t *etho = (eth_header_t*)buf;
  arp_t *arpo = (arp_t*)(buf+sizeof(eth_header_t));

  memcpy(etho->dst, eth->src, 6);
  memcpy(etho->src, my_mac, 6);
  etho->type = htons(0x806);
  arpo->hw_type = htons(1);
  arpo->pr_type = htons(0x800);
  arpo->hw_size = 6;
  arpo->pr_size = 4;
  arpo->opcode = htons(2);
  memcpy(arpo->sender_mac, etho->src, 6);
  memcpy(arpo->target_mac, etho->dst, 6);
  memcpy(arpo->target_ip, arp->sender_ip, 4);
  memcpy(arpo->sender_ip, my_ip, 4);

  send_data(buf, sizeof(eth_header_t) + sizeof(arp_t));
}

void arp_in(uint8_t *data, int len) {

  arp_t *arp = (arp_t*)(data+sizeof(eth_header_t));
  printf("this is arp\n");
  if (ntohs(arp->opcode) == 1) { //request
    printf("this is arp request\n");
    reply_arp(data, len);
  }

}
