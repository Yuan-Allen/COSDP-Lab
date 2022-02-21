/*
 * FILE: rdt_receiver.cc
 * DESCRIPTION: Reliable data transfer receiver.
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

#include "rdt_receiver.h"
#include "rdt_struct.h"

static const int HEADER_SIZE =
    sizeof(short) + sizeof(int) + sizeof(char); // 7 byte

static std::vector<packet> packet_window;
static const int WINDOW_SIZE = 10;

// bitmap，记录当前window各个位置是否有packet
static std::vector<bool> window_valid;

static message *msg = nullptr;
static int msg_cursor; // 该填msg的第几个byte了

static int next_ack; //当前window的下界

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

void SendAck(int ack) {
    packet ack_pkt;
    memcpy(ack_pkt.data + sizeof(short), &ack, sizeof(ack));
    short checksum = InternetChecksum(&ack_pkt);
    memcpy(ack_pkt.data, &checksum, sizeof(checksum));
    Receiver_ToLowerLayer(&ack_pkt);
}

/* receiver initialization, called once at the very beginning */
void Receiver_Init() {
    fprintf(stdout, "At %.2fs: receiver initializing ...\n",
            GetSimulationTime());
    packet_window.assign(WINDOW_SIZE, packet());
    window_valid.assign(WINDOW_SIZE, false);
    msg = new message;
    msg->size = 0;
    msg->data = nullptr;
    msg_cursor = 0;
    next_ack = 0;
}

/* receiver finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to use this opportunity to release some
   memory you allocated in Receiver_init(). */
void Receiver_Final() {
    fprintf(stdout, "At %.2fs: receiver finalizing ...\n", GetSimulationTime());
}

/* event handler, called when a packet is passed from the lower layer at the
   receiver */
void Receiver_FromLowerLayer(struct packet *pkt) {
    // 检查checksum
    short checksum = 0;
    memcpy(&checksum, pkt->data, sizeof(checksum));
    if (checksum != InternetChecksum(pkt)) {
        return;
    }

    int packet_seq = 0;
    memcpy(&packet_seq, pkt->data + sizeof(short), sizeof(packet_seq));
    // fprintf(stdout, "packet seq1: %d\n", packet_seq);
    // fprintf(stdout, "next ack: %d\n", next_ack);
    if (packet_seq > next_ack && packet_seq < next_ack + WINDOW_SIZE) {
        // packet_seq在window范围内且不是window的下界，先存起来，但窗口不动
        if (!window_valid[packet_seq % WINDOW_SIZE]) {
            window_valid[packet_seq % WINDOW_SIZE] = true;
            memcpy(&(packet_window[packet_seq % WINDOW_SIZE]), pkt->data,
                   RDT_PKTSIZE);
        }
        SendAck(next_ack);
        return;
    } else if (packet_seq != next_ack) {
        // packet不在window范围内，不接收
        SendAck(next_ack);
        return;
    } else {
        ASSERT(packet_seq == next_ack);
        // fprintf(stdout, "packet seq2: %d\n", packet_seq);
        // 说明正好是window的下界，接收并且窗口可以移动了
        while (true) {
            ++next_ack;
            int packet_payload_size =
                static_cast<int>(pkt->data[HEADER_SIZE - 1]);
            // 组装msg
            if (msg->size == 0) {
                // 这是该msg收到的第一个包，payload前4个byte是msg size
                ASSERT(msg_cursor == 0);
                memcpy(&(msg->size), pkt->data + HEADER_SIZE,
                       sizeof(msg->size));
                msg->data = (char *)malloc(msg->size);
                memcpy(msg->data, pkt->data + HEADER_SIZE + sizeof(msg->size),
                       packet_payload_size - sizeof(msg->size));
                msg_cursor += packet_payload_size - sizeof(msg->size);
            } else {
                memcpy(msg->data + msg_cursor, pkt->data + HEADER_SIZE,
                       packet_payload_size);
                msg_cursor += packet_payload_size;
            }

            if (msg_cursor == msg->size) {
                Receiver_ToUpperLayer(msg);
                msg->size = 0;
                delete[] msg->data;
                msg_cursor = 0;
            }
            ASSERT(msg_cursor <= msg->size);
            // window后面接着还有pkt，继续处理
            if (window_valid[next_ack % WINDOW_SIZE]) {
                pkt = &packet_window[next_ack % WINDOW_SIZE];
                memcpy(&packet_seq, pkt->data + sizeof(short),
                       sizeof(packet_seq));
                window_valid[next_ack % WINDOW_SIZE] = false;
            } else {
                break;
            }
        }
        SendAck(next_ack);
    }
}
