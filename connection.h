#pragma once
#include <unordered_map>
#include <cstdint>
#include <sys/socket.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <sys/socket.h>
#include <set>
#include <unordered_map>
#include <sys/uio.h>
#include <stdlib.h>
#include <numeric>
#include "packet.h"
#include "Recovery.h"
#include "recv_buf.h"
#include "send_buf.h"
#include <cmath>

namespace dmludp {

const size_t HEADER_LENGTH = 26;

// The default max_datagram_size used in congestion control.
const size_t MAX_SEND_UDP_PAYLOAD_SIZE = 8900;

const size_t MAX_ACK_NUM_PKTNUM = MAX_SEND_UDP_PAYLOAD_SIZE;

const size_t ELICIT_FLAG = 8;

const double alpha = 0.875;

const double beta = 0.25;

using Type_len = uint8_t;

using Packet_num_len = uint64_t;

using Priority_len = uint8_t;

using Offset_len = uint32_t;

using Acknowledge_sequence_len = uint64_t;

using Difference_len = uint8_t;

using Acknowledge_time_len = uint8_t;

using Packet_len = uint16_t;

struct SendInfo {
    /// The local address the packet should be sent from.
    sockaddr_storage from;

    /// The remote address the packet should be sent to.
    sockaddr_storage to;

};

struct RecvInfo {
    /// The remote address the packet was received from.
    sockaddr_storage from;

    /// The local address the packet was received on.
    sockaddr_storage to;
};


class sbuffer{
public:
    uint8_t * src;

    size_t len;

    size_t left;

    sbuffer(uint8_t* src, size_t len):
    src(src),
    len(len),
    left(len){

    };

    ~sbuffer(){};

    size_t sent(){
        return len - left;
    };

    void written(size_t result){
        left -= result;
    };
};

class Config {
public:
    size_t max_send_udp_payload_size;

    uint64_t max_idle_timeout;

    Config():
    max_send_udp_payload_size(MAX_SEND_UDP_PAYLOAD_SIZE),
    max_idle_timeout(5000){};

    ~Config(){

    }; 

    /// Sets the `max_idle_timeout` transport parameter, in milliseconds.
    /// same with tcp max idle timeout
    /// The default value is infinite, that is, no timeout is used.
    void set_max_idle_timeout(uint64_t v) {
        max_idle_timeout = v;
    };
};

class Received_Record_Debug{
public:
    // key: #packet, value: (offset, length)
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> pktnum2offset;

    // key: acknowledge packet num, value: (application num, received or not)
    std::map<uint64_t, std::map<uint64_t, uint8_t>> acknowledege_record;

    std::vector<std::pair<uint64_t, uint64_t>> received_complete_record;

    Received_Record_Debug(){};

    ~Received_Record_Debug(){};

    void add_offset_and_pktnum(uint64_t pn, uint64_t offset, uint16_t len){
        pktnum2offset.emplace(pn, std::make_pair(offset, len));
    };

    void add_acknowledeg_info(uint64_t ack_pn, std::map<uint64_t, uint8_t> received){
        acknowledege_record[ack_pn] = std::move(received);
    }

    void add_recevie(uint64_t rx_excepted, uint64_t received_len){
        received_complete_record.emplace_back(rx_excepted, received_len);
    }

    void clear(){
        pktnum2offset.clear();
        acknowledege_record.clear();
    };

    void show(){
        std::cout<<"[Receive Error]"<<std::endl;
        std::cout<<"[Info] Application packet info"<<std::endl;
        for (const auto& [key, value] : pktnum2offset){
            std::cout << "Application: " << key << ", offset: " << value.first << ", length:" << value.second << std::endl;
        }
        std::cout<<std::endl;
        std::cout<<"[Info] Acknowledge info"<<std::endl;
        for (const auto& outer_pair : acknowledege_record) {
            std::cout << "Acknowledge pktnum: " << outer_pair.first << std::endl;

            // iterate inner map
            for (const auto& inner_pair : outer_pair.second) {
                std::cout << "Application num: " << inner_pair.first << ", received: " << (int)inner_pair.second << std::endl;
            }
        }

        std::cout<<"[Info] Application packet received"<<std::endl;
        for (auto it = received_complete_record.begin(); it != received_complete_record.end(); ++it) {
            std::cout << "[Compare] rx_length:" << it->first << " "<<(it->first == it->second)<< " rlen:" << it->second << std::endl;
        }   
        std::cout<<std::endl;

    }

};

class Connection{
public: 
    size_t recv_count;

    /// Total number of sent packets.
    size_t sent_count;

    /// Whether this is a server-side connection.
    bool is_server;

    /// Whether the connection handshake has been completed.
    bool handshake_completed;

    /// Whether the connection handshake has been confirmed.
    bool handshake_confirmed;

    /// Whether the connection is closed.
    bool closed;

    bool server;

    struct sockaddr_storage localaddr;

    struct sockaddr_storage peeraddr;
    
    size_t written_data;

    bool stop_flag;

    bool stop_ack;

    // Record sent packet offset
    std::vector<uint64_t> record2ack;

    // Record sent application packet num
    std::vector<uint64_t> record2ack_pktnum;

    // Key: sent packet number, value: correspoind offset
    std::map<uint64_t, uint64_t> pktnum2offset;

    // map for received application pktnum and corresponding offset
    std::map<uint64_t, std::pair<uint64_t,uint8_t>> receive_pktnum2offset;

    std::vector<uint8_t> receive_result;

    std::unordered_map<uint64_t, uint8_t> recv_dic;

    // store norm2 for every 256 bits float
    // Note: It refers to the priorty of each packet.
    std::vector<uint8_t> norm2_vec;

    size_t rx_length;

    bool recv_flag;

    bool feed_back;

    uint64_t send_num;

    std::unordered_map<uint64_t, uint64_t> sent_dic;

    bool bidirect;

    Recovery recovery;

    std::array<PktNumSpace, 2> pkt_num_spaces;

    std::chrono::nanoseconds rtt;

    std::chrono::nanoseconds srtt;

    std::chrono::nanoseconds rto;

    std::chrono::nanoseconds rttvar;
    
    std::chrono::high_resolution_clock::time_point handshake;

    // std::set<uint64_t> ack_set;

    SendBuf send_buffer;

    RecvBuf rec_buffer;

    Received_Record_Debug RRD;

    // Date: 7th Jan 2024
    std::vector<sbuffer> data_buffer;

    size_t current_buffer_pos;

    bool initial;

    // Record how many data sent at once.
    size_t written_data_len;

    // total data sent after one get data.
    size_t written_data_once;

    // Record errno
    size_t dmludp_error;

    // Used to record how many packet has been sent before EAGAIN
    size_t dmludp_error_sent;

    // sender record the map relationship between acknowledege packet number and (start application packet number and end application packet number)
    std::map<uint64_t, std::pair<uint64_t, uint64_t>> send_pkt_duration;

    static std::shared_ptr<Connection> connect(sockaddr_storage local, sockaddr_storage peer, Config config ) {
        return std::make_shared<Connection>(local, peer, config, false);
    };

    static std::shared_ptr<Connection> accept(sockaddr_storage local, sockaddr_storage peer, Config config)  {
        return std::make_shared<Connection>(local, peer, config, true);
    };

    const static uint8_t handshake_header[HEADER_LENGTH];

    const static uint8_t fin_header[HEADER_LENGTH];

    // when get new data flow, send_connection_difference++
    uint8_t send_connection_difference;

    // receive_connection_difference keeps current data flow send_connection_difference. 
    // If receive_complete() is true, receive_connection_difference++ to keep track with send_connection_difference.
    uint8_t receive_connection_difference;

    size_t current_loop_min;

    size_t current_loop_max;

    std::pair<uint64_t, uint64_t> receive_range;

    std::pair<uint64_t, uint64_t> send_range;

    bool can_send;

    bool partial_send;

    size_t congestion_window;

    std::set<uint64_t> send_unack_packet_record;

    Packet_num_len last_elicit_ack_pktnum;

    uint16_t epoll_delay;

    uint64_t last_congestion_window;

    size_t received_packets;

    // Record how many packet sent.
    size_t partial_send_packets;

    size_t plan2send;

    std::pair<uint64_t, uint64_t> last_cwnd_copy;

    std::pair<uint64_t, uint64_t> current_range;

    std::pair<uint64_t, uint64_t> last_range;

    std::pair<uint64_t, uint64_t> last_last_range;

    std::pair<uint64_t, uint64_t> current_partial_range;

    bool send_signal;

    // when partial ack near normal ack, to solve normal timeout.
    bool partial_signal;
    // multiple partial ack, and timeout
    bool partial_signal2;

    ////////////////////////////////
    /*
    If using sendmsg send udp data, replace mmsghdr with msghdr.
    */
    std::vector<std::shared_ptr<Header>> send_hdrs;
    std::vector<struct mmsghdr> send_messages;
    std::vector<struct iovec> send_iovecs;
    std::vector<uint8_t> send_ack;

    struct iovec ack_iovec;
    
    struct mmsghdr ack_mmsghdr;

    ssize_t send_message_start;

    ssize_t send_message_end;

    ssize_t send_message_left;

    // In one cwnd, real sent packets
    ssize_t real_sent;
    // Expected sent packets
    ssize_t expect_sent;

    std::pair<uint64_t, uint64_t> next_range;

    // Set at get_data()
    bool data_gotten;    

    bool send_partial_signal;

    bool send_full_signal;

    bool send_phrase;

    // used to judge loss or not
    std::pair<uint64_t, uint64_t> sent_packet_range;

    std::pair<uint64_t, uint64_t> sent_packet_range_cache1;

    std::pair<uint64_t, uint64_t> sent_packet_range_cache2;

    // send_difference < receive_difference
    // difference_flag = true;
    bool difference_flag;
    ///////////////////////////////

    Connection(sockaddr_storage local, sockaddr_storage peer, Config config, bool server):    
    recv_count(0),
    sent_count(0),
    is_server(server),
    handshake_completed(false),
    handshake_confirmed(false),
    closed(false),
    server(server),
    localaddr(local),
    peeraddr(peer),
    written_data(0),
    stop_flag(true),
    stop_ack(true),
    recv_dic(),
    norm2_vec(),
    recv_flag(false),
    feed_back(false),
    send_num(0),
    rtt(0),
    srtt(0),
    rto(0),
    rttvar(0),
    handshake(std::chrono::high_resolution_clock::now()),
    bidirect(true),
    initial(false),
    current_buffer_pos(0),
    written_data_len(0),
    written_data_once(0),
    dmludp_error(0),
    rx_length(0),
    dmludp_error_sent(0),
    send_connection_difference(0),
    receive_connection_difference(1),
    current_loop_min(0),
    current_loop_max(0),
    can_send(true),
    partial_send(false),
    congestion_window(0),
    last_elicit_ack_pktnum(0),
    epoll_delay(0),
    last_congestion_window(0),
    plan2send(0),
    received_packets(0),
    partial_send_packets(0),
    send_signal(false),
    partial_signal(false),
    partial_signal2(false),
    send_message_start(0),
    send_message_end(0),
    send_message_left(0),
    data_gotten(true),
    send_phrase(true),
    difference_flag(false),
    real_sent(0),
    expect_sent(0)
    {
        send_ack.reserve(42);
        init();
    };

    ~Connection(){

    };

    void init(){
        send_hdrs.reserve(500);
        send_messages.reserve(1000);
        send_iovecs.reserve(1000);
        ack_iovec.iov_base = nullptr;
        ack_iovec.iov_len = 0;
        memset(&ack_mmsghdr, 0, sizeof(ack_mmsghdr));
    }

    void initial_rtt() {
        auto arrive_time = std::chrono::high_resolution_clock::now();
        srtt = arrive_time - handshake;
        // std::cout<<"Initial rtt:"<<std::chrono::duration<double, std::nano>(srtt).count();
        rttvar = srtt / 2;
        // std::cout<<", rttvar:"<<std::chrono::duration<double, std::nano>(rttvar).count();
        rto = srtt + 4 * rttvar;
        //std::cout<<"rto:"<<std::chrono::duration<double, std::nano>(rto).count()<<std::endl;
    }

