# CloudOS-Lab2-DPDK
#### Author: 竺天灏 521021910101 influenza-lv3@sjtu.edu.cn       
***

### Part 1: Get familiar with DPDK
>Q1: What's the purpose of using hugepage?

使用大页，在一个页中所能记录的数据更多。也就是说，查找页的次数降低，从而可以使TLB的访问次数降低，
进而减少TLB miss的可能性。总而言之，使用大页可以提高内存的利用率和数据处理的效率。</br></br>

>Q2: Take examples/helloworld as an example, describe the execution flow of DPDK programs?

程序的运行入口在main函数。首先调用```rte_eal_init```函数，创建一个Environment Abstraction Layer。如果成功创建，则让每一个运行的core都运行```lcore_hello```
函数，进行输出。在等待完所有worker core执行完毕之后，主线程将会对环境进行清理，并结束程序。所以主要的流程为：初始化环境、为工作核发送任务、主线程任务、清理环境、结束。</br></br>

>Q3: Read the codes of examples/skeleton, describe DPDK APIs related to sending and receiving packets.

```rte_pktmbuf_pool_create```：创建数据包的内存缓冲池。
```
rte_pktmbuf_pool_create(const char *name,
                        unsigned int n,
                        unsigned int cache_size,
                        uint16_t priv_size,
                        uint16_t data_room_size,
                        int socket_id)
```
1. 在socket_id对应的CPU上创建内存池
2. n表示内存值中可以设置mbuf的个数
3. cache_size指缓存大小，有大小限制
4. data_room_size为mbuf数据区大小</br></br>

```rte_eth_rx_burst```：用于从一个以太网接口的接收队列中读取数据包
```
uint16_t rte_eth_rx_burst(uint16_t port_id, uint16_t queue_id,
                          struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);
```

