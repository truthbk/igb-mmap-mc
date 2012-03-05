/*
 *
 *       Filename:  sf.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  10/04/11 12:50:15
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Jaime Fullaondo (truthbk@gmail.com), 
 *        Company:  HPCN, Universidad Atuonoma de Madrid
 *
 */

#ifndef _SF_H_
#define _SF_H_

#define MAX_DEVICES	16

#ifdef __KERNEL__

#define SF_MAJOR 1010
#define SF_NAME "somber_frames"

#define MAX_BUFS 16

struct ____cacheline_aligned sf_context {
	struct semaphore sem;
	wait_queue_head_t wq;

	struct igb_adapter *adapter;

	struct sf_pkt_info *info;

	/* should we include some ring tracking? */

	/* we may need several MMAP to deposit packet copies*/
	int num_bufs;
	
	int buf_refcnt[MAX_BUFS];
	char *kbufs[MAX_BUFS];
	char __user *ubufs[MAX_BUFS];
};


#else	/* __KERNEL__ */

#include <string.h>
#include <stdint.h>
#include <linux/types.h>

#define __user
#define IFNAMSIZ 16
#define ETH_ALEN 6

#define ALIGN(x,a)              __ALIGN_MASK(x,(typeof(x))(a)-1)
#define __ALIGN_MASK(x,mask)    (((x)+(mask))&~(mask))

static inline __sum16 ip_fast_csum(const void *iph, unsigned int ihl)
{
	unsigned int sum;

	asm("  movl (%1), %0\n"
	    "  subl $4, %2\n"
	    "  jbe 2f\n"
	    "  addl 4(%1), %0\n"
	    "  adcl 8(%1), %0\n"
	    "  adcl 12(%1), %0\n"
	    "1: adcl 16(%1), %0\n"
	    "  lea 4(%1), %1\n"
	    "  decl %2\n"
	    "  jne      1b\n"
	    "  adcl $0, %0\n"
	    "  movl %0, %2\n"
	    "  shrl $16, %0\n"
	    "  addw %w2, %w0\n"
	    "  adcl $0, %0\n"
	    "  notl %0\n"
	    "2:"
	    /* Since the input registers which are loaded with iph and ih
	       are modified, we must also specify them as outputs, or gcc
	       will assume they contain their original values. */
	    : "=r" (sum), "=r" (iph), "=r" (ihl)
	    : "1" (iph), "2" (ihl)
	       : "memory");
	return (__sum16)sum;
}

#endif	/* __KERNEL__ */

#define MAX_PACKET_SIZE	2048
#define MAX_CHUNK_SIZE	4096

#define SF_CHECKSUM_RX_UNKNOWN 	0
#define SF_CHECKSUM_RX_GOOD	1
#define SF_CHECKSUM_RX_BAD 	2

struct sf_device {
	char name[IFNAMSIZ];
	char dev_addr[ETH_ALEN];
	uint32_t ip_addr;	/* network order */

	/* NOTE: this is different from kernel's internal index */
	int ifindex;

	int num_rx_queues;
	int num_tx_queues;
};

struct sf_queue {
	int ifindex;
};


struct sf_pkt_info {
	uint32_t offset;
	uint16_t len;
	uint8_t checksum_rx;
};

struct sf_chunk {
	/* number of packets to send/recv */
	int cnt;
	int recv_blocking;

	/* 
	   for RX: output (where did these packets come from?)
	   for TX: input (which interface do you want to xmit?)
	 */
	struct sf_queue queue;

	struct sf_pkt_info __user *info;
	char __user *buf;
};

struct sf_packet {
	int ifindex;
	int len;
	char __user *buf;
};

static inline void prefetcht0(void *p)
{
	asm volatile("prefetcht0 (%0)\n\t"
			: 
			: "r" (p)
		    );
}

static inline void prefetchnta(void *p)
{
	asm volatile("prefetchnta (%0)\n\t"
			: 
			: "r" (p)
		    );
}

static inline void memcpy_aligned(void *to, const void *from, size_t len)
{
	if (len <= 64) {
		memcpy(to, from, 64);
	} else if (len <= 128) {
		memcpy(to, from, 64);
		memcpy((uint8_t *)to + 64, (uint8_t *)from + 64, 64);
	} else {
		size_t offset;

		for (offset = 0; offset < len; offset += 64)
			memcpy((uint8_t *)to + offset, 
					(uint8_t *)from + offset, 
					64);
	}
}

#define SF_IOC_LIST_DEVICES 	0
#define SF_IOC_ATTACH_RX_DEVICE	1
#define SF_IOC_DETACH_RX_DEVICE	2
#define SF_IOC_RECV_CHUNK	3
#define SF_IOC_SEND_CHUNK	4
#define SF_IOC_SLOWPATH_PACKET	5

#ifndef __KERNEL__

struct sf_handle {
	int fd;

	uint64_t rx_chunks[MAX_DEVICES];
	uint64_t rx_packets[MAX_DEVICES];
	uint64_t rx_bytes[MAX_DEVICES];

	uint64_t tx_chunks[MAX_DEVICES];
	uint64_t tx_packets[MAX_DEVICES];
	uint64_t tx_bytes[MAX_DEVICES];

	void *priv;
};

int sf_list_devices(struct sf_device *devices);
int sf_init_handle(struct sf_handle *handle);
void sf_close_handle(struct sf_handle *handle);
int sf_attach_rx_device(struct sf_handle *handle, struct sf_queue *queue);
int sf_detach_rx_device(struct sf_handle *handle, struct sf_queue *queue);
int sf_alloc_chunk(struct sf_handle *handle, struct sf_chunk *chunk);
void sf_free_chunk(struct sf_chunk *chunk);
int sf_recv_chunk(struct sf_handle *handle, struct sf_chunk *chunk);
int sf_send_chunk(struct sf_handle *handle, struct sf_chunk *chunk);
int sf_slowpath_packet(struct sf_handle *handle, struct sf_packet *packet);

void dump_packet(char *buf, int len);
void dump_chunk(struct sf_chunk *chunk);

int get_num_cpus();
int bind_cpu(int cpu);
uint64_t rdtsc();

#endif

#endif	/* _sf_H_ */