    void update_rtt2(std::chrono::high_resolution_clock::time_point tmp_handeshake){
        auto arrive_time = std::chrono::high_resolution_clock::now();
        rtt = arrive_time - tmp_handeshake;
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(arrive_time.time_since_epoch()).count();
        // std::cout << "update_rtt: " << now_ns << " ns" << ", srtt:"<<srtt.count()<<", rtt:"<<rtt.count()<<std::endl;
        auto tmp_srtt = std::chrono::duration<double, std::nano>(srtt.count() * alpha + (1 - alpha) * rtt.count());
        // std::cout<<"tmp_srtt:"<<tmp_srtt.count();
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_srtt);
        auto diff = srtt - rtt;
        auto tmp_rttvar = std::chrono::duration<double, std::nano>((1 - beta) * rttvar.count() + beta * std::abs(diff.count()));
        // std::cout<<", tmp_rttvar:"<<tmp_rttvar.count();
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_rttvar);
        // std::cout<<", new srtt:"<<srtt.count();
        rto = srtt + 4 * rttvar;
    }

    void update_rtt() {
        /*
        handshake_confirmed
        handshake_complete
        
        1st RTO:
        SRTT <- R
        RTTVAR <- R/2
        RTO <- SRTT + max (G, K*RTTVAR)
        where K = 4.
        */

        /*
        RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R'|
        SRTT <- (1 - alpha) * SRTT + alpha * R'
        RTO <- SRTT + max (G, K*RTTVAR)
        */
        auto arrive_time = std::chrono::high_resolution_clock::now();
        rtt = arrive_time - handshake;    
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(arrive_time.time_since_epoch()).count();
       // std::cout << "update_rtt: " << now_ns << " ns" << ", srtt:"<<srtt.count()<<", rtt:"<<rtt.count();
        auto tmp_srtt = std::chrono::duration<double, std::nano>(srtt.count() * alpha + (1 - alpha) * rtt.count());
        // std::cout<<"tmp_srtt:"<<tmp_srtt.count();
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_srtt);
        auto diff = srtt - rtt;
        auto tmp_rttvar = std::chrono::duration<double, std::nano>((1 - beta) * rttvar.count() + beta * std::abs(diff.count()));
        // std::cout<<", tmp_rttvar:"<<tmp_rttvar.count();
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_rttvar);
        // std::cout<<", new srtt:"<<srtt.count();
        rto = srtt + 4 * rttvar;
        //std::cout<<", rto:"<<std::chrono::duration<double, std::nano>(rto).count()<<std::endl;
    };

    // Merge to intial_rtt
    void set_rtt(uint64_t inter){
        if (bidirect){
            rtt = std::chrono::nanoseconds(inter);
        }
        if (srtt.count() == 0){
            srtt = rtt;
        }
        if (rttvar.count() == 0){
            rttvar = rtt / 2;
        }
    };

    Type header_info(uint8_t* src, size_t src_len, Packet_num_len &pkt_num, Priority_len &pkt_priorty, 
        Offset_len &pkt_offset, Acknowledge_sequence_len &pkt_seq, Acknowledge_time_len &pkt_ack_time,
        Difference_len &pkt_difference, Packet_len &pkt_len){
        auto pkt_ty = reinterpret_cast<Header *>(src)->ty;
        pkt_num = reinterpret_cast<Header *>(src)->pkt_num;
        pkt_priorty = reinterpret_cast<Header *>(src)->priority;
        pkt_offset = reinterpret_cast<Header *>(src)->offset;
        pkt_seq = reinterpret_cast<Header *>(src)->seq;
        pkt_ack_time = reinterpret_cast<Header *>(src)->ack_time;
        pkt_difference = reinterpret_cast<Header *>(src)->difference;
        pkt_len = reinterpret_cast<Header *>(src)->pkt_length;
        return pkt_ty;
    };

    size_t recv_slice(uint8_t* src, size_t src_len){
        recv_count += 1;

        Packet_num_len pkt_num;
        Priority_len pkt_priorty;
        Offset_len pkt_offset;
        Acknowledge_sequence_len pkt_seq;
        Acknowledge_time_len pkt_ack_time;
        Difference_len pkt_difference;
        Packet_len pkt_len;
        
        auto pkt_ty = header_info(src, src_len, pkt_num, pkt_priorty, pkt_offset, pkt_seq, pkt_ack_time, pkt_difference, pkt_len);
       
        size_t read_ = 0;

        if (pkt_ty == Type::Handshake && is_server){
            if(handshake_completed){
                initial_rtt();
            }
            handshake_completed = true;
            initial = true;
        }

        //If receiver receives a Handshake packet, it will be prepared to send a Handshank.
        if (pkt_ty == Type::Handshake && !is_server){
            initial_rtt();
            handshake_confirmed = true;
            feed_back = true;
        }
        
        // All side can send data.
        if (pkt_ty == Type::ACK){
            // handshake = std::chrono::high_resolution_clock::now();
            // auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(handshake.time_since_epoch()).count();
            //std::cout << "ack ts: " << now_ns << " ns" << std::endl;
            process_acknowledge(src, src_len);
	    }

        if (pkt_ty == Type::ElicitAck){
            /* TODO
            Differenciate 2 scenario:
            1. send_difference > receive_difference
            2. send_difference < receive_difference
                All elicit acknowledge should be responsed with all 1
            */
            recv_flag = true;
            // std::cout<<"[Receive] ElicitAck packet number:"<<hdr->pkt_num<<std::endl;
            send_num = pkt_num;
            check_loss_pktnum(src, src_len);
            feed_back = true;
        }

        if (pkt_ty == Type::Application){
            // std::cout<<"[Debug] application offset:"<<pkt_offset<<", pn:"<<pkt_num<<std::endl;
            if (receive_pktnum2offset.find(pkt_num) != receive_pktnum2offset.end()){
                if(receive_pktnum2offset.at(pkt_num).second == 1){
                    return 0;
		        }
            }
            //     std::cout<<"[Error] Duplicate application packet"<<std::endl;
            //     _Exit(0);
            // }
            
            // RRD.add_offset_and_pktnum(hdr->pkt_num, hdr->offset, hdr->pkt_length);
            // if (pkt_offset == 0){
            //     clear_recv_setting();
            // }

            if (pkt_difference != receive_connection_difference){
                clear_recv_setting();
            }
            if (pkt_num > current_loop_max){
                current_loop_max = pkt_num;
            }

            if (pkt_num < current_loop_min){
                current_loop_min = pkt_num;
            }

            reset_single_receive_parameter(pkt_seq);
            // send_num = pkt_seq;
            
            // Debug
            // if (recv_dic.find(pkt_offset) != recv_dic.end()){
            //     // std::cout<<"[Error] same offset:"<<pkt_offset<<std::endl;
            //     // RRD.show();
            //     // _Exit(0);
            //     // receive_pktnum2offset.insert(std::make_pair(pkt_num, pkt_offset));
            //     rec_buffer.write(src + HEADER_LENGTH, pkt_len, pkt_offset);
            //     // Debug
            //     // recv_dic.insert(std::make_pair(pkt_offset, pkt_priorty));
            // }else{
            //     recv_count += 1;
            //     // optimize to reduce copy time.
                
            //     // receive_pktnum2offset.insert(std::make_pair(pkt_num, pkt_offset));
            //     // Debug
            //     // recv_dic.insert(std::make_pair(pkt_offset, pkt_priorty));
            // }
            if (recv_dic.find(pkt_offset) == recv_dic.end() && pkt_difference == receive_connection_difference){
                rec_buffer.write(src + HEADER_LENGTH, pkt_len, pkt_offset);
                recv_dic.insert(std::make_pair(pkt_offset, pkt_priorty));
            }

            receive_pktnum2offset.insert(std::make_pair(pkt_num, std::make_pair(pkt_offset, 1)));
            
            // std::cout<<"receive buffer size:"<<rec_buffer.data.size()<<std::endl;
                     
        }

        // In dmludp.h
        if (pkt_ty == Type::Stop){
            stop_flag = false;
            return 0;
        }

        return read_;
    };


    int pre_process_application_packet(uint8_t* data, size_t buf_len, uint32_t &off, uint64_t &pn){
        auto result = reinterpret_cast<Header *>(data)->ty;
        pn = reinterpret_cast<Header *>(data)->pkt_num;
        auto pkt_priorty = reinterpret_cast<Header *>(data)->priority;
        off = reinterpret_cast<Header *>(data)->offset;
        auto pkt_len = reinterpret_cast<Header *>(data)->pkt_length;
        Difference_len pkt_difference = reinterpret_cast<Header *>(data)->difference;
        auto pkt_seq = reinterpret_cast<Header *>(data)->seq;

        if (result == Type::Application){
            // std::cout<<"[Debug] application packet:"<<pn<<", off:"<<off<<", len:"<<pkt_len<<", difference:"<<(int)pkt_difference<<", receive_connection_difference:"<<(int)receive_connection_difference<<std::endl;
            if (receive_pktnum2offset.find(pn) != receive_pktnum2offset.end()){
                std::cout<<"[Error] Duplicate application packet"<<std::endl;
                return 0;
            }
                
            // RRD.add_offset_and_pktnum(hdr->pkt_num, hdr->offset, hdr->pkt_length);
            uint8_t tmp_send = pkt_difference + 1;
            uint8_t tmp_received = receive_connection_difference + 1;
            if (tmp_send == receive_connection_difference){
                difference_flag = true;
                return 0;
            }

            if (pkt_difference != receive_connection_difference){
                clear_recv_setting();
            }
            
            if (pn > current_loop_max){
                current_loop_max = pn;
            }

            if (pn < current_loop_min){
                current_loop_min = pn;
            }
            reset_single_receive_parameter(pkt_seq);
            // send_num = pkt_seq;

            // Debug
            if (recv_dic.find(off) != recv_dic.end()){
                std::cout<<"[Error] same offset:"<<off<<std::endl;
                // RRD.show();
                // _Exit(0);
                receive_pktnum2offset.insert(std::make_pair(pn, std::make_pair(off,0)));
                // // Debug
                // recv_dic.insert(std::make_pair(off, pkt_priorty));
            }else{
                recv_count += 1;
                // optimize to reduce copy time.
                receive_pktnum2offset.insert(std::make_pair(pn, std::make_pair(off,0)));
                // Debug
                // recv_dic.insert(std::make_pair(off, pkt_priorty));
            }
        }
        return result;
    };

    void update_receive_parameter(){
        current_loop_min = current_loop_max + 1;
    }

    void process_acknowledge(uint8_t* src, size_t src_len){
        Packet_num_len pkt_num = reinterpret_cast<Header *>(src)->pkt_num;
        Acknowledge_time_len pkt_ack_time = reinterpret_cast<Header *>(src)->ack_time;
        Difference_len pkt_difference = reinterpret_cast<Header *>(src)->difference;

        uint8_t tmp_send_connection_difference = send_connection_difference + 1;
        if(send_connection_difference != pkt_difference){
		    // std::cout<<"send_connection_difference != pkt_difference"<<std::endl;
            if (tmp_send_connection_difference == pkt_difference){
                send_buffer.clear();
            }
            return;
        }
	    // std::cout<<"send_connection_difference:"<<(int)send_connection_difference<<", packet connection:"<<(int)reinterpret_cast<Header *>(src)->difference<<std::endl;
        auto first_pkt = *(uint64_t *)(src + HEADER_LENGTH);
        auto end_pkt = *(uint64_t *)(src + HEADER_LENGTH + sizeof(uint64_t));
	    // std::cout<<"first_pkt:"<<first_pkt<<", end_pkt:"<<end_pkt<<std::endl;
	    // std::cout<<"sent_packet_range.first:"<<sent_packet_range.first<<", sent_packet_range.second:"<<sent_packet_range.second<<std::endl;
	    // std::cout<<"last ack:"<<last_elicit_ack_pktnum<<std::endl;
	    // std::cout<<"packet seq:"<<reinterpret_cast<Header *>(src)->seq<<std::endl;
        bool loss_check = false;
        size_t received_num = 0;

        bool is_last_ack = false;

        if (pkt_num < last_elicit_ack_pktnum){
            is_last_ack = true;
        }

	    //std::cout<<"pkt_num:"<<pkt_num<<", last_elicit_ack_pktnum:"<<last_elicit_ack_pktnum<<std::endl;
	    //std::cout<<"last_cwnd_copy.first:"<<last_cwnd_copy.first<<", last_cwnd_copy.second:"<<last_cwnd_copy.second<<std::endl;
        //std::cout<<"sent_packet_range.first:"<<sent_packet_range.first<<", sent_packet_range.second:"<<sent_packet_range.second<<std::endl;
        for (auto check_pn = first_pkt; check_pn <= end_pkt; check_pn++){
            auto check_offset = pktnum2offset[check_pn];
            
            auto received_ = check_pn - first_pkt + 2 * sizeof(Packet_num_len) + HEADER_LENGTH;
            send_unack_packet_record.erase(check_pn);
       
            if (src[received_] == 0){
                send_buffer.acknowledege_and_drop(check_offset, true);
                // std::cout<<"pn:"<<check_pn<<", offset:"<<check_offset<<" received"<<std::endl;
                received_num++;
                // if (check_pn <= last_cwnd_copy.second && check_pn >= last_cwnd_copy.first){
                //     received_packets++;
                //     // remove acked packet number avoding duplicate partial sending.
                // }
                if (check_pn <= sent_packet_range.second && check_pn >= sent_packet_range.first){
                    received_packets++;
                    // remove acked packet number avoding duplicate partial sending.
                }
            }else{
                loss_check = true;
		        if(sent_dic.find(check_offset)!=sent_dic.end()){
                    if (sent_dic.at(check_offset) == 0){
                        // Send proactively low priority packet if it wasn't received by receiver.
                        send_buffer.acknowledege_and_drop(check_offset, true);
                    }
		        }
            }
        }


        if (received_packets == sent_packet_range.second - sent_packet_range.first + 1){
            loss_check = false;
        }  

        if (end_pkt == sent_packet_range.second){
            pkt_ack_time = 1;
        }

        if (is_last_ack){
            // For past acknowledge, just remove received data from send buffer.
	        //	std::cout<<"is_last_ack"<<std::endl;
            send_unack_packet_record.erase(send_unack_packet_record.begin(), send_unack_packet_record.lower_bound(first_pkt));
            send_pkt_duration.erase(pkt_num);
            can_send = false;
            partial_send = false;
        }else{
            if (pkt_num == last_elicit_ack_pktnum){
                if (pkt_ack_time == 1){
                    // Cover last window all received info
                    can_send = true;
                    partial_send = false;
                    update_rtt();
                    send_pkt_duration.erase(pkt_num);
                    send_range = std::make_pair(1, 0);
                    if (!loss_check){
                        recovery.update_win(true, 1);
                    }else{
                        recovery.update_win(true);
                    }
                    send_unack_packet_record.clear();
                    send_signal = true;
                }else{
                    // Just cover partial received info.
		            if(received_packets !=0){
                        partial_send = true;
		               // std::cout<<"partial_send:"<<received_packets<<std::endl;
                        can_send = false;
                    }
                }
                partial_signal2 = true;
            }
            
        }

    }

    void clear_recv_setting(){
        recv_dic.clear();
        receive_pktnum2offset.clear();
    }

    void recv_reset(){
        rec_buffer.reset();
    }

    //  no loss scenario, no stop packet.
    bool receive_complete(){
        auto rlen = rec_buffer.receive_length();
	    // std::cout<<"[Compare] rx_length:"<<rx_length<<" "<<(rx_length == rlen)<<" rlen:"<<rlen<<std::endl;
        if (rx_length == rlen){
            // RRD.clear();
            receive_connection_difference++;
            clear_recv_setting();
            return true;
        }
        return false;
    }

    void rx_len(size_t expected){
        rx_length = expected;
    }

    void reset_rx_len(){
        rx_length = 0;
    }

    void set_send_time(){
        handshake = std::chrono::high_resolution_clock::now();
        send_buffer.manage_recovery();
    }
    
    // Used to get pointer owner and length
    // get_data() is used after get(op) in gloo.
    bool get_data(struct iovec* iovecs, int iovecs_len, const std::vector<std::vector<uint8_t>> &priotity_list = {}){
        bool completed = true;
        written_data_once = 0;
        written_data_len = 0;
	    dmludp_error_sent = 0;
        // Sequcence2range.clear();

        current_buffer_pos = 0;

        // if (!send_pkt_duration.empty()){
        //     std::cout<<"[Error] send_pkt_duration is not empty!"<<std::endl;
        //     _Exit(0);
        // }

        send_buffer.off = 0;
        if (data_buffer.size() > 0){
            completed = false;
            return completed;
        }
        send_connection_difference++;
        for (auto i = 0 ; i < iovecs_len; i++){
            data_buffer.emplace_back(reinterpret_cast<uint8_t*>(iovecs[i].iov_base), iovecs[i].iov_len);
            written_data_once += iovecs[i].iov_len;
        }
        if (data_buffer.size() == 0){
            completed = false;
        }

        if (!norm2_vec.empty()){
            norm2_vec.clear();
        }
        size_t len = 0;
        if (iovecs_len % MAX_SEND_UDP_PAYLOAD_SIZE == 0){
            len = written_data_once / MAX_SEND_UDP_PAYLOAD_SIZE;
        }else{
            len = written_data_once / MAX_SEND_UDP_PAYLOAD_SIZE + 1;
        }
        if (len == 1){
            norm2_vec.insert(norm2_vec.end(), len, 3);
        }else{
            norm2_vec.insert(norm2_vec.end(), len, 3);
        }
        sent_dic.clear();
        pktnum2offset.clear();
        send_buffer.clear();
        can_send = true;
        partial_send = false;
        data_gotten = true;
        real_sent = 0;
        expect_sent = 0;
        return completed;
    }

    size_t get_once_data_len(){
        return written_data_once;
    }

    void clear_sent_once(){
        written_data_once = 0;
    }
 
    void data_preparation(){
        data2buffer(data_buffer.at(current_buffer_pos));
    }

    void recovery_send_buffer(){
        send_buffer.recovery_data();
    }

    ssize_t data2buffer(sbuffer &send_data){
        ssize_t result = 0;
        size_t out_len = 0;
        if (send_buffer.is_empty()){
            result = send_buffer.write(send_data.src, send_data.sent(), send_data.left, recovery.cwnd_expect(), out_len);
        }else{
            if (send_data.left == 0){
                result = -1;
            }else{
                result = send_buffer.write(send_data.src, send_data.sent(), send_data.left, recovery.cwnd_expect() * 2, out_len);
                result = 0;
            }
        }
        // std::cout<<"send_buffer.size()"<<send_buffer.data.size()<<std::endl;
        return result;
    }

    // Check timeout or not
    bool on_timeout(){
        bool timeout_;
        std::chrono::nanoseconds duration((uint64_t)(get_rtt()));
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        // std::cout<<"on_time:"<< now_ns << " ns" <<std::endl;
        if (epoll_delay >= 4){
            duration *= 2;
        }
        if (handshake + duration < now){
            timeout_ = true;
        }else{
            timeout_ = false;
        }
        return timeout_;
    }

    ssize_t check_status(){
        /*
        1. acknowledge within timer, no time out. can_send: true, timeout: false.
        2. no acknowledge packet arrive, no time out. start preparing send_buffer. can_send: false, timeout: false.
        3. partial acknowlege packet arrive, no time out. send equivalent packets and preparing send buffer. can
        4. time out. timeout true
        */
        ssize_t result = 0;
        if (on_timeout()){
            if (partial_signal && !can_send){
                recovery.update_win(true);
                can_send = true;
                partial_send = false;
            }

            if (partial_signal2 && !can_send && !partial_send){
                recovery.update_win(true);
                can_send = true;
                partial_send = false;
            }

            // Time out
            if (can_send){
                congestion_window = recovery.cwnd();
                if(partial_send_packets *  MAX_SEND_UDP_PAYLOAD_SIZE >= congestion_window){
                    send_buffer.update_max_data(MAX_SEND_UDP_PAYLOAD_SIZE);
                }else{
                    send_buffer.update_max_data(congestion_window - partial_send_packets *  MAX_SEND_UDP_PAYLOAD_SIZE);
                }
		        // std::cout<<"3 congestion_window:"<<congestion_window<<std::endl;
                if (data_buffer.at(current_buffer_pos).left > 0){
                    data2buffer(data_buffer.at(current_buffer_pos));
                }
                result = 4;
                // partial_send_packets = 0;
                // received_packets = 0;
            }else{
                if (partial_send){
                    // Partial acknowldege will be regarded as loss.
                    recovery.update_win(true);
                    can_send = false;
                    congestion_window = recovery.cwnd();
                    send_buffer.update_max_data(congestion_window);
                    if (data_buffer.at(current_buffer_pos).left > 0){
                        data2buffer(data_buffer.at(current_buffer_pos));
                    }
                    result = 6;
                    // received_packets = 0;
                }else{
                    // Real time out, congestion control window will grow fron initial size.
                    recovery.set_recovery(true);
                    can_send = false;
                    congestion_window = recovery.cwnd();
                    // std::cout<<"1 congestion_window:"<<congestion_window<<std::endl;
                    if (partial_send){
                        if(partial_send_packets * MAX_SEND_UDP_PAYLOAD_SIZE >= congestion_window){
                            send_buffer.update_max_data(MAX_SEND_UDP_PAYLOAD_SIZE);
                        }else{
                            send_buffer.update_max_data(congestion_window - partial_send_packets *  MAX_SEND_UDP_PAYLOAD_SIZE);
                        }
                    }else{
                        send_buffer.update_max_data(congestion_window);
                    }
                    if (data_buffer.at(current_buffer_pos).left > 0){
                        data2buffer(data_buffer.at(current_buffer_pos));
                    }
                    for (auto x = send_unack_packet_record.begin(); x != send_unack_packet_record.end(); x++){
                        if (pktnum2offset.find(*x) != pktnum2offset.end()){
                            auto delete_offset = pktnum2offset[*x];
                        }
                    }

                    if(congestion_window == last_congestion_window){
                        epoll_delay++;
                    }else{
                        epoll_delay = 0;
                    }

                    result = 5;
                    // partial_send_packets = 0;
                    // received_packets = 0;
                }
            }
        }else{
            if (can_send){
                //TODO: set cwnd to send buffer
                // no time out, sender can send data.
                congestion_window = recovery.cwnd();
		        // std::cout<<"2 congestion_window:"<<congestion_window<<std::endl;
                if(partial_send_packets *  MAX_SEND_UDP_PAYLOAD_SIZE >= congestion_window){
                    send_buffer.update_max_data(MAX_SEND_UDP_PAYLOAD_SIZE);
                }else{
                    send_buffer.update_max_data(congestion_window - partial_send_packets *  MAX_SEND_UDP_PAYLOAD_SIZE);
                }
                if (data_buffer.at(current_buffer_pos).left > 0){
                    data2buffer(data_buffer.at(current_buffer_pos));
                }
                result = 1;
                // partial_send_packets = 0;
                // received_packets = 0;
            }else{
                // no time out, sender waits for the partial acknowldege or total acknowldege.
                if (partial_send){
                    result = 2;
                    send_buffer.update_max_data(received_packets * MAX_SEND_UDP_PAYLOAD_SIZE);
                    if (data_buffer.at(current_buffer_pos).left > 0){
                        data2buffer(data_buffer.at(current_buffer_pos));
                    }
                    // received_packets = 0;
                }else{
                    data_preparation();
                    result = 3;
                }
            }
        }
        // std::cout<<"check_status:"<<result<<std::endl;
      
        last_congestion_window = congestion_window;

        return result;
    }

    // check_status2() works with send_message()
    ssize_t check_status2(){
        /*
        1. acknowledge within timer, no time out. can_send: true, timeout: false.
        2. no acknowledge packet arrive, no time out. start preparing send_buffer. can_send: false, timeout: false.
        3. partial acknowlege packet arrive, no time out. send equivalent packets and preparing send buffer. can
        4. time out. timeout true
        */
        ssize_t result = 0;
        if (on_timeout()){
            if (partial_signal && !can_send){
                recovery.update_win(true);
                can_send = true;
                partial_send = false;
            }

            if (partial_signal2 && !can_send && !partial_send){
                recovery.update_win(true);
                can_send = true;
                partial_send = false;
            }

            // Time out
            if (can_send){
                congestion_window = recovery.cwnd();
		        // std::cout<<"3 congestion_window:"<<congestion_window<<std::endl;
                if (data_buffer.at(current_buffer_pos).left > 0){
                    data2buffer(data_buffer.at(current_buffer_pos));
                }
                result = 4;
            }else{
                if (partial_send){
                    // Partial acknowldege will be regarded as loss.
//                    recovery.update_win(true);
                    recovery.set_recovery(true);
      			    can_send = false;
                    congestion_window = recovery.cwnd();
		            // std::cout<<"4 congestion_window:"<<congestion_window<<std::endl;
                    // send_buffer.update_max_data(congestion_window);
                    if (data_buffer.at(current_buffer_pos).left > 0){
                        data2buffer(data_buffer.at(current_buffer_pos));
                    }
                    result = 6;
                    // received_packets = 0;
                }else{
                    // Real time out, congestion control window will grow fron initial size.
                    recovery.set_recovery(true);
                    can_send = false;
                    congestion_window = recovery.cwnd();
                    // std::cout<<"1 congestion_window:"<<congestion_window<<std::endl;
                    if (data_buffer.at(current_buffer_pos).left > 0){
                        data2buffer(data_buffer.at(current_buffer_pos));
                    }

                    if(congestion_window == last_congestion_window){
                        epoll_delay++;
                    }else{
                        epoll_delay = 0;
                    }
                    result = 5;
                }
            }
        }else{
            if (can_send){
                //TODO: set cwnd to send buffer
                // no time out, sender can send data.
                congestion_window = recovery.cwnd();
		        // std::cout<<"2 congestion_window:"<<congestion_window<<std::endl;
                if (data_buffer.at(current_buffer_pos).left > 0){
                    data2buffer(data_buffer.at(current_buffer_pos));
                }
                result = 1;
            }else{
                // no time out, sender waits for the partial acknowldege or total acknowldege.
                if (partial_send){
                    result = 2;
                    if (data_buffer.at(current_buffer_pos).left > 0){
                        data2buffer(data_buffer.at(current_buffer_pos));
                    }
                }else{
                    data_preparation();
                    result = 3;
                }
            }
        }
        // std::cout<<"check_status:"<<result<<std::endl;
      
        last_congestion_window = congestion_window;

        return result;
    }

    std::vector<struct mmsghdr> get_mmsghdr(){
	    // std::cout<<"send buffer data size:"<<send_buffer.data.size()<<std::endl;
        if (send_message_start < 0 || send_message_start > send_messages.size()){
            std::cout<<"[Error] send_message_start:"<<send_message_start<<", send_messages.size():"<<send_messages.size()<<std::endl;
	        std::cout<<"send buffer data size:"<<send_buffer.data.size()<<std::endl;
            _Exit(0);
        }
        	/*std::cout<<"ssend_message_start:"<<send_message_start<<", end_message_end:"<<send_message_end<<std::endl;
        	if(send_message_end%2==0){
                for(auto i = 0; i < send_message_end; i++){
                    for (size_t j = 0; j < send_iovecs[2*i].iov_len; ++j) {
                        std::cout << (int)static_cast<uint8_t*>(send_iovecs[2*i].iov_base)[j]<<" ";
                    }
                    std::cout << std::endl;
                }
            }else{
                for(auto i = 0; i < send_message_end - 1; i++){
                    for (size_t j = 0; j < send_iovecs[2*i].iov_len; ++j) {
                        std::cout << (int)static_cast<uint8_t*>(send_iovecs[2*i].iov_base)[j]<<" ";
                    }
                    std::cout << std::endl;
                }
	        }*/
    /*	for (auto k =send_message_start; k<send_messages.size();k++) {
            for (size_t i = 0; i < send_messages[k].msg_hdr.msg_iovlen; ++i) {
                if (send_messages[k].msg_hdr.msg_iov[i].iov_len == 26){
			    std::cout<<"k:"<<k<<" ";
                    for(auto j = 0; j < 26; j++){
                        std::cout << (int)(static_cast<uint8_t*>(send_messages[k].msg_hdr.msg_iov[i].iov_base))[j]<< " ";
                    }
                    std::cout<<std::endl;
                }
            }
        }*/
        // auto index = send_message_start;
        return send_messages;
    }

    /*  Prepare mmsghdr(either send_mmsg or send_partial_mmsg)
        Also processing the resend when EAGAIN occur
        Remove acknowledge packet from send_mmsg2, Add this message in send_messages
        Usage:
        send_messages();
        auto messages = get_mmsghdr();
        while(true){
            auto result += sendmmsg(messages);
        }
        if (result == -1){
            send_message_complete(EAGAIN, result);
        }else{
            send_message_complete();
        }
    */
    /*
    cwnd, partial, send_message.size
    */
    // ssize_t send_message(size_t &start_){
    //     // Check send status and if EAGAIN happens.
    //     auto has_error = get_dmludp_error();
	//     //std::cout<<"ssize_t send_message(size_t &start_)"<<std::endl;
    //     ssize_t written_len_ = 0;
    //     auto send_seq = 0;
    //     // Process EAGAIN
    //     /* TODO
    //         Here ignore the scenario that the resend caused by EAGAIN meets full send preparation.
    //     */ 
	//     //std::cout<<"data.size():"<<send_buffer.data.size()<<", copy.size():"<<send_buffer.data_copy.size()<<std::endl;
    //     // First process send error.
    //     if (has_error != 0){
    //         // std::cout<<"0 send_message_start:"<<send_message_start;
	//         start_ = send_message_start + dmludp_error_sent;
    //         written_len_ = send_message_end - dmludp_error_sent - send_message_start;
    //         if (written_len_ == 0){
    //             set_error(0, 0);
    //         }
	//         // std::cout<<", written_len_:"<<written_len_<<std::endl;
    //         return written_len_;
    //     }
    //     auto current_status = check_status2();

    //     if (data_gotten){
    //         auto tmp = send_first_message();
    //         send_message_start = 0;
    //         start_ = send_message_start;
    //         // std::cout<<"1 send_message_start:"<<send_message_start<<std::endl;
    //         send_message_end = send_messages.size();        
    //         written_len_ = send_message_end - send_message_start;
    //         // data_gotten = false;
    //         return written_len_;
    //     }

    //     // Status 4(normal), 6(loss), 5(time out), 1(normal), 2(partial), 3(preparation)
    //     // send partial
    //     // Partial send number is received packets
    //     // Fulll send number is cwnd / MAX_SEND_UDP_PAYLOAD_SIZE - partial send number + 1;
    //     if (current_status == 2){
    //         if (partial_send_packets > send_messages.size()){
    //             return 0;
    //         }
    //         send_message_start = partial_send_packets;
    //         partial_send_packets += received_packets; 
    //         //send_message_end = partial_send_packets;
    //         if(partial_send_packets > send_messages.size()){
    //             send_message_end = send_messages.size();
    //             partial_send_packets = send_messages.size();
    //         }else{
    //             send_message_end = partial_send_packets;
    //         }
            
	//         // if(send_message_start > send_messages.size()){
    //         //     return 0;
    //         // }

    //         if (send_message_start >= send_message_end){
    //             return 0;
    //         }
	//         // written_len_ = received_packets;

    //         written_len_ = send_message_end - send_message_start;
	//         //written_len_ = received_packets;
    //         // send_partial_signal = true;
    //         // send_full_signal = false;
    //         send_phrase = false;
    //         start_ = send_message_start;
    //         //std::cout<<"send_buffer.size:"<<send_buffer.data.size()<<std::endl;
    //         // std::cout<<"2 send_message_start:"<<send_message_start<<", partial:"<<partial_send_packets<<std::endl;
    //         if(send_message_start == partial_send_packets){
    //             return 0;
    //         }   
    //     }else if(current_status == 3){
    //         // Data preparation phrase.
    //         return 0;
    //     }else{
    //         //	std::cout<<"send_buffer.size:"<<send_buffer.data.size()<<std::endl;
    //         //	std::cout<<"send_buffer.offset.size:"<<send_buffer.received_offset.size()<<std::endl;
    //         // TODO
    //         // Recheck cwnd <=>? real data in buffer
    //         last_elicit_ack_pktnum = pkt_num_spaces.at(1).updatepktnum();
            
    //         auto sentpkts = 0;

    //         // Get packets numbers in this cwnd.
    //         if (congestion_window % MAX_SEND_UDP_PAYLOAD_SIZE == 0){
    //             sentpkts = congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE;
    //         }else{
    //             sentpkts = congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + 1;
    //         }
    //         // When send_messages cannot fullfil the cwnd, sendpkts resize to send_messages.size().
	//         if (sentpkts > send_messages.size()){
    //             sentpkts = send_messages.size();
    //         }

    //         // partial_send_packets: expected max sent packets
    //         // send_message_end: real sent packets
    //         bool less_check = false;
    //         if (sentpkts <= partial_send_packets){
    //             written_len_ = 0;
	// 	        less_check = true;
    //             send_message_start = send_message_end;
    //             if (send_message_start >= send_messages.size()){
    //                 send_message_start = send_messages.size();
    //             }
    //         }else{
    //             written_len_ = sentpkts - partial_send_packets;
    //             send_message_start = partial_send_packets;
    //         }  

    //         sent_packet_range_cache2.first = sent_packet_range_cache1.first;
    //         sent_packet_range_cache2.second = sent_packet_range_cache1.second;
    //         sent_packet_range_cache1.first = sent_packet_range.first;
    //         sent_packet_range_cache1.second = sent_packet_range.second;
    //         sent_packet_range.first = send_hdrs.at(0)->pkt_num;
    //         if (sentpkts <= partial_send_packets){
    //             auto end_index = send_message_end - 1;
    //             sent_packet_range.second = send_hdrs.at(end_index)->pkt_num;
    //         }else{
    //             auto end_index = std::min((partial_send_packets + written_len_ - 1), (send_hdrs.size() - 1));
    //             sent_packet_range.second = send_hdrs.at(end_index)->pkt_num;
    //         }

	//         // std::cout<<"seq:"<<last_elicit_ack_pktnum<<", sent_packet_range.first:"<<sent_packet_range.first<<", sent_packet_range.second:"<<sent_packet_range.second<<", send difference:"<<(int)send_connection_difference<<std::endl;

	//         // std::cout<<"send_message:"<<last_elicit_ack_pktnum<<std::endl; 
    //         send_elicit_ack_message_pktnum_new2(last_elicit_ack_pktnum, sentpkts);
    //         /*std::cout<<"Elicit ack"<<std::endl;
    //         for(auto index = 0; index<send_ack.size();index++){
    //             std::cout<<(int)send_ack[index]<<" ";
    //         }
    //         std::cout<<std::endl;*/
    //         ack_iovec.iov_base = send_ack.data();
    //         ack_iovec.iov_len = send_ack.size();

    //         ack_mmsghdr.msg_hdr.msg_iov = &ack_iovec;
    //         ack_mmsghdr.msg_hdr.msg_iovlen = 1;

	//         // std::cout<<"send_messages.size():"<<send_messages.size()<<", sentpkts:"<<sentpkts<<std::endl;
    //         //send_messages.insert(send_messages.begin() + sentpkts, ack_mmsghdr);
    //         /*if (sentpkts > send_messages.size()){
    //             sentpkts = send_messages.size();
    //         }*/
    //         if(send_messages.size() > sentpkts && !less_check){
    //             send_messages.insert(send_messages.begin() + sentpkts, ack_mmsghdr);
    //         }
    //         else if(send_messages.size() > sentpkts && less_check){
    //             // std::cout<<"partial_send_packets:"<<partial_send_packets<<std::endl;
    //             if(partial_send_packets < send_messages.size()){
    //                 send_messages.insert(send_messages.begin() + partial_send_packets, ack_mmsghdr);
    //             }else{
    //                 send_messages.push_back(ack_mmsghdr);
    //             }  
	//         }
    //         else{
    //             send_messages.push_back(ack_mmsghdr);
    //         }
    //         send_message_end = partial_send_packets + written_len_ + 1;
    //         written_len_++;
    //         // send_partial_signal = false;
    //         // send_full_signal = true;
    //         send_phrase = true;
    //         start_ = send_message_start;
	//         if(send_message_start > send_messages.size() && send_buffer.written_complete()){
    //             return 0;
    //         }
    //        // std::cout<<"3 send_message_start:"<<send_message_start<<std::endl;
    //         }
    //     //std::cout<<"send_message start_:"<<start_<<std::endl;
    //    // std::cout<<"send_message_start:"<<send_message_start<<", send_message_end:"<<send_message_end<<std::endl; 
    //     return written_len_;
    // }
    ssize_t send_message(size_t &start_){
        // Check send status and if EAGAIN happens.
        auto has_error = get_dmludp_error();
	    //std::cout<<"ssize_t send_message(size_t &start_)"<<std::endl;
        ssize_t written_len_ = 0;
        auto send_seq = 0;
        // Process EAGAIN
        /* TODO
            Here ignore the scenario that the resend caused by EAGAIN meets full send preparation.
        */ 
	    //std::cout<<"data.size():"<<send_buffer.data.size()<<", copy.size():"<<send_buffer.data_copy.size()<<std::endl;
        // First process send error.
        if (has_error != 0){
            // std::cout<<"0 send_message_start:"<<send_message_start;
	        start_ = send_message_start + dmludp_error_sent;
            written_len_ = send_message_end - dmludp_error_sent - send_message_start;
            if (written_len_ == 0){
                set_error(0, 0);
            }
	        // std::cout<<", written_len_:"<<written_len_<<std::endl;
            return written_len_;
        }
        auto current_status = check_status2();

        if (data_gotten){
            auto tmp = send_first_message();
            send_message_start = 0;
            start_ = send_message_start;
            // std::cout<<"1 send_message_start:"<<send_message_start<<std::endl;
            send_message_end = send_messages.size();        
            written_len_ = send_message_end - send_message_start;
            // data_gotten = false;
            return written_len_;
        }

        // Status 4(normal), 6(loss), 5(time out), 1(normal), 2(partial), 3(preparation)
        // send partial
        // Partial send number is received packets
        // Fulll send number is cwnd / MAX_SEND_UDP_PAYLOAD_SIZE - partial send number + 1;
        if (current_status == 2){
            if (send_message_start >= send_messages.size()){
                return 0;
            }

            if (send_message_end >= send_messages.size()){
                return 0;
            }

            send_message_start = send_message_end;
            ssize_t expected_send_message_end = send_message_end + received_packets;
            if (expected_send_message_end >= send_messages.size()){
                send_message_end = send_messages.size();
            }else{
                send_message_end = expected_send_message_end;
            }

            if (send_message_start >= send_message_end){
                return 0;
            }
            
            written_len_ = send_message_end - send_message_start;

            send_phrase = false;
            start_ = send_message_start;
        }else if(current_status == 3){
            // Data preparation phrase.
            return 0;
        }else{
            //	std::cout<<"send_buffer.size:"<<send_buffer.data.size()<<std::endl;
            //	std::cout<<"send_buffer.offset.size:"<<send_buffer.received_offset.size()<<std::endl;
            // TODO
            // Recheck cwnd <=>? real data in buffer
            last_elicit_ack_pktnum = pkt_num_spaces.at(1).updatepktnum();
            
            auto sentpkts = 0;

            // Get packets numbers in this cwnd.
            if (congestion_window % MAX_SEND_UDP_PAYLOAD_SIZE == 0){
                sentpkts = congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE;
            }else{
                sentpkts = congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + 1;
            }

            sent_packet_range_cache2.first = sent_packet_range_cache1.first;
            sent_packet_range_cache2.second = sent_packet_range_cache1.second;
            sent_packet_range_cache1.first = sent_packet_range.first;
            sent_packet_range_cache1.second = sent_packet_range.second;
            sent_packet_range.first = send_hdrs.at(0)->pkt_num;

            /* 
            sentpkts < send_message_end || sentpkts > send_message_end || sentpkts == send_message_end
            sentpkts < send_messages.size() || sentpkts > send_messages.size() || sentpkts == send_messages.size() 
            send_message_end < send_messages.size() || send_message_end > send_messages.size() || send_message_end == send_messages.size()
            A: sentpkts
            B: send_message_end
            C: send_messages.size()

            A <= B <= C 
            => sentpkts <= send_message_end <= send_messages.size() X
            A <= C <= B
            => sentpkts <= send_messages.size() <= send_message_end X
            B <= A <= C
            => send_message_end <= sentpkts <= send_messages.size()
            B <= C <= A
            => send_message_end <= send_messages.size() <= sentpkts
            C <= A <= B
            => send_messages.size() <= sentpkts <= send_message_end X
            C <= B <= A
            => send_messages.size() <= send_message_end <= sentpkts X
            */
            if (send_message_end >= send_messages.size()){
                // send_messages.size() <= send_message_end <= sentpkts
                // sentpkts <= send_messages.size() <= send_message_end
                // send_messages.size() <= sentpkts <= send_message_end 
                sent_packet_range.second = send_hdrs.back()->pkt_num;
                send_elicit_ack_message_pktnum_new2(last_elicit_ack_pktnum);
                ack_iovec.iov_base = send_ack.data();
                ack_iovec.iov_len = send_ack.size();

                ack_mmsghdr.msg_hdr.msg_iov = &ack_iovec;
                ack_mmsghdr.msg_hdr.msg_iovlen = 1;
                send_messages.push_back(ack_mmsghdr);
                // Send size 1, just a acknowledge packet
                // e.g 0, 1, 2 are applicatoin packet
                // 3 is a elicit acknowledge packet
                send_message_start = send_messages.size() - 1;
                send_message_end = send_messages.size();
            }else{
                if (sentpkts <= send_message_end){
                    // sentpkts <= send_message_end <= send_messages.size()
                    sent_packet_range.second = send_hdrs.at(send_message_end - 1)->pkt_num;
                    send_elicit_ack_message_pktnum_new2(last_elicit_ack_pktnum);
                    ack_iovec.iov_base = send_ack.data();
                    ack_iovec.iov_len = send_ack.size();

                    ack_mmsghdr.msg_hdr.msg_iov = &ack_iovec;
                    ack_mmsghdr.msg_hdr.msg_iovlen = 1;
                    send_messages.insert(send_messages.begin() + send_message_end, ack_mmsghdr);
  
                    send_message_start = send_message_end;
                    send_message_end += 1;
                }else{
                    if (sentpkts >= send_messages.size()){
                        // send_message_end <= send_messages.size() <= sentpkts
                        sent_packet_range.second = send_hdrs.back()->pkt_num;
                        send_elicit_ack_message_pktnum_new2(last_elicit_ack_pktnum);
                        ack_iovec.iov_base = send_ack.data();
                        ack_iovec.iov_len = send_ack.size();

                        ack_mmsghdr.msg_hdr.msg_iov = &ack_iovec;
                        ack_mmsghdr.msg_hdr.msg_iovlen = 1;
                        send_messages.push_back(ack_mmsghdr);
                        // 0, 1, 2 application packet
                        // 3 elicit acknowledge packet
                        send_message_start = send_message_end;
                        send_message_end = send_messages.size();
                    }else{
                        // send_message_end <= sentpkts <= send_messages.size()
                        sent_packet_range.second = send_hdrs.at(sentpkts - 1)->pkt_num;
                        send_elicit_ack_message_pktnum_new2(last_elicit_ack_pktnum);
                        ack_iovec.iov_base = send_ack.data();
                        ack_iovec.iov_len = send_ack.size();

                        ack_mmsghdr.msg_hdr.msg_iov = &ack_iovec;
                        ack_mmsghdr.msg_hdr.msg_iovlen = 1;
                        send_messages.insert(send_messages.begin() + sentpkts, ack_mmsghdr);
                        send_message_start = send_message_end;
                        send_message_end = sentpkts + 1;
                    }
                }
            }
            send_phrase = true;
            start_ = send_message_start;
            written_len_ = send_message_end - send_message_start;
        }

        return written_len_;
    }

    void send_message_complete(size_t err_ = 0, size_t error_sent = 0){
        set_error(err_, error_sent);

        // If err_ == EAGAIN, connection won't emit any new data to application send buffer.
        if (err_ != 0){
            return;
        }
        /*std::cout<<"++++++++++++++++++++++++++++++"<<std::endl;
        for (auto k =send_message_start; k<send_message_end;k++) {
            for (size_t i = 0; i < send_messages[k].msg_hdr.msg_iovlen; ++i) {
                if (send_messages[k].msg_hdr.msg_iov[i].iov_len == 26){
                        std::cout<<"k:"<<k<<" ";
                    for(auto j = 0; j < 26; j++){
                        std::cout << (int)(static_cast<uint8_t*>(send_messages[k].msg_hdr.msg_iov[i].iov_base))[j]<< " ";
                    }
                    std::cout<<std::endl;
                }
            }
        }
	    std::cout<<"------------------------------"<<std::endl;*/
        if(!send_phrase){
            received_packets = 0;
	        partial_send = false;
            return;
        }

        // if (send_partial_signal == true && send_full_signal == false){
        //     return;
        // }

        set_handshake();
        // last_elicit_ack_pktnum = pkt_num_spaces.at(1).updatepktnum();
        send_phrase = true;

        // sent_packet_range.first = send_hdrs.at(0)->pkt_num;
        // auto end_index = std::min((send_message_end - 2), (ssize_t)(send_hdrs.size() - 1));
        // sent_packet_range.second = send_hdrs.at(end_index)->pkt_num;
        //std::cout<<"first:"<<sent_packet_range.first<<", second:"<<sent_packet_range.second<<std::endl;
        if (data_gotten){
            data_gotten = false;
            if (data_buffer.at(current_buffer_pos).left > 0){
                data2buffer(data_buffer.at(current_buffer_pos));
            }
        }
	    /*else{
            last_elicit_ack_pktnum = pkt_num_spaces.at(1).updatepktnum();
        }*/
	    //std::cout<<"last_elicit_ack_pktnum:"<<last_elicit_ack_pktnum<<std::endl;
        

        if (send_message_end >= send_messages.size()){
            send_hdrs.clear();
            send_messages.clear();
            send_iovecs.clear(); 
        }else{
            mmsg_rearrange();
	    //std::cout<<"mmsg_rearrange(), send_messages.size:"<<send_messages.size()<<std::endl;
        }

        
        received_packets = 0;
        partial_send_packets = 0;
        send_message_start = 0;
        //send_buffer.data_clear();
        send_buffer.manage_recovery();
        if (!send_buffer.is_empty()){
            // Add cwnd expectation function as per last real cwnd.
            // Add new packet type: received number to reduce searching cost.
            ssize_t forecast_ = recovery.cwnd_expect();
            if(forecast_ <= 0){
                std::cout<<"[Error] Wrong cwnd!"<<std::endl;
                _Exit(0);
            }
            if (send_messages.size() * MAX_SEND_UDP_PAYLOAD_SIZE < forecast_){
                send_mmsg2(forecast_);
                // std::cout<<"send_mmsg2, send_messages.size:"<<send_messages.size()<<", forecast_:"<<forecast_<<std::endl;
            }
            // else{
            //     last_elicit_ack_pktnum = pkt_num_spaces.at(1).updatepktnum();
            // }
        }
        if (!send_hdrs.empty()){
            last_last_range.first = last_range.first;
            last_range.first = current_range.first;
            current_range.first = next_range.first;
            
            last_last_range.second = last_range.second;
            last_range.second = current_range.second;
            current_range.second = next_range.first + send_message_end - 1;
            next_range.first = next_range.first + send_message_end;

        }else{
            last_last_range.first = last_range.first;
            last_range.first = current_range.first;
            current_range.first = next_range.first;
            
            last_last_range.second = last_range.second;
            last_range.second = current_range.second;
            current_range.second = next_range.second;
            next_range.first = 0;
            next_range.second = 0;
        }

        if (data_buffer.at(current_buffer_pos).left > 0){
            data2buffer(data_buffer.at(current_buffer_pos));
        }
        send_message_start = 0;
        send_message_end = 0;
        //send_buffer.data_clear();
    }

    void mmsg_rearrange(){
        size_t index = 0;
        size_t remove_size = send_message_end;
        // if (congestion_window % MAX_SEND_UDP_PAYLOAD_SIZE == 0){
        //     remove_size = congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + 1;
        // }else{
        //     remove_size = congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + 2;
        // }
        
        //TODO
        // Didn't consider acknowldege in this structure. Rethinkg send_hdrs, send_iovecs.
	    //std::cout<<"send_hdrs.size():"<<send_hdrs.size()<<", remove_size:"<<remove_size<<std::endl;
        if(remove_size >= 0 && remove_size <= send_hdrs.size()){
            send_hdrs.erase(send_hdrs.begin(), send_hdrs.begin() + remove_size - 1);
        }
        //std::cout<<"send_messages.size():"<<send_messages.size()<<", remove_size:"<<remove_size<<std::endl;
        if(remove_size >= 0 && remove_size <= send_messages.size()){
            send_messages.erase(send_messages.begin(), send_messages.begin() + remove_size);
        }
        
	    //std::cout<<"send_iovecs.size():"<<send_iovecs.size()<<", remove_size:"<<remove_size<<std::endl;
        if(remove_size >= 0 && remove_size < send_iovecs.size()){
            send_iovecs.erase(send_iovecs.begin(), send_iovecs.begin() + 2 * (remove_size - 1));
        }

        if(send_iovecs.size() != 2*send_messages.size() && send_messages.size() != send_hdrs.size()){
            std::cout<<"[Error] send_iovecs size not match"<<std::endl;
            _Exit(0);
        }

        send_messages.clear();
        send_messages.resize(send_hdrs.size());
	    //std::cout<<"send_messages.size:"<<send_messages.size()<<", send_iovecs.size:"<<send_iovecs.size()<<", send_hdrs.size:"<<send_hdrs.size()<<std::endl;
        for (auto i = 0; i < send_messages.size(); i++){
            send_messages.at(i).msg_hdr.msg_iov = &send_iovecs.at(2*i);
            send_messages.at(i).msg_hdr.msg_iovlen = 2;
	        /*for(auto j = 0; j < send_iovecs[2*i].iov_len ;j++){
                std::cout<<(int)static_cast<uint8_t*>(send_iovecs[2*i].iov_base)[j]<<" ";
            }
            std::cout<<std::endl;*/
        }

        for(auto i = 0; i < send_hdrs.size(); i++){
            send_hdrs.at(i)->seq += 1;
        }
    }

    ssize_t send_first_message(){
        auto send_seq = 0;
        // consider add ack message at the end of the flow.
        size_t add_one = 0;
        
        if(congestion_window % MAX_SEND_UDP_PAYLOAD_SIZE != 0){
            add_one = 1;
        }

        send_iovecs.resize((congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)* 2);
        send_messages.resize(congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + add_one);

        send_buffer.update_max_data(congestion_window);

        size_t written_len = 0;

        uint64_t last_unack_pktnum = 0;
        uint64_t first_pn = 0;
        uint64_t end_pn = 0;

        bool first_check = false;

        /*
        TODO
        If loss or time out, some data wont be sent and left, the initial index will not be 0.
        It should be the left + 1.
        */ 
        for (auto i = 0; ; ++i){
            if (i == 0){
                send_seq = pkt_num_spaces.at(1).updatepktnum();
                last_elicit_ack_pktnum = send_seq;
            }
            ssize_t out_len = 0; 
            Offset_len out_off = 0;
            bool s_flag = false;
            auto pn = 0;
            auto priority = 0;
            
            pn = pkt_num_spaces.at(0).updatepktnum();

            if(i == 0){
                first_pn = pn;
            }
            end_pn = pn;

            s_flag = send_buffer.emit(send_iovecs[i * 2 + 1], out_len, out_off);

            if (out_len == -1 && s_flag){
                i = i - 1;
            }else{
                //// tmp
                if (out_off == 0){
                    first_check = true;
                }
                /// tmp
                sent_count += 1;
                
                priority = priority_calculation(out_off);
                Type ty = Type::Application;
        
                std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off, send_seq, 0, send_connection_difference, (Packet_num_len)out_len);
                send_hdrs.emplace_back(hdr);
                send_iovecs[2*i].iov_base = (void *)hdr.get();
                send_iovecs[2*i].iov_len = HEADER_LENGTH;

                // std::cout<<"pn:"<<pn<<", offset:"<<out_off<<std::endl;
                if (sent_dic.find(out_off) != sent_dic.end()){
                    if (sent_dic[out_off] != 3 && ((get_dmludp_error() != 11))){
                        sent_dic[out_off] -= 1;
                    }
                }else{
                    sent_dic[out_off] = priority;
                }
                
                record2ack_pktnum.push_back(pn);
                pktnum2offset[pn] = out_off;
                send_messages[i].msg_hdr.msg_iov = &send_iovecs[2 * i];
                send_messages[i].msg_hdr.msg_iovlen = 2;
                written_len += out_len;

                send_unack_packet_record.insert(pn);
            }

            
            if (s_flag){     
                stop_flag = true;
                if ((i+1) <= (congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)){
                    send_iovecs.resize((i + 1) * 2);
                    send_messages.resize(i + 1);
                }
                // std::cout<<"messages.size()"<<messages.size()<<", send_buffer.data.size()"<<send_buffer.data.size()<<std::endl;
                break;
            }

        }

        // if (first_check){
        //     current_range.first = first_pn;
        //     last_range.first = first_pn;
        //     last_last_range.first = first_pn;
        //     current_range.second = end_pn;
        //     last_range.second = end_pn;
        //     last_last_range.second = end_pn;
        // }else{
        //     if (last_cwnd_copy.first != current_range.first && last_cwnd_copy.second != current_range.second){
        //         // There is partial sending before send_mmsg
        //         current_range.second = end_pn;
        //     }else{
        //         // no partial sending.
        //         last_last_range.first = last_range.first;
        //         last_range.first = current_range.first;
        //         current_range.first = first_pn;
                
        //         last_last_range.second = last_range.second;
        //         last_range.second = current_range.second;
        //         current_range.second = end_pn;
        //     }
        // }

        sent_packet_range.first = first_pn;
        sent_packet_range.second = end_pn;
        sent_packet_range_cache1.first = 0;
        sent_packet_range_cache1.second = 0;
        sent_packet_range_cache2.first = 0;
        sent_packet_range_cache2.second = 0;

        auto index = 0;
        auto ioves_size = send_iovecs.size();
        auto message_size = send_messages.size();
        send_iovecs.resize(ioves_size + 1);
        send_messages.resize(message_size + 1);
        // while (true){
            size_t result = 0;
            // result = send_elicit_ack_message_pktnum_new(send_ack, send_seq);
            // if (result == -1){
            //     break;
            // }
	    result = send_elicit_ack_message_pktnum_new2(send_seq);
            send_iovecs[ioves_size].iov_base = send_ack.data();
            send_iovecs[ioves_size].iov_len = send_ack.size();

            send_messages[message_size].msg_hdr.msg_iov = &send_iovecs[ioves_size];
            send_messages[message_size].msg_hdr.msg_iovlen = 1;
        //     ioves_size++;
        //     message_size++;
        //     index++;
        // }

        if (written_len){
            stop_ack = false;
        }

        written_data_len += written_len;
        last_cwnd_copy = current_range;

        // Jun 27th 5:13 pm
        current_partial_range = current_range;

        send_signal = false;
        partial_signal = false;
        partial_signal2 = false;
  	    return written_len;
    }

    ssize_t send_mmsg2(size_t expected_cwnd)
    {
        auto send_seq = 0;
        // consider add ack message at the end of the flow.
        size_t add_one = 0;
        
        if(expected_cwnd % MAX_SEND_UDP_PAYLOAD_SIZE != 0){
            add_one = 1;
        }

        // TODO
        // If loss, reduce the max_data size
        // 
        // if (partial_send_packets == 0){
        //     send_seq = pkt_num_spaces.at(1).updatepktnum();
        //     // Move to send_message_complete()
        //     last_elicit_ack_pktnum = send_seq;
        // }

        if (send_messages.size() * MAX_SEND_UDP_PAYLOAD_SIZE > expected_cwnd){
            return 0;
        }

        size_t initial_index = send_messages.size();
        auto current_cwnd = expected_cwnd - send_messages.size() * MAX_SEND_UDP_PAYLOAD_SIZE;
        send_iovecs.resize((expected_cwnd / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)* 2);
        send_messages.resize(expected_cwnd / MAX_SEND_UDP_PAYLOAD_SIZE + add_one);
        // std::cout<<"sendmsg pkts:"<<(expected_cwnd / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)<<std::endl;
        send_buffer.update_max_data(current_cwnd);
	    // std::cout<<"before : send_messages.size:"<<send_messages.size()<<", send_hdrs.size:"<<send_hdrs.size()<<", send_iovecs.size:"<<send_iovecs.size()<<std::endl;
        size_t written_len = 0;
	    // std::cout<<"initial_index:"<<initial_index<<std::endl;
        uint64_t last_unack_pktnum = 0;
        uint64_t first_pn = 0;
        uint64_t end_pn = 0;

        bool first_check = false;

        /*
        TODO
        1. If loss or time out, some data wont be sent and left, the initial index will not be 0.
        It should be the left + 1.
        2. If timeout,
        */ 
        

        for (ssize_t i = initial_index; ; ++i){
            // TODO
            // If cwnd shrink, process update seq.
            // if (i == initial_index){
            //     send_seq = pkt_num_spaces.at(1).updatepktnum();
            //     // Move to send_message_complete()
            //     last_elicit_ack_pktnum = send_seq;
            //     //////
            // }
            send_seq = last_elicit_ack_pktnum + 1;
            ssize_t out_len = 0; 
            Offset_len out_off = 0;
            bool s_flag = false;
            auto pn = 0;
            auto priority = 0;
            
            pn = pkt_num_spaces.at(0).updatepktnum();

            if(i == 0){
                first_pn = pn;
            }
            end_pn = pn;

            s_flag = send_buffer.emit(send_iovecs[i * 2 + 1], out_len, out_off);

            if (out_len == -1 && s_flag){
                i = i - 1;
            }else{
                //// tmp
                if (out_off == 0){
                    first_check = true;
                }
                /// tmp
                sent_count += 1;
                
                priority = priority_calculation(out_off);
                Type ty = Type::Application;
        
                std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off, send_seq, 0, send_connection_difference, (Packet_num_len)out_len);
                send_hdrs.push_back(hdr);
		        if(2*i == send_iovecs.size()){
                    send_iovecs.resize(2*i + 2);
                    send_messages.resize(i + 1);
                }
                send_iovecs.at(2*i).iov_base = (void *)hdr.get();
                send_iovecs.at(2*i).iov_len = HEADER_LENGTH;

                // Each sequcence coressponds to the packet range.

                //std::cout<<"pn:"<<pn<<", offset:"<<out_off<<", len:"<<(Packet_num_len)out_len<<std::endl;
                if (sent_dic.find(out_off) != sent_dic.end()){
                    if (sent_dic[out_off] != 3 && ((get_dmludp_error() != 11))){
                        sent_dic[out_off] -= 1;
                    }
                }else{
                    sent_dic[out_off] = priority;
                }
                
                // record2ack_pktnum.push_back(pn);
                pktnum2offset[pn] = out_off;
                send_messages[i].msg_hdr.msg_iov = &send_iovecs[2 * i];
                send_messages[i].msg_hdr.msg_iovlen = 2;
                written_len += out_len;

                send_unack_packet_record.insert(pn);
            }

	       // std::cout<<"i:"<<i<<std::endl;        
            if (s_flag){     
		 //       std::cout<<"stop i:"<<i<<std::endl;
                stop_flag = true;
                if ((i+1) <= (expected_cwnd / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)){
                    send_iovecs.resize((i + 1) * 2);
                    send_messages.resize(i + 1);
                }
                // std::cout<<"messages.size()"<<messages.size()<<", send_buffer.data.size()"<<send_buffer.data.size()<<std::endl;
                break;
            }

        }
        next_range.first = first_pn;
        next_range.second = end_pn;
    

        if (written_len){
            stop_ack = false;
        }

        written_data_len += written_len;
        last_cwnd_copy = current_range;

        // Jun 27th 5:13 pm
        current_partial_range = current_range;
	    //std::cout<<"sendmmsg: send_messages.size:"<<send_messages.size()<<", send_hdrs.size:"<<send_hdrs.size()<<", send_iovecs.size:"<<send_iovecs.size()<<std::endl;
        //set_handshake();
        send_signal = false;
        partial_signal = false;
        partial_signal2 = false;
  	    return written_len;
    };

    // ssize_t send_elicit_ack_message_pktnum_new2(uint64_t elicit_acknowledege_packet_number, uint64_t range_){
    //     auto ty = Type::ElicitAck;        
    //     auto preparenum = std::min(range_, record2ack_pktnum.size());
    //     if(record2ack_pktnum.empty()){
    //         return -1;
    //     }

    //     size_t pktlen = 0;

    //     uint64_t start_pktnum = record2ack_pktnum[0];
             
    //     size_t sent_num = std::min(preparenum, MAX_ACK_NUM_PKTNUM);

    //     uint64_t end_pktnum = record2ack_pktnum[sent_num - 1];

    //     if ((last_cwnd_copy.second - last_cwnd_copy.first) >= (current_range.second - current_range.first)){
    //         if ((last_last_range.second - last_last_range.first) >= (last_cwnd_copy.second - last_cwnd_copy.first)){
    //             start_pktnum = last_last_range.first;
    //         }else{
    //             start_pktnum = last_cwnd_copy.first;
    //         }    

    //         if (end_pktnum - start_pktnum >= MAX_ACK_NUM_PKTNUM){
    //             start_pktnum = end_pktnum - MAX_ACK_NUM_PKTNUM + 1;
    //         }
    //     }

    //     auto pn = elicit_acknowledege_packet_number;
	//     // std::cout<<"acknowledge pn:"<<pn<<std::endl;
    //     Header* hdr = new Header(ty, pn, 0, 0, pn, 0, send_connection_difference, pktlen);
    //     // std::cout<<"[Elicit] Elicit acknowledge packet num:"<<pn<<std::endl;
    //     send_ack.resize(HEADER_LENGTH + 2 * sizeof(uint64_t));

    //     hdr->to_bytes(send_ack);
    //     memcpy(send_ack.data() + HEADER_LENGTH, &start_pktnum, sizeof(uint64_t));
    //     memcpy(send_ack.data() + HEADER_LENGTH + sizeof(uint64_t), &end_pktnum, sizeof(uint64_t));
    //     if(sent_num == record2ack_pktnum.size()){
    //         record2ack_pktnum.clear();
    //     }else{
    //         record2ack_pktnum.erase(record2ack_pktnum.begin(), record2ack_pktnum.begin() + sent_num);
    //     }

    //     delete hdr; 
    //     hdr = nullptr; 

    //     send_pkt_duration[pn] = std::make_pair(start_pktnum, end_pktnum);
    //     // std::cout<<"start:"<<start_pktnum<<", end:"<<end_pktnum<<std::endl;
    //     send_range = std::make_pair(start_pktnum, end_pktnum);

    //     pktlen += HEADER_LENGTH;
    //     return pktlen;
    // }
    ssize_t send_elicit_ack_message_pktnum_new2(uint64_t elicit_acknowledege_packet_number, uint64_t range_ = 0){
        auto ty = Type::ElicitAck;        
        // auto preparenum = std::min(range_, record2ack_pktnum.size());

        size_t pktlen = 0;

        // uint64_t start_pktnum = record2ack_pktnum[0];
             
        // size_t sent_num = std::min(preparenum, MAX_ACK_NUM_PKTNUM);

        // uint64_t end_pktnum = record2ack_pktnum[sent_num - 1];

        uint64_t start_pktnum = sent_packet_range.first;
        uint64_t end_pktnum = sent_packet_range.second;

        if ((sent_packet_range_cache1.second - sent_packet_range_cache1.first) >= (sent_packet_range.second - sent_packet_range.first)){
            if ((sent_packet_range_cache2.second - sent_packet_range_cache2.first) >= (sent_packet_range_cache1.second - sent_packet_range_cache1.first)){
                start_pktnum = sent_packet_range_cache2.first;
            }else{
                start_pktnum = sent_packet_range_cache1.first;
            }    

            if (end_pktnum - start_pktnum >= MAX_ACK_NUM_PKTNUM){
                start_pktnum = end_pktnum - MAX_ACK_NUM_PKTNUM + 1;
            }
        }

        auto pn = elicit_acknowledege_packet_number;
	    // std::cout<<"acknowledge pn:"<<pn<<std::endl;
        Header* hdr = new Header(ty, pn, 0, 0, pn, 0, send_connection_difference, pktlen);
	    //std::cout<<"ack send_connection_difference:"<<(int)send_connection_difference<<std::endl;
        // std::cout<<"[Elicit] Elicit acknowledge packet num:"<<pn<<std::endl;
        send_ack.resize(HEADER_LENGTH + 2 * sizeof(uint64_t));

        hdr->to_bytes(send_ack);
        memcpy(send_ack.data() + HEADER_LENGTH, &start_pktnum, sizeof(uint64_t));
        memcpy(send_ack.data() + HEADER_LENGTH + sizeof(uint64_t), &end_pktnum, sizeof(uint64_t));

        delete hdr; 
        hdr = nullptr; 

        send_pkt_duration[pn] = std::make_pair(start_pktnum, end_pktnum);
        // std::cout<<"start:"<<start_pktnum<<", end:"<<end_pktnum<<std::endl;
        send_range = std::make_pair(start_pktnum, end_pktnum);

        pktlen += HEADER_LENGTH;
        return pktlen;
    }

    // send_partial_mmsg works just for last cwnd range packet received.
    ssize_t send_partial_mmsg(
        std::vector<std::shared_ptr<Header>> &hdrs,
        std::vector<struct mmsghdr> &messages, 
        std::vector<struct iovec> &iovecs)
    {
        iovecs.resize(received_packets * 2);
        messages.resize(received_packets);

        partial_send_packets += received_packets; 

        size_t written_len = 0;

        uint64_t last_unack_pktnum = 0;
        auto send_seq = last_elicit_ack_pktnum + 1;
        uint64_t start_pn = 0;
        uint64_t end_pn = 0;

        for (auto i = 0; ; ++i){
            if(i == 0){
                send_buffer.reset_iterator();
            }
            ssize_t out_len = 0; 
            Offset_len out_off = 0;
            bool s_flag = false;
            auto pn = 0;
            auto priority = 0;
            
            pn = pkt_num_spaces.at(0).updatepktnum();
            if(i == 0){
                start_pn = pn;
            }
            end_pn = pn;


            s_flag = send_buffer.emit(iovecs[i * 2 + 1], out_len, out_off);

            if (out_len == -1 && s_flag){
                i = i - 1;
            }else{
                sent_count += 1;
            
                priority = priority_calculation(out_off);
                Type ty = Type::Application;
        
                std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off, send_seq, 0, send_connection_difference, (Packet_num_len)out_len);
                hdrs.push_back(hdr);
                iovecs[2*i].iov_base = (void *)hdr.get();
                iovecs[2*i].iov_len = HEADER_LENGTH;

                // std::cout<<"pn:"<<pn<<", offset:"<<out_off<<std::endl;
                if (sent_dic.find(out_off) != sent_dic.end()){
                    if (sent_dic[out_off] != 3 && ((get_dmludp_error() != 11))){
                        sent_dic[out_off] -= 1;
                    }
                }else{
                    sent_dic[out_off] = priority;
                }
                
                record2ack_pktnum.push_back(pn);
                pktnum2offset[pn] = out_off;
                messages[i].msg_hdr.msg_iov = &iovecs[2 * i];
                messages[i].msg_hdr.msg_iovlen = 2;
                written_len += out_len;

                // Sequcence2range[send_seq].insert(pn);

                send_unack_packet_record.insert(pn);
            }

            
            if (s_flag){  
                if ((i+1)<received_packets){
                    iovecs.resize(i * 2);
                    messages.resize(i);
                }   
                stop_flag = true;
                break;
            }
        }

        // update
        // consider multiple partial send
        if (last_cwnd_copy.first == current_range.first && last_cwnd_copy.second == current_range.second){
            last_last_range.first = last_range.first;
            last_last_range.second = last_range.second;
            last_range.first = current_range.first;
            last_range.second = current_range.second;
    
            current_range.first = start_pn;
            current_range.second = end_pn;

            current_partial_range.first = start_pn;
            current_partial_range.second = end_pn;
        }else{
            current_range.second = end_pn;
            current_partial_range.second = end_pn;
        }

        auto index = 0;

        if (written_len){
            stop_ack = false;
        }

        written_data_len += written_len;
        partial_send = false;
        received_packets = 0;
  	    return written_len;
    };

    // Prepare data.
    ssize_t send_mmsg(
        std::vector<std::shared_ptr<Header>> &hdrs,
        std::vector<struct mmsghdr> &messages, 
        std::vector<struct iovec> &iovecs,
        std::vector<std::vector<uint8_t>> &out_ack)
    {
        auto send_seq = 0;
        // consider add ack message at the end of the flow.
        size_t add_one = 0;
        
        if(congestion_window % MAX_SEND_UDP_PAYLOAD_SIZE != 0){
            add_one = 1;
        }
        iovecs.resize((congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)* 2);
        messages.resize(congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + add_one);
        size_t written_len = 0;

        uint64_t last_unack_pktnum = 0;
        uint64_t first_pn = 0;
        uint64_t end_pn = 0;

        bool first_check = false;

        for (auto i = 0; ; ++i){
            if (i == 0){
                send_seq = pkt_num_spaces.at(1).updatepktnum();
                last_elicit_ack_pktnum = send_seq;
            }
            ssize_t out_len = 0; 
            Offset_len out_off = 0;
            bool s_flag = false;
            auto pn = 0;
            auto priority = 0;
            
            pn = pkt_num_spaces.at(0).updatepktnum();

            if(i == 0){
                first_pn = pn;
            }
            end_pn = pn;

            s_flag = send_buffer.emit(iovecs[i*2+1], out_len, out_off);

            if (out_len == -1 && s_flag){
                i = i - 1;
            }else{
                //// tmp
                if (out_off == 0){
                    first_check = true;
                }
                /// tmp
                sent_count += 1;
                
                priority = priority_calculation(out_off);
                Type ty = Type::Application;
        
                std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off, send_seq, 0, send_connection_difference, (Packet_num_len)out_len);
                hdrs.push_back(hdr);
                iovecs[2*i].iov_base = (void *)hdr.get();
                iovecs[2*i].iov_len = HEADER_LENGTH;

                // Each sequcence coressponds to the packet range.

                // std::cout<<"pn:"<<pn<<", offset:"<<out_off<<std::endl;
                if (sent_dic.find(out_off) != sent_dic.end()){
                    if (sent_dic[out_off] != 3 && ((get_dmludp_error() != 11))){
                        sent_dic[out_off] -= 1;
                    }
                }else{
                    sent_dic[out_off] = priority;
                }
                
                record2ack_pktnum.push_back(pn);
                pktnum2offset[pn] = out_off;
                messages[i].msg_hdr.msg_iov = &iovecs[2 * i];
                messages[i].msg_hdr.msg_iovlen = 2;
                written_len += out_len;

                send_unack_packet_record.insert(pn);
            }

            
            if (s_flag){     
                stop_flag = true;
                /* old
                if ((i+1) <= send_buffer.data.size()){
                */
                if ((i+1) <= (congestion_window / MAX_SEND_UDP_PAYLOAD_SIZE + add_one)){
                    iovecs.resize((i + 1) * 2);
                    messages.resize(i + 1);
                }
                // std::cout<<"messages.size()"<<messages.size()<<", send_buffer.data.size()"<<send_buffer.data.size()<<std::endl;
                break;
            }

        }

        if (first_check){
            current_range.first = first_pn;
            last_range.first = first_pn;
            last_last_range.first = first_pn;
            current_range.second = end_pn;
            last_range.second = end_pn;
            last_last_range.second = end_pn;
        }else{
            if (last_cwnd_copy.first != current_range.first && last_cwnd_copy.second != current_range.second){
                // There is partial sending before send_mmsg
                current_range.second = end_pn;
            }else{
                // no partial sending.
                last_last_range.first = last_range.first;
                last_range.first = current_range.first;
                current_range.first = first_pn;
                
                last_last_range.second = last_range.second;
                last_range.second = current_range.second;
                current_range.second = end_pn;
            }
        }
    

        auto index = 0;
        out_ack.resize((record2ack_pktnum.size() / MAX_ACK_NUM_PKTNUM) + 1);
        auto ioves_size = iovecs.size();
        auto message_size = messages.size();
        iovecs.resize(ioves_size + out_ack.size());
        messages.resize(message_size + out_ack.size());
        while (true){
            size_t result = 0;
            result = send_elicit_ack_message_pktnum_new(out_ack[index], send_seq);
            if (result == -1){
                break;
            }
            iovecs[ioves_size].iov_base = out_ack[index].data();
            iovecs[ioves_size].iov_len = out_ack[index].size();

            messages[message_size].msg_hdr.msg_iov = &iovecs[ioves_size];
            messages[message_size].msg_hdr.msg_iovlen = 1;
            ioves_size++;
            message_size++;
            index++;
        }

        if (written_len){
            stop_ack = false;
        }

        written_data_len += written_len;
        last_cwnd_copy = current_range;

        // Jun 27th 5:13 pm
        current_partial_range = current_range;

        set_handshake();
        send_signal = false;
        partial_signal = false;
        partial_signal2 = false;
  	    return written_len;
    };

    ssize_t send_elicit_ack_message_pktnum_new(std::vector<uint8_t> &out, uint64_t elicit_acknowledege_packet_number){
        auto ty = Type::ElicitAck;        
        auto preparenum = record2ack_pktnum.size();
        if(record2ack_pktnum.empty()){
            return -1;
        }

        size_t pktlen = 0;

        uint64_t start_pktnum = record2ack_pktnum[0];
             
        size_t sent_num = std::min(preparenum, MAX_ACK_NUM_PKTNUM);

        uint64_t end_pktnum = record2ack_pktnum[sent_num - 1];

         if ((last_cwnd_copy.second - last_cwnd_copy.first) >= (current_range.second - current_range.first)){
            if ((last_last_range.second - last_last_range.first) >= (last_cwnd_copy.second - last_cwnd_copy.first)){
                start_pktnum = last_last_range.first;
            }else{
                start_pktnum = last_cwnd_copy.first;
            }    

            if (end_pktnum - start_pktnum >= MAX_ACK_NUM_PKTNUM){
                start_pktnum = end_pktnum - MAX_ACK_NUM_PKTNUM + 1;
            }
        }

        auto pn = elicit_acknowledege_packet_number;
	    // std::cout<<"acknowledge pn:"<<pn<<std::endl;
        Header* hdr = new Header(ty, pn, 0, 0, pn, 0, send_connection_difference, pktlen);
        // std::cout<<"[Elicit] Elicit acknowledge packet num:"<<pn<<std::endl;
        out.resize(HEADER_LENGTH + 2 * sizeof(uint64_t));

        hdr->to_bytes(out);
        memcpy(out.data() + HEADER_LENGTH, &start_pktnum, sizeof(uint64_t));
        memcpy(out.data() + HEADER_LENGTH + sizeof(uint64_t), &end_pktnum, sizeof(uint64_t));
        if(sent_num == record2ack_pktnum.size()){
            record2ack_pktnum.clear();
        }else{
            record2ack_pktnum.erase(record2ack_pktnum.begin(), record2ack_pktnum.begin() + sent_num);
        }

        delete hdr; 
        hdr = nullptr; 

        send_pkt_duration[pn] = std::make_pair(start_pktnum, end_pktnum);
        // std::cout<<"start:"<<start_pktnum<<", end:"<<end_pktnum<<std::endl;
        send_range = std::make_pair(start_pktnum, end_pktnum);

        pktlen += HEADER_LENGTH;
        return pktlen;
    }

    size_t get_error_sent(){
        return dmludp_error_sent;
    }

    size_t get_dmludp_error(){
        return dmludp_error;
    }

    void set_error(size_t err, size_t application_sent){
        dmludp_error = err;
	    if (application_sent != 0){
            dmludp_error_sent += application_sent;
        }else{
            dmludp_error_sent = 0;
        }
    }

    // If transmission complete
    bool transmission_complete(){
        bool result = true;
        for (auto i : data_buffer){
            if (i.left != 0){
                result = false;
            }
        }

        bool test_result = !std::any_of(data_buffer.begin(), data_buffer.end(), [](const sbuffer& i) {
            return i.left != 0;
        });
        
        if (test_result != result){
            std::cout<<"test_result != result"<<std::endl;
            _Exit(0);
        }

        if (!send_buffer.is_empty()){
            result = false;
        }
        
        if (result == true){
            data_buffer.clear();
            current_buffer_pos = 0;
        }
        // if(result){
        //     std::cout<<"transmission complete"<<std::endl;
        // }
        return result;
    }

    //Send single packet
    size_t send_data(uint8_t* out){
        size_t total_len = HEADER_LENGTH;

        uint64_t psize = 0;

        auto ty = write_pkt_type(); 

        if (ty == Type::Handshake && server){
            // out = handshake_header;
            memcpy(out, handshake_header, HEADER_LENGTH);
            set_handshake();
        }

        if (ty == Type::Handshake && !server){
            // out = handshake_header;
            memcpy(out, handshake_header, HEADER_LENGTH);
            feed_back = false;
            initial = true;
        }

        if (ty == Type::ACK){
            feed_back = false;
            psize = (uint64_t)(receive_result.size()) + 2 * sizeof(uint64_t);
            Header* hdr = new Header(ty, send_num, 0, 0, send_num, 1, receive_connection_difference, psize);
            if (difference_flag){
                uint8_t tmp = receive_connection_difference - 1;
                hdr->difference = tmp;
            }
            memcpy(out, hdr, HEADER_LENGTH);
            memcpy(out + HEADER_LENGTH, &receive_range.first, sizeof(uint64_t));
            memcpy(out + HEADER_LENGTH + sizeof(uint64_t), &receive_range.second, sizeof(uint64_t));
            memcpy(out + HEADER_LENGTH + 2 * sizeof(uint64_t), receive_result.data(), receive_result.size());
            receive_result.clear();
            difference_flag = false;
            delete hdr; 
            hdr = nullptr; 
        }      

        if (ty == Type::Fin){
            memcpy(out, fin_header, HEADER_LENGTH);
            return total_len;
        }

        // handshake = std::chrono::high_resolution_clock::now();

        total_len += (size_t)psize;

        return total_len;
    };

    void reset_single_receive_parameter(Acknowledge_sequence_len pkt_seq){
        if (send_num != pkt_seq){
            send_num = pkt_seq;
            // single_acknowlege_time = 0;
        }
    }


    size_t send_data_acknowledge(uint8_t* src, size_t src_len){
        auto ty = Type::ACK;
        auto start = current_loop_min;
        auto end = current_loop_max;
        uint64_t psize = end - start + 1;
        Header* hdr = new Header(ty, send_num, 0, 0, send_num, 0, receive_connection_difference, psize);
        if(difference_flag){
            uint8_t tmp = receive_connection_difference - 1;
            hdr->difference = tmp;
        }
        memcpy(src, hdr, HEADER_LENGTH);
        memcpy(src + HEADER_LENGTH, &start, sizeof(Packet_num_len));
        memcpy(src + HEADER_LENGTH + sizeof(Packet_num_len), &end, sizeof(Packet_num_len));

        for (auto pn = start; pn <= end; pn++){
            auto off_ = HEADER_LENGTH + 2 * sizeof(Packet_num_len) + pn - start;
            if(difference_flag){
                src[off_] = 0;
            }else{
                if(receive_pktnum2offset.find(pn) == receive_pktnum2offset.end()){
                    src[off_] = 1;
                }else{
                    src[off_] = 0;
                }
            }
        }
        psize += HEADER_LENGTH + 2 * sizeof(Packet_num_len);
        difference_flag = false;
        return psize;
    }

    size_t send_data_stop(uint8_t* out){ 
        size_t total_len = HEADER_LENGTH;

        auto pn = pkt_num_spaces.at(1).updatepktnum();

        uint64_t offset = 0;
        uint8_t priority = 0;
        uint64_t psize = 0;

        auto ty = Type::Stop; 

        Header* hdr = new Header(ty, pn, offset, priority, 0, 0, send_connection_difference, psize);

        memcpy(out, hdr, HEADER_LENGTH);
        delete hdr; 
        hdr = nullptr; 

        return total_len;
    };

    size_t send_data_handshake(uint8_t* out){     
        size_t total_len = HEADER_LENGTH;
        memcpy(out, handshake_header, HEADER_LENGTH);
        set_handshake();
        return total_len;
    };

    bool is_stopped(){
        return stop_flag && stop_ack && initial;
    };

    // Check if fixed length of first entry in received buffer exist.
    bool check_first_entry(size_t check_len){
        auto fst_len = rec_buffer.first_item_len(check_len);
        if (fst_len != check_len){
            return false;
        }
        return true;
    };

    void recv_padding(size_t total_len){
        rec_buffer.data_padding(total_len);
    }

    size_t read(uint8_t* out, bool iscopy, size_t output_len = 0){
        return rec_buffer.emit(out, iscopy, output_len);
    };

    uint64_t max_ack() {
        return rec_buffer.max_ack();
    };

    bool has_recv(){
        return rec_buffer.is_empty();
    }
    
    size_t recv_len(){
        return rec_buffer.length();
    }

    uint8_t priority_calculation(uint64_t off){
        auto real_index = (uint64_t)(off / MAX_SEND_UDP_PAYLOAD_SIZE);
        if (real_index >= norm2_vec.size()){
            std::cout<<"out of range"<<std::endl;
        }
        return norm2_vec[real_index];
    };

    void reset(){
        norm2_vec.clear();
        send_buffer.clear();
    };

    void check_loss_pktnum(uint8_t* src, size_t src_len){
        uint64_t start = *reinterpret_cast<uint64_t*>(src + HEADER_LENGTH);
        uint64_t end = *reinterpret_cast<uint64_t*>(src + HEADER_LENGTH + sizeof(uint64_t));
	    uint8_t send_difference = reinterpret_cast<Header *>(src)->difference;
        uint8_t trail_send_difference = send_difference + 1;
        // Used to debug.
        // std::map<uint64_t, uint8_t> ack_record;
        receive_range = std::make_pair(start, end);
	    if(trail_send_difference == receive_connection_difference){
            difference_flag = true;
            if(end - start < MAX_SEND_UDP_PAYLOAD_SIZE){
                receive_result.resize((end - start + 1), 0);
            }
        }else{
            for (auto pn = start; pn <= end; pn++){
                if (receive_pktnum2offset.find(pn) != receive_pktnum2offset.end()){
                    receive_result.push_back(0);
                    // ack_record[pn] = 0;
                }else{
                    receive_result.push_back(1);
                    // ack_record[pn] = 1;
                }
            }
        }
        /*for (auto pn = start; pn <= end; pn++){
            if (receive_pktnum2offset.find(pn) != receive_pktnum2offset.end()){
                receive_result.push_back(0);
                // ack_record[pn] = 0;
            }else{
                receive_result.push_back(1);
                // ack_record[pn] = 1;
            }          
        }*/
        
        // RRD.add_acknowledeg_info(send_num, std::move(ack_record));
    }

    void set_handshake(){
        handshake = std::chrono::high_resolution_clock::now();
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(handshake.time_since_epoch()).count();
        //std::cout << "send ts: " << now_ns << " ns" << std::endl;
        can_send = false;
        partial_send = false;
        received_packets = 0;
        partial_send_packets = 0;
    };

    double get_rtt() {
        return rto.count();
    };   

    /// Returns true if the connection handshake is complete.
    bool is_established(){
        return handshake_completed;
    };

    /// Returns true if the connection is closed.
    ///
    /// If this returns true, the connection object can be dropped.
    bool is_closed() {
        return closed;
    };
    
    Type write_pkt_type(){
        // let now = Instant::now();
        if (rtt.count() == 0 && is_server == true){
            handshake_completed = true;
            return Type::Handshake;
        }

        if (handshake_confirmed == false && is_server == false){
            handshake_confirmed = true;
            return Type::Handshake;
        }

        if (recv_flag == true){
            recv_flag = false;
            return Type::ACK;
        }

        if ((sent_count % ELICIT_FLAG == 0 && sent_count > 0) || stop_flag == true){
            sent_count = 0;
            return Type::ElicitAck;
        }


        if (rtt.count() != 0){
            return Type::Application;
        }

        if (send_buffer.data.empty()){
            return Type::Stop;
        }

        return Type::Unknown;
    };
    
};

const uint8_t Connection::handshake_header[] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const uint8_t Connection::fin_header[] = {7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}


