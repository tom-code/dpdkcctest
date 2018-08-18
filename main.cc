
#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>

#include <arpa/inet.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

struct rte_mempool *my_pool;


static const struct rte_eth_conf port_conf_default = {
  .rxmode = {
    .max_rx_pkt_len = ETHER_MAX_LEN,
    .ignore_offload_bitfield = 1,
  },
};

static unsigned nb_ports;

static int port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
  struct rte_eth_conf port_conf = port_conf_default;
  const uint16_t rx_rings = 1, tx_rings = 1;
  uint16_t nb_rxd = RX_RING_SIZE;
  uint16_t nb_txd = TX_RING_SIZE;
  int retval;
  uint16_t q;
  struct rte_eth_dev_info dev_info;
  struct rte_eth_txconf txconf;

  if (port >= rte_eth_dev_count())
    return -1;

  rte_eth_dev_info_get(port, &dev_info);
  if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
    port_conf.txmode.offloads |= DEV_TX_OFFLOAD_MBUF_FAST_FREE;

  retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
  if (retval != 0) return retval;

  retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
  if (retval != 0) return retval;

  for (q = 0; q < rx_rings; q++) {
    retval = rte_eth_rx_queue_setup(port, q, nb_rxd, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
    if (retval < 0) return retval;
  }

  txconf = dev_info.default_txconf;
  txconf.txq_flags = ETH_TXQ_FLAGS_IGNORE;
  txconf.offloads = port_conf.txmode.offloads;
  for (q = 0; q < tx_rings; q++) {
    retval = rte_eth_tx_queue_setup(port, q, nb_txd,
        rte_eth_dev_socket_id(port), &txconf);
    if (retval < 0)
      return retval;
  }

  retval  = rte_eth_dev_start(port);
  if (retval < 0)
    return retval;

  struct ether_addr addr;

  rte_eth_macaddr_get(port, &addr);
  printf("Port %u MAC: %02"PRIx8" %02"PRIx8" %02"PRIx8
      " %02"PRIx8" %02"PRIx8" %02"PRIx8"\n",
      (unsigned)port,
      addr.addr_bytes[0], addr.addr_bytes[1],
      addr.addr_bytes[2], addr.addr_bytes[3],
      addr.addr_bytes[4], addr.addr_bytes[5]);

  rte_eth_promiscuous_enable(port);

  return 0;
}


void send() {
  struct rte_mbuf *hdr;

  if (unlikely ((hdr = rte_pktmbuf_alloc(my_pool)) == NULL)) return;

  ether_hdr *eth_hdr = rte_pktmbuf_mtod(hdr, struct ether_hdr *);
  rte_eth_macaddr_get(0, &eth_hdr->s_addr);
  
  unsigned char b[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  ether_addr_copy((ether_addr*)b, &eth_hdr->d_addr);
  eth_hdr->ether_type = 0x608;

  struct arps {
    uint16_t hw_type = htons(1);
    uint16_t pr_type = htons(0x800);
    uint8_t  hw_size = 6;
    uint8_t  pr_size = 4;
    uint16_t opcode  = htons(1);
    uint8_t sender_mac[6];
    uint8_t sender_ip[4] = {192, 168, 1, 200};
    uint8_t target_mac[6] = {0,0,0,0,0,0};
    uint8_t target_ip[4] = {192, 168, 1, 3};
    uint8_t pad[18];
  } arpd;
  rte_eth_macaddr_get(0, (ether_addr*)arpd.sender_mac);

  uint8_t *data = (rte_pktmbuf_mtod(hdr, unsigned char *) + sizeof(struct ether_hdr));
  memcpy(data, &arpd, sizeof(arpd));

  hdr->data_len = sizeof(struct ether_hdr) + sizeof(arpd);
  hdr->pkt_len = sizeof(struct ether_hdr) + sizeof(arpd);

  rte_eth_tx_burst(0, 0, &hdr, 1);

  rte_pktmbuf_free(hdr);
}

void send_data(uint8_t *data, int len) {
  struct rte_mbuf *hdr;
  if (unlikely ((hdr = rte_pktmbuf_alloc(my_pool)) == NULL)) return;

  hdr->data_len = len;
  hdr->pkt_len = len;
  uint8_t *odata = rte_pktmbuf_mtod(hdr, unsigned char *);
  memcpy(odata, data, len);
  rte_eth_tx_burst(0, 0, &hdr, 1);
  rte_pktmbuf_free(hdr);
}

void eth_switch(uint8_t *data, int len);
struct rte_mempool *mbuf_pool;
static  __attribute__((noreturn)) void lcore_main(void)
{
  uint16_t port;

  for (port = 0; port < nb_ports; port++)
    if (rte_eth_dev_socket_id(port) > 0 && rte_eth_dev_socket_id(port) != (int)rte_socket_id())
      printf("WARNING, port %u is on remote NUMA node to " "polling thread.\n\tPerformance will " "not be optimal.\n", port);

  printf("\ncore %u processing packets\n", rte_lcore_id());

  for (;;) {
    for (port = 0; port < nb_ports; port++) {
      struct rte_mbuf *bufs[BURST_SIZE];
      const uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, BURST_SIZE);
      if (unlikely(nb_rx == 0))
        continue;

      for (int buf_idx=0; buf_idx < nb_rx; buf_idx++) {
        //printf("got packet %d\n", nb_rx);
        unsigned char *b = rte_pktmbuf_mtod(bufs[buf_idx], unsigned char*);
        int len = rte_pktmbuf_pkt_len(bufs[buf_idx]);
        eth_switch(b, len);
        for (int i=0; i<len; i++) {
          printf("%02x ", (unsigned char)b[i]);
        }
        //printf("\n");
        //for (int i=0; i<len; i++) {
        //  if (b[i]>30) printf("%c", b[i]);
        //}
        printf("\n");
        //if (b[5] != 0x4a) send();
      }

        uint16_t buf;

        for (buf = 0; buf < nb_rx; buf++)
          rte_pktmbuf_free(bufs[buf]);
    }
  }
}

static int slave_main(__attribute__((unused)) void *arg1) {
  while (true) {
  }
}

void start_slave() {
  int master_core = rte_lcore_id();
  int slave_core = rte_get_next_lcore(rte_lcore_id(), 1, 0);
  printf("cores %d %d\n", master_core, slave_core);

  rte_eal_remote_launch(slave_main, NULL, slave_core);
}

int main(int argc, char *argv[])
{

  int ret = rte_eal_init(argc, argv);
  if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
  argc -= ret;
  argv += ret;

  printf("total cores %d\n", rte_lcore_count());

  nb_ports = rte_eth_dev_count();

  my_pool = rte_pktmbuf_pool_create("my_POOL", NUM_MBUFS * nb_ports , MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * nb_ports, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
  if (mbuf_pool == NULL)
    rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

  /* initialize all ports */
  for (uint16_t portid = 0; portid < nb_ports; portid++)
    if (port_init(portid, mbuf_pool) != 0)
      rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8"\n",
          portid);


  start_slave();
  lcore_main();
  return 0;
}
