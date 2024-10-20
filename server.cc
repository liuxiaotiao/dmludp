#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <sys/epoll.h>
#include "dmludp.h"
#include "connection.h"
#include "RangeBuf.h"
#include "cubic.h"
#include "recv_buf.h"
#include "send_buf.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>  // 包含 uint8_t 的定义
#include <arpa/inet.h>
#define PORT 12355
#define MAX_EVENTS 10
#define CLIENT_IP "10.10.1.2"
#define SEND_TIME 2000
#define RECEIVE_BUFFER_LENGTH 3000

int main() {
    // std::ifstream file("randomfile.bin", std::ios::binary | std::ios::ate);
    std::ifstream file("small.bin", std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error opening file" << std::endl;
        return 1;
    }

    // 获取文件大小
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 创建一个足够大的 uint8_t 类型的数组来存储文件内容
    std::vector<uint8_t> data(file_size);

    // 读取整个文件
    if (!file.read(reinterpret_cast<char*>(data.data()), file_size)) {
        std::cerr << "Error reading file" << std::endl;
        return 1;
    }

    file.close();  // 关闭文件

    int server_fd, epoll_fd;

    server_fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (server_fd == -1) {
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
    inet_pton(AF_INET, "10.10.1.1", &(local->sin_addr));
    inet_pton(AF_INET, "10.10.1.2", &(peer->sin_addr));

    if (bind(server_fd, (const struct sockaddr *)local, sizeof(*local)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        close(server_fd);
        return -1;
    }

    if (connect(server_fd, (struct sockaddr *)peer, sizeof(*peer)) < 0) {
        std::cerr << "Failed to connect" << std::endl;
        close(server_fd);
        return -1;
    }

    bool is_server = true;

    uint8_t out[9000];
    uint8_t buffer[9000];

    auto dmludp_config = dmludp_config_new();
    auto dmludp_connection =  dmludp_accept(localAddr, peerAddr, *dmludp_config);
    dmludp_config_free(dmludp_config);


    if (is_server){
        while(true){
            int rv = recv(server_fd, buffer, sizeof(buffer), 0);
            if (rv < 0){
                continue;
            }
            uint32_t off = 0;
            uint64_t pktnum = 0;
            auto header = dmludp_header_info(buffer, 26, off, pktnum);
            if(header != 2){
                continue;
            }
            ssize_t dmludp_recv = dmludp_conn_recv(dmludp_connection, buffer, rv);
            auto written = dmludp_send_data_handshake(dmludp_connection, out, sizeof(out));
            auto send_bytes = send(server_fd, out, written, 0);
            break;
        }

        while(true){
            int rv = recv(server_fd, buffer, sizeof(buffer), 0);
            if (rv < 0){
                continue;
            }
            uint32_t off = 0;
            uint64_t pktnum = 0;
            auto header = dmludp_header_info(buffer, 26, off, pktnum);
            if(header != 2){
                continue;
            }
            ssize_t dmludp_recv = dmludp_conn_recv(dmludp_connection, buffer, rv);
            break;
        }
    }else{
    }


    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        close(server_fd);
        return -1;
    }
    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = server_fd;

    // 添加套接字到 epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        close(server_fd);
        return -1;
    }
    // struct mmsghdr msgs[100];
    // struct iovec iovecs[100];
    // uint8_t bufs[100][9000];

    // for (int i = 0; i < 100; i++) {
    //     iovecs[i].iov_base = bufs[i];
    //     iovecs[i].iov_len = sizeof(bufs[i]);
    //     msgs[i].msg_hdr.msg_iov = &iovecs[i];
    //     msgs[i].msg_hdr.msg_iovlen = 1;
    //     msgs[i].msg_hdr.msg_name = NULL;
    //     msgs[i].msg_hdr.msg_namelen = 0;
    // }

    std::vector<std::shared_ptr<Header>> hdrs;
    std::vector<struct mmsghdr> messages;
    std::vector<struct iovec> iovecs_;
    std::vector<std::vector<uint8_t>> out_ack;

    hdrs.reserve(500);
    messages.reserve(1000);
    iovecs_.reserve(1000);
    out_ack.reserve(10);

    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> inner_vector;
        inner_vector.reserve(44);  // 预分配内部向量
        out_ack.push_back(inner_vector);
    }

    auto start = std::chrono::high_resolution_clock::now();
    bool first_check = true;
    size_t send_time = 1;
    std::vector<struct msghdr> msgs(RECEIVE_BUFFER_LENGTH);
    std::vector<struct iovec> iovecs(RECEIVE_BUFFER_LENGTH);
    uint8_t bufs[RECEIVE_BUFFER_LENGTH][1500];

    for (int i = 0; i < RECEIVE_BUFFER_LENGTH; i++) {
        iovecs[i].iov_base = bufs[i];
        iovecs[i].iov_len = sizeof(bufs[i]);
        msgs[i].msg_iov = &iovecs[i];
        msgs[i].msg_iovlen = 1;
        msgs[i].msg_name = NULL;
        msgs[i].msg_namelen = 0;
    }
    while (true) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, 1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }
        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == server_fd) {
                if (events[n].events & EPOLLIN){
                    // while(true){
                    //     auto retval = recvmmsg(server_fd, msgs, 100, 0, NULL);
                    //     if (retval == -1){
                    //         if (errno == EAGAIN) {
                    //             break;
                    //         }
                    //         if (errno == EINTR){
                    //             continue;
                    //         }
                    //     }
                    //     for (auto index = 0; index < retval; index++){
                    //         auto read = msgs[index].msg_len;
                    //         if (read > 0){
                    //             auto dmludpread = dmludp_conn_recv(dmludp_connection, static_cast<uint8_t *>(msgs[index].msg_hdr.msg_iov->iov_base), read);
                    //             uint32_t offset;
                    //             uint64_t pkt_num;
                    //             auto rv = dmludp_header_info(static_cast<uint8_t *>(msgs[index].msg_hdr.msg_iov->iov_base), 26, offset, pkt_num);
                    //             // Elicit ack
                    //             if(rv == 4){
                    //                 uint8_t out[9000];
                    //                 ssize_t dmludpwrite = dmludp_conn_send(dmludp_connection, out, sizeof(out));
                    //                 ssize_t socketwrite = ::send(server_fd, out, dmludpwrite, 0);
                    //             }
                    //             else if (rv == 6){
                    //                 // Packet completes tranmission and start to iov.
                    //                 uint8_t out[9000];
                    //                 auto stopsize = dmludp_send_data_stop(dmludp_connection, out, sizeof(out));
                    //                 ssize_t socket_write = ::send(server_fd, out, stopsize, 0);
                    //                 break;
                    //             }
                    //             else if (rv == 3){
                    //             // Application packet 
                    //             }
                    //             else if (rv == 5){
                    //                 if(dmludp_transmission_complete(dmludp_connection)){
                    //                     auto end = std::chrono::high_resolution_clock::now();
                    //                     // 计算持续时间
                    //                     auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                    //                     dmludp_conn_clear_sent_once(dmludp_connection);
                    //                     // 输出执行时间
                    //                     std::cout << send_time++ << " Duration: " << duration.count() << " microseconds" << std::endl;
                    //                     if (send_time == SEND_TIME){
                    //                         return 0;
                    //                     } 
                    //                     start = std::chrono::high_resolution_clock::now();
                    //                     std::array<struct iovec, 1> siov;
                    //                     siov[0].iov_base = data.data();
                    //                     siov[0].iov_len = data.size();
                    //                     bool w2dmludp = dmludp_get_data(dmludp_connection, siov.data(), 1);

                    //                     if (!w2dmludp){
                    //                         return false;
                    //                     }
                    //                 }
                    //             }
                    //         }
                    //     }
                    // }
                    int receive_number = 0;
                    while(true){
                        auto retval = recvmsg(server_fd, msgs.data() + receive_number, 0);

                        if (retval == -1){
                            if (errno == EAGAIN) {
                                break;
                            }
                            if (errno == EINTR){
                                continue;
                            }
                        }

                        uint32_t offset;
                        uint64_t pkt_num;
                        auto rv = dmludp_process_header_info(dmludp_connection, static_cast<uint8_t *>(msgs[receive_number].msg_iov->iov_base), 26, offset, pkt_num);
                        if(rv == 4){
                            auto dmludpread = dmludp_conn_recv(dmludp_connection, static_cast<uint8_t *>(msgs[receive_number].msg_iov->iov_base), msgs[receive_number].msg_iov->iov_len);
                            ssize_t dmludpwrite = dmludp_conn_send(dmludp_connection, out, sizeof(out));
                            ssize_t socketwrite = ::send(server_fd, out, dmludpwrite, 0);
                            dmludp_update_receive_parameters(dmludp_connection);
                        }
                        else if (rv == 6){
                            // Packet completes tranmission and start to iov.
                            auto stopsize = dmludp_send_data_stop(dmludp_connection, out, sizeof(out));
                            ssize_t socket_write = ::send(server_fd, out, stopsize, 0);
                            auto ispadding = true;
                            break;
                        }
                        else if (rv == 3){
                            // auto dmludpread = dmludp_conn_recv(dmludp_connection, static_cast<uint8_t *>(msgs[index].msg_hdr.msg_iov->iov_base), read);
                            auto dmludpread = dmludp_conn_recv(dmludp_connection, static_cast<uint8_t *>(msgs[receive_number].msg_iov->iov_base), msgs[receive_number].msg_iov->iov_len);
                        // Application packet 
                        }
                        else if (rv == 5){
                            auto dmludpread = dmludp_conn_recv(dmludp_connection, static_cast<uint8_t *>(msgs[receive_number].msg_iov->iov_base), msgs[receive_number].msg_iov->iov_len);
                            if(dmludp_transmission_complete(dmludp_connection)){
                                auto end = std::chrono::high_resolution_clock::now();
                                // 计算持续时间
                                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                                dmludp_conn_clear_sent_once(dmludp_connection);
                                // 输出执行时间
                                std::cout << send_time++ << " Duration: " << duration.count() << " microseconds" << std::endl;
                                if (send_time == SEND_TIME){
                                    return 0;
                                } 
                                start = std::chrono::high_resolution_clock::now();
                                std::array<struct iovec, 1> siov;
                                siov[0].iov_base = data.data();
                                siov[0].iov_len = data.size();
                                bool w2dmludp = dmludp_get_data(dmludp_connection, siov.data(), 1);

                                if (!w2dmludp){
                                    return false;
                                }
                            }
                            if(dmludp_connection->get_send_status() == 4){
                                break;
                            }
                        }

                        receive_count++;
                    }

                }


                if (events[n].events & EPOLLOUT){
                    std::vector<std::vector<uint8_t>> out;
                    
                    if (dmludp_transmission_complete(dmludp_connection)){
                        if(!first_check){
                            auto end = std::chrono::high_resolution_clock::now();
                            // 计算持续时间
                            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                            dmludp_conn_clear_sent_once(dmludp_connection);
                            // 输出执行时间
                            std::cout << send_time++ << " Duration: " << duration.count() << " microseconds" << std::endl;
                            if (send_time == SEND_TIME){
                                return 0;
                            } 
                        }
                        
			
                        start = std::chrono::high_resolution_clock::now();
                        std::array<struct iovec, 1> siov;
                        siov[0].iov_base = data.data();
                        siov[0].iov_len = data.size();
                        bool w2dmludp = dmludp_get_data(dmludp_connection, siov.data(), 1);
                        first_check = false;
                        if (!w2dmludp){
                            return false;
			            }
                    }

                    auto dmludp_status = dmludp_connection->check_status();
                    if(dmludp_status != 1){
                        continue;
                    }
                    while(true){
                        if(dmludp_connection->get_send_status() != 3){
                            auto dmludp_sent = dmludp_connection->send_packet();
                            if(!dmludp_sent){
                                break;
                            }
                            auto retval = sendmsg(server_fd, dmludp_connection->send_messages.data(), 0);
                            if(retval == -1){
                                if (errno == EINTR){
                                    continue;
                                }

                                if (errno == EAGAIN){
                                    dmludp_connection->send_packet_complete(EAGAIN);
                                }
                                break;
                            }
                            dmludp_connection->send_packet_complete();
                            if(dmludp_connection->get_send_status() == 4 && !dmludp_connection->recovery.cwnd_available()){
                                dmludp_connection->set_send_status(3);
                            }
                        }
                        else{
                            auto result = dmludp_connection->send_elicit_ack_message_pktnum_new3();
                            auto retval = send(server_fd, dmludp_connection->send_ack.data(), result, 0);
                            if(retval == -1){
                                if (errno == EINTR){
                                    continue;
                                }

                                if (errno == EAGAIN){
                                    dmludp_connection->send_packet_complete(EAGAIN);
                                }
                                break;
                            }
                            dmludp_connection->send_packet_complete();
                            break;
                        }
                    }

                }
            }
        }
    }

    close(server_fd);
    return 0;
}
