/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <stdint.h>
#include <inttypes.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>

#include <lib/librte_net/rte_ether.h>
#include <lib/librte_net/rte_ip.h>
#include <lib/librte_net/rte_udp.h>

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32

static uint32_t g_from_ip = 192 + (168 << 8) + (80 << 16) + (10 << 24);
static uint32_t g_to_ip = 192 + (168 << 8) + (80 << 16) + (6 << 24);
static uint8_t g_from_mac_addr[RTE_ETHER_ADDR_LEN];
static uint8_t g_to_mac_addr[RTE_ETHER_ADDR_LEN] = { 0x00, 0x50, 0x56, 0xc0, 0x00, 0x02 };
int port = 0;


static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = RTE_ETHER_MAX_LEN,
	},
};

/* basicfwd.c: Basic DPDK skeleton forwarding example. */

/* static func need a previous declaration */
static void fill_pack(uint8_t *unit_packet, size_t pkt_size, uint8_t to_mac_addr[RTE_ETHER_ADDR_LEN],
    uint32_t from_ip, uint32_t to_ip, uint16_t from_port, uint16_t to_port);

/*
 * Initializes a given port using global settings and with the RX buffers
 * coming from the mbuf_pool passed as a parameter.
 */
static void
port_init(struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 0, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	uint16_t q;

	while (port < RTE_MAX_ETHPORTS &&
	       rte_eth_devices[port].data->owner.id != RTE_ETH_DEV_NO_OWNER) {
		port++;
    }

	/* Configure the Ethernet device. */
	if (rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf))
		rte_exit(EXIT_FAILURE, "Error with rte_eth_dev_configure\n");

	/* We dont need RX queue for Ethernet port. */

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		if (rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), NULL) < 0)
				rte_exit(EXIT_FAILURE, "Error with rte_eth_tx_queue_setup\n");
	}

	/* Start the Ethernet port. */
	if (rte_eth_dev_start(port) < 0)
		rte_exit(EXIT_FAILURE, "Error with rte_eth_dev_start\n");

	/* Display the port MAC address. */
	if (rte_eth_macaddr_get(port, (struct rte_ether_addr *)g_from_mac_addr) != 0)
		rte_exit(EXIT_FAILURE, "Error with rte_eth_macaddr_get\n");

	printf("FROM MAC: %02x %02x %02x %02x %02x %02x\n",
			g_from_mac_addr[0], g_from_mac_addr[1],
			g_from_mac_addr[2], g_from_mac_addr[3],
			g_from_mac_addr[4], g_from_mac_addr[5]);
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void
send_main(struct rte_mempool *mbuf_pool, int num)
{
	struct rte_mbuf *mbufs[BURST_SIZE];
	int pkt_size = 128;

	for (int i = 0; i < BURST_SIZE; i++) {
        mbufs[i] = rte_pktmbuf_alloc(mbuf_pool);
        if (!mbufs[i]) {
            rte_exit(EXIT_FAILURE, "Error with rte_pktmbuf_alloc\n");
        }

        mbufs[i]->pkt_len = pkt_size;
        mbufs[i]->data_len = pkt_size;
    }

	// send BURST_SIZE packets at a time. if there isnt enough, send the maximum number of packets
	int not_send = num;
	while (not_send > 0) {
		int this_burst_size = BURST_SIZE;
		// decide the number of packets to send this burst
		if (not_send < BURST_SIZE)
			this_burst_size = not_send;

		for (int i = 0; i < this_burst_size; i++) {
			uint8_t *unit_packet = rte_pktmbuf_mtod(mbufs[i], uint8_t *);
			int port_id = 2048;
            fill_pack(unit_packet, pkt_size, g_to_mac_addr,
                g_from_ip, g_to_ip, port_id, port_id);
		}

		int sent_num = rte_eth_tx_burst(port, 0, mbufs, this_burst_size);

		not_send -= sent_num;
	}
}

static void fill_pack(uint8_t *unit_packet, size_t pkt_size, uint8_t to_mac_addr[RTE_ETHER_ADDR_LEN],
    uint32_t from_ip, uint32_t to_ip, uint16_t from_port, uint16_t to_port) {

    struct rte_ether_hdr *ether = (struct rte_ether_hdr *)unit_packet;
    rte_memcpy(ether->s_addr.addr_bytes, g_from_mac_addr, RTE_ETHER_ADDR_LEN);
    rte_memcpy(ether->d_addr.addr_bytes, to_mac_addr, RTE_ETHER_ADDR_LEN);
    ether->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr *)(unit_packet + sizeof(struct rte_ether_hdr));
	ip->version_ihl = 0x45;
	ip->type_of_service = 0;
	ip->total_length = htons(pkt_size - sizeof(struct rte_ether_hdr));
	ip->packet_id = 0;
	ip->fragment_offset = 0;
	ip->time_to_live = 64; // ttl = 64
	ip->next_proto_id = IPPROTO_UDP;
	ip->src_addr = from_ip;
	ip->dst_addr = to_ip;
	
	// ip->hdr_checksum = 0;
	ip->hdr_checksum = rte_ipv4_cksum(ip);

    struct rte_udp_hdr *udp = (struct rte_udp_hdr *)(unit_packet + sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr));
	udp->src_port = from_port;
	udp->dst_port = to_port;
	uint16_t udplen = pkt_size - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr);
	udp->dgram_len = htons(udplen);

	// udp->dgram_cksum = 0;
	udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

    char *content = "Let evil recoil on those who slander me, in your faithfulness destroy them.";
    char *payload = (char *)(udp + 1);
    memcpy(payload, content, 75);
}

/*
 * The main function, which does initialization and calls the per-lcore
 * functions.
 */
int
main(int argc, char *argv[])
{
	struct rte_mempool *mbuf_pool;
	unsigned nb_ports;
	uint16_t portid;

	/* Initialize the Environment Abstraction Layer (EAL). */
	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");

	/* Check that there is an even number of ports to send/receive on. */
	// nb_ports = rte_eth_dev_count_avail();
	// if (nb_ports < 2 || (nb_ports & 1))
	// 	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");
	// no checking!! just one port seems to be OK.

	/* Creates a new mempool in memory to hold the mbufs. */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS * 1,
		MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
	port_init(mbuf_pool);

	if (rte_lcore_count() > 1)
		printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

	/* Call lcore_main on the main core only. */
	send_main(mbuf_pool, 3);

	/* clean up the EAL */
	rte_eal_cleanup();

	return 0;
}
