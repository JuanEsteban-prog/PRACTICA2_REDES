#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include <poll.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>

#include "rlib.h"

#ifndef MAX_PAYLOAD
#define MAX_PAYLOAD 500
#endif

#define MAX_WINDOW_BUFFER 256

typedef struct {
    char data[MAX_PAYLOAD];
    int size;
} packet_buffer_item;

static packet_buffer_item tx_buffer[MAX_WINDOW_BUFFER];

static uint32_t send_base;
static uint32_t next_seqno;
static int window_size_limit;

static uint32_t expected_seqno;

static long timeout_val;

void connection_initialization(int window_size, long timeout_in_ns)
{
    if (window_size > MAX_WINDOW_BUFFER) {
        window_size = MAX_WINDOW_BUFFER;
    }
    window_size_limit = window_size;
    timeout_val = timeout_in_ns;

    send_base = 1;
    next_seqno = 1;
    expected_seqno = 1;

    memset(tx_buffer, 0, sizeof(tx_buffer));
}

void receive_callback(packet_t *pkt, size_t pkt_size)
{
    if (VALIDATE_CHECKSUM(pkt) == 0) {
        return;
    }

    if (IS_ACK_PACKET(pkt)) {
        
        uint32_t ack_val = ntohl(pkt->ackno);

        if (ack_val > send_base) {
            
            send_base = ack_val;

            if (send_base == next_seqno) {
                SET_TIMER(0, -1);
            } else {
                SET_TIMER(0, timeout_val);
            }

            RESUME_TRANSMISSION();
        }
    }
    else {
        uint32_t seq_val = ntohl(pkt->seqno);

        if (seq_val == expected_seqno) {
            
            int data_len = ntohs(pkt->len) - DATA_PACKET_HEADER;
            
            if (data_len > 0) {
                if (data_len > MAX_PAYLOAD) data_len = MAX_PAYLOAD;
                ACCEPT_DATA(pkt->data, data_len);
            }
            expected_seqno++;
        }
        
        SEND_ACK_PACKET(expected_seqno);
    }
}

void send_callback()
{
    if (next_seqno >= send_base + window_size_limit) {
        PAUSE_TRANSMISSION();
        return;
    }

    int buffer_idx = next_seqno % MAX_WINDOW_BUFFER;

    int bytes_read = READ_DATA_FROM_APP_LAYER(tx_buffer[buffer_idx].data, MAX_PAYLOAD);

    if (bytes_read > 0) {
        tx_buffer[buffer_idx].size = bytes_read;

        SEND_DATA_PACKET(bytes_read + DATA_PACKET_HEADER, 0, next_seqno, tx_buffer[buffer_idx].data);

        if (send_base == next_seqno) {
            SET_TIMER(0, timeout_val);
        }

        next_seqno++;

        if (next_seqno >= send_base + window_size_limit) {
            PAUSE_TRANSMISSION();
        }
    }
}

void timer_callback(int timer_number)
{
    if (timer_number == 0) {
        
        uint32_t i;
        for (i = send_base; i < next_seqno; i++) {
            
            int buffer_idx = i % MAX_WINDOW_BUFFER;
            
            char *data_ptr = tx_buffer[buffer_idx].data;
            int size = tx_buffer[buffer_idx].size;

            SEND_DATA_PACKET(size + DATA_PACKET_HEADER, 0, i, data_ptr);
        }

        SET_TIMER(0, timeout_val);
    }
}