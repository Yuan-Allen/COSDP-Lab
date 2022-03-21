# Lab 3

袁航宇 518030910096

allen_yuan@sjtu.edu.cn

## Part1：创建网络拓扑

### Task1 请在你自己的环境中完成上面的连通性测试，并以截图的形式分别记录Node:h1和Node:h2中 iperf的输出结果。

整体如下：

![task1](Task1\task1.png)

虚拟机截图不是很清晰，带系统时间的细节截图如下：

![task1_h1](Task1\task1_h1.png)

![task1_h2](Task1\task1_h2.png)

## Part2：三种限速方式

### Task2.1 请截图记录输出结果，截图要求同Task1，并着重关注其中的带宽、抖动、丢包率等数据。

查看h1和h2对应的网卡如下：

```shell
s1-eth1: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet6 fe80::449a:a4ff:fe15:2114  prefixlen 64  scopeid 0x20<link>
        ether 46:9a:a4:15:21:14  txqueuelen 1000  (Ethernet)
        RX packets 514118  bytes 33932080 (33.9 MB)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 732862  bytes 32126152108 (32.1 GB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0

s1-eth2: flags=4163<UP,BROADCAST,RUNNING,MULTICAST>  mtu 1500
        inet6 fe80::50f5:e0ff:feb8:3d4f  prefixlen 64  scopeid 0x20<link>
        ether 52:f5:e0:b8:3d:4f  txqueuelen 1000  (Ethernet)
        RX packets 732767  bytes 32126143810 (32.1 GB)
        RX errors 0  dropped 0  overruns 0  frame 0
        TX packets 514204  bytes 33939780 (33.9 MB)
        TX errors 0  dropped 0 overruns 0  carrier 0  collisions 0
```

最后输出截图如下：

h2:

![task2_1_h2](Task2\task2_1_h2.png)

h1:

![task2_1_h1](Task2\task2_1_h1.png)

可以看见，带宽为5.71Mbits/sec，得到了有效的限速。抖动为0.011ms。由于设置的丢包机制，丢包率较高，为46%。

### Task2.2 同上，此处也需要截图记录实验结果。

查看队列信息，得到：

```
root@ubuntu:/home/os# ovs-vsctl list qos
_uuid               : 969e31ad-92ef-42aa-aee3-dc4318ddb71f
external_ids        : {}
other_config        : {}
queues              : {0=f599d2be-4d8b-4d55-819a-446cb88d2f82}
type                : linux-htb
root@ubuntu:/home/os# ovs-vsctl list queue
_uuid               : f599d2be-4d8b-4d55-819a-446cb88d2f82
dscp                : []
external_ids        : {}
other_config        : {max-rate="5000000"}
```

根据文档操作，得到h3：

![task2_2_h3](Task2\task2_2_h3.png)

h4:

![task2_2_h4_1](Task2\task2_2_h4_1.png)

![task2_2_h4_2](Task2\task2_2_h4_2.png)

可得到带宽为4.86Mbits/sec，抖动为58.153ms，丢包率为0%。

### Question 1 尝试理解Line15,16两条指令，指出每条指令的具体工作是什么，并逐个分析其中各个参数的具体含义。

```
ovs-ofctl add-flow s1 in_port=5,action=meter:1,output:6 -O openflow13
```

下发转发的流表。匹配进端口为5，转发动作为meter:1,output:6。meter:1表示匹配到的流表首先交给meter表处理，就是超过5M的数据包丢弃掉，然后再交给output:6，从6端口转发出去。-O参数后面跟协议，s1表示交换机的id。



```
ovs-ofctl dump-flows s1 -O openflow13
```

查看交换机中的流表项，-O参数后面跟协议，s1表示交换机的id。

### Task2.3 同上，请将此处的实验结果按要求截图。

h5:

![task2_3_h5_1](Task2\task2_3_h5_1.png)

![task2_3_h5_2](Task2\task2_3_h5_2.png)

h6:

![task2_3_h6_1](Task2\task2_3_h6_1.png)

![task2_3_h6_2](Task2\task2_3_h6_2.png)

