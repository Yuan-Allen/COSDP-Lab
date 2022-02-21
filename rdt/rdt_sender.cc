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

static const double TIMEOUT = 0.3; // second
static const int HEADER_SIZE =
    sizeof(short) + sizeof(int) + sizeof(char); // 7 byte
static const char PAYLOAD_SIZE =
    RDT_PKTSIZE - HEADER_SIZE; // 注意类型是char，只需1byte

static std::vector<message> message_buffer;
static const int BUFFER_SIZE = 10000;

static std::vector<packet> packet_window;
static const int WINDOW_SIZE = 10;

static int packet_num;           // window里的packet数目
static int packet_seq;           // 下一个进入window的packet编号
static int packet_next_send_seq; //下一个要send的packet编号
static int message_next;         // 下一个从upper layer收到的消息编号
static int message_seq;          // 当前处理的消息编号
int message_cursor; // message里data的cursor（发送到第几byte了）

static short InternetChecksum(packet *pkt) {
    unsigned long checksum = 0;                  // unsigned使用逻辑右移
    for (int i = 2; i < RDT_PKTSIZE; i += 2) {   // 跳过前两个byte
        checksum += *(short *)(&(pkt->data[i])); // short为16bit
    }
    while (checksum >> 16) {
        // wrap-around carry bit
        checksum = (checksum >> 16) + (checksum & 0xffff);
    }
    return ~checksum;
}

// 根据message和cursor填pkt
void FillPacket(packet *pkt, message msg) {
    char size = 0; // char for 1 byte
    if (msg.size > message_cursor + PAYLOAD_SIZE) {
        // msg剩余size很大，可以装满一个packet并且还有剩余
        size = PAYLOAD_SIZE;
    } else if (msg.size > message_cursor) {
        // msg剩余size可以用一个packet装完
        size = msg.size - message_cursor;
    } else {
        ASSERT(false);
    }

    memcpy(pkt->data + sizeof(short), &packet_seq,
           sizeof(packet_seq)); // packet编号
    memcpy(pkt->data + sizeof(short) + sizeof(packet_seq), &size,
           sizeof(size)); // payload size
    memcpy(pkt->data + sizeof(short) + sizeof(packet_seq) + sizeof(size),
           msg.data + message_cursor, size);
    short checksum = InternetChecksum(pkt);
    memcpy(pkt->data, &checksum, sizeof(checksum));
}

// 把window里该发的packets都发出去
void SendPackets() {
    packet pkt;
    while (packet_next_send_seq < packet_seq) {
        memcpy(&pkt, &(packet_window[packet_next_send_seq % WINDOW_SIZE]),
               sizeof(packet));
        Sender_ToLowerLayer(&pkt);
        packet_next_send_seq++;
    }
}

// 把message从buffer中取出，装满window并发包
void FillWindow() {
    message msg = message_buffer.at(message_seq % BUFFER_SIZE);
    packet pkt;
    while (packet_num < WINDOW_SIZE && message_seq < message_next) {
        FillPacket(&pkt, msg);
        memcpy(&(packet_window[packet_seq % WINDOW_SIZE]), &pkt, sizeof(pkt));
        if (msg.size > message_cursor + PAYLOAD_SIZE) {
            message_cursor += PAYLOAD_SIZE;
        } else if (msg.size > message_cursor) {
            message_seq++;
            if (message_seq < message_next) {
                // 取下一个msg，为后续可能的循环做准备
                msg = message_buffer.at(message_seq % BUFFER_SIZE);
            }
            message_cursor = 0;
        } else {
            ASSERT(false);
        }
        packet_seq++;
        packet_num++;
    }

    SendPackets();
}

/* sender initialization, called once at the very beginning */
void Sender_Init() {
    fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    message_buffer.assign(BUFFER_SIZE, message{.size = 0, .data = nullptr});
    packet_window.assign(WINDOW_SIZE, packet());
    packet_num = 0;
    packet_seq = 0;
    packet_next_send_seq = 0;
    message_next = 0;
    message_seq = 0;
    message_cursor = 0;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some
   memory you allocated in Sender_init(). */
void Sender_Final() {
    fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
    for (message &msg : message_buffer) {
        if (msg.size != 0) {
            delete[] msg.data;
            msg.data = nullptr;
            msg.size = 0;
        }
    }
}

/* event handler, called when a message is passed from the upper layer at the
   sender */
void Sender_FromUpperLayer(struct message *msg) {
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

    if (Sender_isTimerSet()) {
        return;
    }

    Sender_StartTimer(TIMEOUT);
    FillWindow();
}

/* event handler, called when a packet is passed from the lower layer at the
   sender */
void Sender_FromLowerLayer(struct packet *pkt) {
    if (packet_num > 0) {
        packet_num--;
        FillWindow();
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout() {}
