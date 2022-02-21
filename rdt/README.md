## Lab1-RDT

袁航宇 518030910096

allen_yuan@sjtu.edu.cn

#### Sender

Sender使用滑动窗口，在窗口满时使用buffer接收上层传来的message存住。

参数设置如下：

```c++
static const double TIMEOUT = 0.3; // second

static std::vector<message> message_buffer;
static const int BUFFER_SIZE = 10000;

static std::vector<packet> packet_window;
static const int WINDOW_SIZE = 10;
```

传消息时从buffer中取出消息，根据消息内容与记录的cursor生成当前要发送的packet，将其存入packet_window，最后发出。

```c++
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
```

checksum使用了Internet Checksum。

*Internet Checksum的算法描述&case: https://www.alpharithms.com/internet-checksum-calculation-steps-044921/*

```c++
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
```

收到ack时，更新packet_num（滑动窗口中packet数量），滑动窗口得以右移。

```c++
void Sender_FromLowerLayer(struct packet *pkt) {
    short checksum = 0;
    memcpy(&checksum, pkt->data, sizeof(checksum));
    if (checksum != InternetChecksum(pkt)) {
        return;
    }

    int ack = 0;
    memcpy(&ack, pkt->data + sizeof(checksum), sizeof(ack));

    if (ack > ack_last_receive && ack <= packet_seq) {
        Sender_StartTimer(TIMEOUT);
        packet_num -= (ack - ack_last_receive);
        ack_last_receive = ack;
        FillWindow();
    }

    if (ack == packet_seq) {
        Sender_StopTimer();
    }
}
```

超时重发

```c++
void Sender_Timeout() {
    Sender_StartTimer(TIMEOUT);
    packet_next_send_seq = ack_last_receive;
    FillWindow();
}
```

##### 包的结构

| checksum | packet seq | payload size | payload  |
| -------- | ---------- | ------------ | -------- |
| 2 byte   | 4 byte     | 1 byte       | the rest |

每个message的第一个包payload的前4个byte用来传message的大小。

这是通过从upper layer收到message时就把信息填入data再放入buffer做到的。

```c++
    // msg->size: int
    message_buffer[buffer_index].size = msg->size + sizeof(msg->size);
    message_buffer[buffer_index].data =
        (char *)malloc(message_buffer[buffer_index].size);
    memcpy(message_buffer[buffer_index].data, &(msg->size), sizeof(msg->size));
    memcpy(message_buffer[buffer_index].data + sizeof(msg->size), msg->data,
           msg->size);
```

#### Receiver

万事不决先算checksum。

```c++
    // 检查checksum
    short checksum = 0;
    memcpy(&checksum, pkt->data, sizeof(checksum));
    if (checksum != InternetChecksum(pkt)) {
        return;
    }
```

Receiver方也有一个window，缓存一部分packet，这样当窗口可以滑动时后面如果有缓存好的packet就直接取出来用了。

```c++
            // window后面接着还有pkt，继续处理
            if (window_valid[next_ack % WINDOW_SIZE]) {
                pkt = &packet_window[next_ack % WINDOW_SIZE];
                memcpy(&packet_seq, pkt->data + sizeof(short),
                       sizeof(packet_seq));
                window_valid[next_ack % WINDOW_SIZE] = false;
            }
```

| checksum | ack    | other    |
| -------- | ------ | -------- |
| 2 byte   | 4 byte | the rest |

#### 测试结果

```shell
yhy@DESKTOP-38JNQHU:~/study/2022Spring/Cloud Operating System Design and Practice/lab/rdt$ ./rdt_sim 1000 0.1 100 0.15 0.15 0.15 0
## Reliable data transfer simulation with:
        simulation time is 1000.000 seconds
        average message arrival interval is 0.100 seconds
        average message size is 100 bytes
        average out-of-order delivery rate is 15.00%
        average loss rate is 15.00%
        average corrupt rate is 15.00%
        tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1044.97s: sender finalizing ...
At 1044.97s: receiver finalizing ...

## Simulation completed at time 1044.97s with
        987004 characters sent
        987004 characters delivered
        50729 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

```shell
yhy@DESKTOP-38JNQHU:~/study/2022Spring/Cloud Operating System Design and Practice/lab/rdt$ ./rdt_sim 1000 0.1 100 0.3 0.3 0.3 0
## Reliable data transfer simulation with:
        simulation time is 1000.000 seconds
        average message arrival interval is 0.100 seconds
        average message size is 100 bytes
        average out-of-order delivery rate is 30.00%
        average loss rate is 30.00%
        average corrupt rate is 30.00%
        tracing level is 0
Please review these inputs and press <enter> to proceed.

At 0.00s: sender initializing ...
At 0.00s: receiver initializing ...
At 1841.21s: sender finalizing ...
At 1841.21s: receiver finalizing ...

## Simulation completed at time 1841.21s with
        994121 characters sent
        994121 characters delivered
        61330 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.
```