可得到带宽为5.22Mbits/sec，抖动为15.707ms，丢包率为49%。

### Question 2 到这里，你已经完成了三种限速方式的实验，并获得了三组测试数据，请你就三组数据中的带宽、抖动和丢包率等参数，对三种限速方式进行横向比较，并适当地分析原因。

整理以上实验数据得到：

|             | 带宽          | 抖动     | 丢包率 |
| ----------- | ------------- | -------- | ------ |
| 网卡限速    | 5.71Mbits/sec | 0.011ms  | 46%    |
| 队列限速    | 4.86Mbits/sec | 58.153ms | 0%     |
| Meter表限速 | 5.22Mbits/sec | 15.707ms | 49%    |

可看出队列限速效果较好，因为其带宽最接近设定的5Mbits/sec，丢包率也最低，但是抖动较高。

网卡限速的带宽误差率较高，这与网卡限速的实现方式有关，其控制精度比较粗。

Meter表限速的表现中规中矩，这可能和ovs交换的流表控制能力有关，因为是软件实现的交换机，不如硬件交换机。交换机中流表的匹配，数据流计数，动作的执行等都是影响其控制力度的原因。

## Part3：拓展与应用

### Task3 在限制Server端（h1）的带宽为10Mb的前提下，观察稳定后的三个Client的带宽，将结果截图并简单分析。

使用队列限速将h1的带宽限制为10M：

```
ovs-vsctl set port s1-eth1 qos=@newqos -- \
--id=@newqos create qos type=linux-htb queues=0=@q0 -- \
--id=@q0 create queue other-config:max-rate=10000000

```

得到结果如下：

h2：

![task3_h2](Task3\task3_h2.png)



h3：

![task3_h3](Task3\task3_h3.png)



h4：

![task3_h4](Task3\task3_h4.png)

h1：

![task3_h1](Task3\task3_h1.png)

可以看到，h2，h3，h4一开始带宽是10Mbits/sec，后来由于限速带宽开始立即下降，最终总和为10Mbits/sec左右，且比例为4:3:3，较为均匀。

### Task4 你可以通过上述三种限速的方法来达成目标，请记录你的设计过程（思路及运行指令），并将你稳定后的三个Client的带宽结果截图。

思路：使用多队列限速，针对h2，h3，h4使用不同的队列。

指令如下：

```
ovs-vsctl set port s1-eth1 qos=@newqos -- \
--id=@newqos create qos type=linux-htb other-config:max-rate=10000000 other-config:min-rate=10000000 queues=0=@q0,1=@q1,2=@q2 -- \
--id=@q0 create queue other-config:min-rate=5000000 -- \
--id=@q1 create queue other-config:min-rate=3000000 -- \
--id=@q2 create queue other-config:max-rate=2000000
```

```
ovs-ofctl -O OpenFlow13 add-flow s1 in_port="s1-eth2",actions=set_queue:0,output:"s1-eth1"

ovs-ofctl -O OpenFlow13 add-flow s1 in_port="s1-eth3",actions=set_queue:1,output:"s1-eth1"

ovs-ofctl -O OpenFlow13 add-flow s1 in_port="s1-eth4",actions=set_queue:2,output:"s1-eth1"
```

可以看到，对于h1设置了带宽max-rate和min-rate都为10000000，对于h2和h3，分别设置了min-rate=5000000和3000000，对于h4则设置了max-rate=2000000。

得到结果如下：

h2：

![task4_h2](Task4\task4_h2.png)

h3：

![task4_h3](Task4\task4_h3.png)

h4：

![task4_h4](Task4\task4_h4.png)

h1：

![task4_h1](Task4\task4_h1.png)

可以得到带宽如下：

| h2            | h3            | h4            |
| ------------- | ------------- | ------------- |
| 6.09Mbits/sec | 3.68Mbits/sec | 93.5Kbits/sec |

可以看到，h2与h3的带宽均满足要求，而h4的带宽仅为93.5Kbits/sec，与h2和h3已经占用了大多数带宽有关。总体还算符合要求。

