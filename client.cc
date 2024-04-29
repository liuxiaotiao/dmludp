#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <sys/epoll.h>
#include "dmludp.h"
#include "connection.h"
#include "RangeBuf.h"
#include "Recovery.h"
#include "recv_buf.h"
#include "send_buf.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>  // 包含 uint8_t 的定义
#include <arpa/inet.h>
#define PORT 12356
#define MAX_EVENTS 10
#define SERVER_IP "10.10.1.1"

int main() {
    int client_fd, epoll_fd;

    client_fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (client_fd == -1) {
        perror("socket creation failed");
        return -1;
    }

    struct epoll_event ev, events[MAX_EVENTS];


    struct sockaddr_storage localAddr;
    struct sockaddr_storage peerAddr;
    memset(&localAddr, 0, sizeof(localAddr));
    memset(&peerAddr, 0, sizeof(peerAddr));

    // 假设本地和对端都使用 IPv4
    sockaddr_in *local = reinterpret_cast<sockaddr_in*>(&localAddr);
    sockaddr_in *peer = reinterpret_cast<sockaddr_in*>(&peerAddr);

    // 设置协议族
    local->sin_family = AF_INET;
    peer->sin_family = AF_INET;

    // 设置端口号
    local->sin_port = htons(PORT);  
    peer->sin_port = htons(PORT);   

    // 设置 IP 地址
    inet_pton(AF_INET, "10.10.1.2", &(local->sin_addr));  
    inet_pton(AF_INET, "10.10.1.1", &(peer->sin_addr));   

    if (bind(client_fd, (const struct sockaddr *)local, sizeof(*local)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(client_fd);
        return -1;
    }

    if (connect(client_fd, (struct sockaddr *)peer, sizeof(*peer)) < 0) {
        std::cerr << "Failed to connect" << std::endl;
        close(client_fd);
        return -1;
    }

    bool is_server = false;

    uint8_t out[1500];
    uint8_t buffer[1500];
    auto dmludp_config = dmludp_config_new();
    auto dmludp_connection = dmludp_connect(localAddr, peerAddr, *dmludp_config);
    dmludp_config_free(dmludp_config);


    if (is_server){
 
    }else{
        auto start = std::chrono::high_resolution_clock::now();
        ssize_t written = dmludp_send_data_handshake(dmludp_connection, out, sizeof(out));
        ssize_t sent = send(client_fd, out, written, 0);
        while(true){
            ssize_t received = recv(client_fd, buffer, sizeof(buffer), 0);
            if(received < 1){
                continue;
            }
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

            int type = 0;
            int pktnum = 0;
            auto header = dmludp_header_info(buffer, 26, type, pktnum);
	    std::cout<<(int)header<<std::endl;
            if(header != 2){
                continue;
            }
            dmludp_set_rtt(dmludp_connection, duration.count());
            ssize_t dmludp_recv = dmludp_conn_recv(dmludp_connection, buffer, received);
            auto written = dmludp_conn_send(dmludp_connection, out, sizeof(out));
            auto send_bytes = send(client_fd, out, written, 0);
            break;
        }
    }

  
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(client_fd);
        return -1;
    }
    ev.events = EPOLLIN;
    ev.data.fd = client_fd;

    // 添加套接字到 epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
        perror("epoll_ctl: client_fd");
        close(client_fd);
        return -1;
    }
    dmludp_conn_recv_reset(dmludp_connection);
    dmludp_conn_rx_len(dmludp_connection, 1131413504);
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == client_fd) {
                if (events[n].events & EPOLLIN){
                    struct mmsghdr msgs[100];
                    struct iovec iovecs[100];
                    uint8_t bufs[100][1500];

                    for (int i = 0; i < 100; i++) {
                        iovecs[i].iov_base = bufs[i];
                        iovecs[i].iov_len = sizeof(bufs[i]);
                        msgs[i].msg_hdr.msg_iov = &iovecs[i];
                        msgs[i].msg_hdr.msg_iovlen = 1;
                        msgs[i].msg_hdr.msg_name = NULL;
                        msgs[i].msg_hdr.msg_namelen = 0;
                    }

                    while(true){
                        auto retval = recvmmsg(client_fd, msgs, 100, 0, NULL);
                        if (retval == -1){
                            if (errno == EAGAIN) {
                                break;
                            }
                            if (errno == EINTR){
                                continue;
                            }
                        }
                        for (auto index = 0; index < retval; index++){
                            auto read = msgs[index].msg_len;
                            if (read > 0){
                                auto dmludpread = dmludp_conn_recv(dmludp_connection, static_cast<uint8_t *>(msgs[index].msg_hdr.msg_iov->iov_base), read);
                                int offset;
                                int pkt_num;
                                auto rv = dmludp_header_info(static_cast<uint8_t *>(msgs[index].msg_hdr.msg_iov->iov_base), 26, offset, pkt_num);
                                // Elicit ack
                                if(rv == 4){
                                    uint8_t out[1500];
                                    ssize_t dmludpwrite = dmludp_conn_send(dmludp_connection, out, sizeof(out));
                                    ssize_t socketwrite = ::send(client_fd, out, dmludpwrite, 0);
				    if(dmludp_conn_receive_complete(dmludp_connection)){
					    return 0;
				    }
                                }
                                else if (rv == 6){
                                    // Packet completes tranmission and start to iov.
                                    uint8_t out[1500];
                                    auto stopsize = dmludp_send_data_stop(dmludp_connection, out, sizeof(out));
                                    ssize_t socket_write = ::send(client_fd, out, stopsize, 0);
                                    auto ispadding = true;
                                    break;
                                }
                                else if (rv == 3){
                                // Application packet 
                                }
                                else if (rv == 5){
                                }
                            }
                        }
                    }
                }
                
                
            }
        }
    }

    close(client_fd);
    return 0;
}
