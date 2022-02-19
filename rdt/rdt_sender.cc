/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or
 *       reordering.  You will need to enhance it to deal with all these
 *       situations.  In this implementation, the packet format is laid out as
 *       the following:
 *
 *       |<-  1 byte  ->|<-             the rest            ->|
 *       | payload size |<-             payload             ->|
 *
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "rdt_sender.h"
#include "rdt_struct.h"

const double TIMEOUT = 0.3; // second
const int HEADER_SIZE = 7;  // byte

std::vector<message> message_buffer;
const int BUFFER_SIZE = 10000;

std::vector<packet *> packet_window;
const int WINDOW_SIZE = 10;

int message_next; // 下一个从upper layer收到的包编号
int message_seq;  // 确认receiver已收到的的包编号
int message_num;  // 在buffer里还没发出去的message数量

short InternetChecksum(packet *pkt) {
    unsigned long checksum = 0;                  // unsigned使用逻辑右移
    for (int i = 2; i < RDT_PKTSIZE; ++i) {      // 跳过前两个byte
        checksum += *(short *)(&(pkt->data[i])); // short为16bit
    }
    while (checksum >> 16) {
        // wrap-around carry bit
        checksum = (checksum >> 16) + (checksum & 0xffff);
    }
    return ~checksum;
}

// 把message从buffer中取出，发包装满window
void FillWindow() {}

/* sender initialization, called once at the very beginning */
void Sender_Init() {
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    message_buffer.assign(BUFFER_SIZE, message{.size = 0, .data = nullptr});
    packet_window.assign(WINDOW_SIZE, nullptr);
    message_next = 0;
    message_seq = 0;
    message_num = 0;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final() {
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg) {
    // /* maximum payload size */
    // int maxpayload_size = RDT_PKTSIZE - header_size;

    /* put message in buffer first */
    int buffer_index = message_next % BUFFER_SIZE;
    if (message_buffer[buffer_index].size != 0) {
        delete[] message_buffer[buffer_index].data;
        message_buffer[buffer_index].size = 0;
    }
    // msg->size: int
    message_buffer[buffer_index].size = msg->size + sizeof(msg->size);
    message_buffer[buffer_index].data =
        (char *)malloc(message_buffer[buffer_index].size);
    memcpy(message_buffer[buffer_index].data, &(msg->size), sizeof(msg->size));
    memcpy(message_buffer[buffer_index].data + sizeof(msg->size), msg->data,
           msg->size);

    message_next++;
    message_num++;

    if (Sender_isTimerSet()) {
        return;
    }

    Sender_StartTimer(TIMEOUT);

    // /* split the message if it is too big */

    // /* reuse the same packet data structure */
    // packet pkt;

    // /* the cursor always points to the first unsent byte in the message */
    // int cursor = 0;

    // while (msg->size - cursor > maxpayload_size) {
    //     /* fill in the packet */
    //     pkt.data[0] = maxpayload_size;
    //     memcpy(pkt.data + header_size, msg->data + cursor, maxpayload_size);

    //     /* send it out through the lower layer */
    //     Sender_ToLowerLayer(&pkt);

    //     /* move the cursor */
    //     cursor += maxpayload_size;
    // }

    // /* send out the last packet */
    // if (msg->size > cursor) {
    //     /* fill in the packet */
    //     pkt.data[0] = msg->size - cursor;
    //     memcpy(pkt.data + header_size, msg->data + cursor, pkt.data[0]);

    //     /* send it out through the lower layer */
    //     Sender_ToLowerLayer(&pkt);
    // }
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {}

/* event handler, called when the timer expires */
void Sender_Timeout() {}
