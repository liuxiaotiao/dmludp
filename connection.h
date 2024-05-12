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

// const size_t MAX_ACK_NUM = 160;

const size_t MAX_ACK_NUM_PKTNUM = 1350;

const size_t ELICIT_FLAG = 8;

const double CONGESTION_THREAHOLD = 0.01;

// The default max_datagram_size used in congestion control.
const size_t MAX_SEND_UDP_PAYLOAD_SIZE = 1350;

const double alpha = 0.875;

const double beta = 0.25;

const uint64_t MIN_ACK_RANGE_DIFFERENCE = 10;

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

    // Whether the connection was timed out
    bool timed_out;

    bool server;

    struct sockaddr_storage localaddr;

    struct sockaddr_storage peeraddr;
    
    size_t written_data;

    bool stop_flag;

    bool stop_ack;

    std::unordered_map<uint64_t, uint8_t> prioritydic;

    // Record sent_pkt number
    std::vector<uint64_t> sent_pkt;

    std::vector<uint64_t> record_send;

    // Record sent packet offset
    std::vector<uint64_t> record2ack;

    // Record sent application packet num
    std::vector<uint64_t> record2ack_pktnum;

    // Key: sent packet number, value: correspoind offset
    std::map<uint64_t, uint64_t> pktnum2offset;

    // map for received application pktnum and corresponding offset
    std::map<uint64_t, uint64_t> receive_pktnum2offset;

    uint64_t start_receive_offset;

    std::vector<uint8_t> receive_result;

    std::unordered_map<uint64_t, uint8_t> recv_dic;

    // store data
    std::vector<uint8_t> send_data_buf;

    // store norm2 for every 256 bits float
    // Note: It refers to the priorty of each packet.
    std::vector<uint8_t> norm2_vec;

    size_t record_win;

    uint64_t total_offset;
    size_t rx_length;

    bool recv_flag;

    std::unordered_map<uint64_t, uint8_t> recv_hashmap;

    bool feed_back;

    size_t ack_point;

    uint64_t max_off;

    uint64_t send_num;

    size_t high_priority;

    size_t sent_number;

    std::unordered_map<uint64_t, uint64_t> sent_dic;

    bool bidirect;

    Recovery recovery;

    std::array<PktNumSpace, 2> pkt_num_spaces;

    std::chrono::nanoseconds rtt;

    std::chrono::nanoseconds srtt;

    std::chrono::nanoseconds rto;

    std::chrono::nanoseconds rttvar;

    std::chrono::nanoseconds send_preparation;
    
    std::chrono::high_resolution_clock::time_point handshake;

    std::set<uint64_t> ack_set;

    SendBuf send_buffer;

    RecvBuf rec_buffer;

    Received_Record_Debug RRD;

    ///// 1/28/204
    // Initial elicit_ack number and all retransmission elicit ack number.
    std::map<uint64_t, std::vector<uint64_t>> keyToValues;

    // Retransmission elicit ack number and its initial elicit ack number.
    std::map<uint64_t, uint64_t> valueToKeys;

    std::unordered_map<uint64_t, std::pair<std::vector<uint8_t>, std::chrono::high_resolution_clock::time_point>> timeout_ack;

    // Date: 7th Jan 2024
    std::vector<sbuffer> data_buffer;

    size_t current_buffer_pos;

    bool initial;

    // Used to control normal message sending.
    bool waiting_flag;

    // Record how many data sent at once.
    size_t written_data_len;

    // total data sent after one get data.
    size_t written_data_once;

    // Record errno
    size_t dmludp_error;

    // Used to record how many packet has been sent before EAGAIN
    size_t dmludp_error_sent;

    std::unordered_map<uint64_t, std::pair<std::vector<uint8_t>, std::chrono::high_resolution_clock::time_point>> retransmission_ack;

    // Record first application packet number in each cwnd
    uint64_t last_application_pktnum;

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

    Connection(sockaddr_storage local, sockaddr_storage peer, Config config, bool server):    
    recv_count(0),
    sent_count(0),
    is_server(server),
    handshake_completed(false),
    handshake_confirmed(false),
    closed(false),
    timed_out(false),
    server(server),
    localaddr(local),
    peeraddr(peer),
    written_data(0),
    stop_flag(true),
    stop_ack(true),
    waiting_flag(false),
    prioritydic(),
    sent_pkt(),
    recv_dic(),
    send_data_buf(),
    norm2_vec(),
    record_win(0),
    total_offset(0),
    recv_flag(false),
    feed_back(false),
    ack_point(0),
    max_off(0),
    send_num(0),
    high_priority(0),
    sent_number(0),
    rtt(0),
    srtt(0),
    rto(0),
    rttvar(0),
    handshake(std::chrono::high_resolution_clock::now()),
    bidirect(true),
    initial(false),
    current_buffer_pos(0),
    retransmission_ack(),
    written_data_len(0),
    written_data_once(0),
    dmludp_error(0),
    rx_length(0),
    dmludp_error_sent(0),
    start_receive_offset(0),
    last_application_pktnum(0),
    send_connection_difference(0),
    receive_connection_difference(1),
    current_loop_min(0),
    current_loop_max(0),
    send_connection_difference(0),
    receive_connection_difference(1)
    {};

    ~Connection(){

    };

    void update_rtt() {
        auto arrive_time = std::chrono::high_resolution_clock::now();
        rtt = arrive_time - handshake;    
        auto tmp_srtt = std::chrono::duration<double, std::nano>(srtt.count() * alpha + (1 - alpha) * rtt.count());
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_srtt);
        auto diff = srtt - rtt;
        auto tmp_rttvar = std::chrono::duration<double, std::nano>((1 - beta) * rttvar.count() + beta * std::abs(diff.count()));
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_rttvar);
        rto = srtt + 4 * rttvar;
    };

    void set_rtt(uint64_t inter){
        if (bidirect){
            rtt = std::chrono::nanoseconds(inter);
        }
        if (srtt.count() == 0){
            srtt = rtt;
        }
        if (rttvar.count() == 0){
            rttvar = rtt;
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
    }

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
            update_rtt();
            handshake_completed = true;
            initial = true;
        }
        
        //If receiver receives a Handshake packet, it will be papred to send a Handshank.
        if (pkt_ty == Type::Handshake && !is_server){
            handshake_confirmed = false;
            feed_back = true;
        }
        
        // All side can send data.
        if (pkt_ty == Type::ACK){
            process_ack(src, src_len);
            if (ack_set.size() == 0){
                stop_ack = true;
            }
	    }

        if (pkt_ty == Type::ElicitAck){
            recv_flag = true;
            // std::cout<<"[Receive] ElicitAck packet number:"<<hdr->pkt_num<<std::endl;
            // std::vector<uint8_t> subbuf(buf.begin() + 1, buf.begin()+ 1 + sizeof(uint64_t));
            // send_num = convertToUint64(subbuf);
            send_num = pkt_num;
            // std::vector<uint8_t> checkbuf(buf.begin() + 26, buf.end());
            check_loss_pktnum(src, src_len);
            feed_back = true;
        }

        if (pkt_ty == Type::Application){
            // std::cout<<"[Debug] application offset:"<<pkt_offset<<", pn:"<<pkt_num<<std::endl;
            if (receive_pktnum2offset.find(pkt_num) != receive_pktnum2offset.end()){
                std::cout<<"[Error] Duplicate application packet"<<std::endl;
                _Exit(0);
            }
            
            // RRD.add_offset_and_pktnum(hdr->pkt_num, hdr->offset, hdr->pkt_length);
            if (pkt_offset == 0){
                clear_recv_setting();
            }
            if (pkt_num > current_loop_max){
                current_loop_max = pkt_num;
            }

            if (pkt_num < current_loop_min){
                current_loop_min = pkt_num;
            }
            send_num = pkt_seq;

            // Debug
            if (recv_dic.find(pkt_offset) != recv_dic.end()){
                // std::cout<<"[Error] same offset:"<<pkt_offset<<std::endl;
                // RRD.show();
                // _Exit(0);
                receive_pktnum2offset.insert(std::make_pair(pkt_num, pkt_offset));
                // Debug
                recv_dic.insert(std::make_pair(pkt_offset, pkt_priorty));
            }else{
                recv_count += 1;
                // optimize to reduce copy time.
                rec_buffer.write(src + HEADER_LENGTH, pkt_len, pkt_offset);
                receive_pktnum2offset.insert(std::make_pair(pkt_num, pkt_offset));
                // Debug
                recv_dic.insert(std::make_pair(pkt_offset, pkt_priorty));
            }
                     
        }

        // In dmludp.h
        if (pkt_ty == Type::Stop){
            stop_flag = false;
            return 0;
        }

        return read_;
    };

    bool send_ack(){
        return feed_back;
    };

    void update_receive_parameter(){
        current_loop_min = current_loop_max + 1;
    }


    // remove unnessary vectore construct
    void process_ack(uint8_t* src, size_t src_len){
        Packet_num_len pkt_num;
        Priority_len pkt_priorty;
        Offset_len pkt_offset;
        Acknowledge_sequence_len pkt_seq;
        Acknowledge_time_len pkt_ack_time;
        Difference_len pkt_difference;
        Packet_len pkt_len;
        auto pkt_ty = header_info(src, src_len, pkt_num, pkt_priorty, pkt_offset, pkt_seq, pkt_ack_time, pkt_difference, pkt_len);

        auto first_pkt = *(uint64_t *)(src + HEADER_LENGTH);
        auto end_pkt = *(uint64_t *)(src + HEADER_LENGTH + sizeof(uint64_t));

        if (ack_set.empty()){
            stop_ack = true;
            return;
        }

        auto received_ack = pkt_num;
        
        auto initial_ack = valueToKeys.find(received_ack);
        
        auto check_ack = retransmission_ack.find(received_ack);
        if(check_ack != retransmission_ack.end()){
            handshake = retransmission_ack.at(pkt_num).second;
        }else{
            auto timeout_check = timeout_ack.find(received_ack);
            if(timeout_check != timeout_ack.end()){
                handshake = timeout_ack.at(pkt_num).second;
            }else{
                return;
            }
        }
        update_rtt();

        auto last_index = keyToValues[valueToKeys[received_ack]].back();
        uint64_t start_pn = send_pkt_duration[last_index].first;
        uint64_t end_pn = send_pkt_duration[last_index].second;
        
        uint64_t ini = 0;
        // std::cout<<"retransmission_ack"<<std::endl;
        // for (auto it = retransmission_ack.begin(); it != retransmission_ack.end(); ++it) {
        //     std::cout << "Key: " << it->first << std::endl;
        // }
        // std::cout<<"send_pkt_duration"<<std::endl;
        // for (auto it = send_pkt_duration.begin(); it != send_pkt_duration.end(); ++it) {
        //     std::cout << "Key: " << it->first << std::endl;
        // }
        if (pkt_ack_time == 1 || (end_pkt - last_application_pktnum) < MIN_ACK_RANGE_DIFFERENCE){
            if (initial_ack != valueToKeys.end()){
                ini = valueToKeys[received_ack];
                ack_set.erase(ini);
                for (int key : keyToValues[ini]) {
                    retransmission_ack.erase(key);
                    send_pkt_duration.erase(key);
                    valueToKeys.erase(key);
                    timeout_ack.erase(key);
                }
            }else{
                return;
            }
            keyToValues.erase(ini);
        }
        
        // auto result = send_pkt_duration.erase(received_ack);

        // std::vector<uint8_t> unackbuf(buf.begin() + 26, buf.begin() + 26 + hd->pkt_length);

        size_t len = src_len - HEADER_LENGTH;
        size_t start = 0;
        float weights = 0;        
        auto real_index = 0;
        // bool non_sent = true;
        for (auto check_pn = start_pn ; check_pn <= end_pn ; check_pn++){   
            // if (check_pn == start_pn){
            //     std::cout<<"[Debug] ACK first packet num:"<<check_pn<<std::endl;
            // }   
            // auto real_index = check_pn - start_pn + HEADER_LENGTH;
            uint8_t priority = 1;
            if (check_pn >= first_pkt || check_pn <= end_pkt){
                auto pkt_index = HEADER_LENGTH + 2 * sizeof(uint64_t) + check_pn - first_pkt;
                priority = src[pkt_index];
            }
            
            auto unack = pktnum2offset[check_pn];
            if (sent_dic.find(unack) != sent_dic.end()){
                if (sent_dic.at(unack) == 0){
                    send_buffer.ack_and_drop(unack);
		            // std::cout<<" remove condition 1"<<std::endl;
                    // non_sent = true;
                }
            }else{
                continue;
            }

            if (priority != 0){
                // convert the result of priority_calculation to uint64
                priority = priority_calculation(unack);
            }

            if (priority == 1){
                weights += 0.15;
            }else if (priority == 2) {
                weights += 0.2;
            }else if (priority == 3) {
                weights += 0.25;
            }else{
                send_buffer.ack_and_drop(unack);
                // non_sent = true;
		        // std::cout<<" remove condition 2"<<std::endl;
            }
            auto real_priority = priority_calculation(unack);
            if (priority != 0 && real_priority == 3){
                high_priority += 1;
            }
            
            if (pkt_ack_time == 1 || (end_pkt - last_application_pktnum) < MIN_ACK_RANGE_DIFFERENCE){
                if ((real_index + 1) % 8 == 0 || check_pn == end_pn){
                    double pnum = (real_index + 1) % 8;
                    if (pnum == 0){
                        pnum = 8;
                    }
                    recovery.update_win(weights, pnum);
                    weights = 0;
                }
            }

            real_index++;
            // if (!non_sent){
            //     std::cout<<"[Loss] pn:"<<check_pn<<" not receive"<<std::endl;  
            // }

            // if (check_pn == end_pn){
            //     std::cout<<"[Debug] ACK last packet num:"<<check_pn<<std::endl;
            // }
        }
        if (send_buffer.pos == 0){
            // std::cout<<std::endl;
            // std::cout<<"[Before] send_buffer.size()"<<send_buffer.data.size()<<std::endl;
            send_buffer.recv_and_drop();
            // std::cout<<"[After] send_buffer.size()"<<send_buffer.data.size()<<std::endl;
            // std::cout<<std::endl;
        }    
    }


    uint8_t findweight(uint64_t unack){
        return prioritydic.at(unack);
    };

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

    // nwrite() is used to write data to congestion control window
    // return represents if current_buffer_pos should add 1.
    ssize_t nwrite(sbuffer &send_data, size_t congestion_window) {
        if (send_buffer.data.empty()){
            size_t off_len = 0;
            auto toffset = send_data.sent() % MAX_SEND_UDP_PAYLOAD_SIZE;
            off_len = (size_t)MAX_SEND_UDP_PAYLOAD_SIZE - toffset;
            auto result = send_buffer.write(send_data.src, send_data.sent(), send_data.left, congestion_window, off_len);
            return result;
        }else{
            size_t off_len = 0;
            
            auto result = send_buffer.write(send_data.src, send_data.sent(), send_data.left, congestion_window, off_len);
            return result;
        }

    };
    
    // Used to get pointer owner and length
    // get_data() is used after get(op) in gloo.
    bool get_data(struct iovec* iovecs, int iovecs_len, const std::vector<std::vector<uint8_t>> &priotity_list = {}){
        bool completed = true;
        written_data_once = 0;
        written_data_len = 0;
	    dmludp_error_sent = 0;
        record_send.clear();
        sent_dic.clear();
        pktnum2offset.clear();
        ack_point = 0;

        if (!send_pkt_duration.empty()){
            std::cout<<"[Error] send_pkt_duration is not empty!"<<std::endl;
            _Exit(0);
        }

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

        return completed;
    }

    size_t get_once_data_len(){
        return written_data_once;
    }

    void clear_sent_once(){
        written_data_once = 0;
    }

    ssize_t send_mmsg(
        std::vector<std::shared_ptr<Header>> &hdrs,
        std::vector<struct mmsghdr> &messages, 
        std::vector<struct iovec> &iovecs,
        std::vector<std::vector<uint8_t>> &out_ack)
    {
        auto sbuf = data_buffer.at(current_buffer_pos);
        
        ssize_t written_len = 0;

        if(!retransmission_ack.empty()){
            return -1;
        }

        auto start = std::chrono::high_resolution_clock::now();
        size_t congestion_window = 0;
        auto high_ratio = 0.0;
        if (high_priority == 0 && sent_number){
            high_ratio = 0;
        }else{
            high_ratio = (double)high_priority  / (double)sent_number;
        }
        high_priority = 0;
        sent_number = 0;
        if (high_ratio > CONGESTION_THREAHOLD){
            congestion_window = recovery.rollback();
        }else{
            congestion_window = recovery.cwnd();
        };
        record_win = congestion_window;
        
        for (auto i = current_buffer_pos ; i < data_buffer.size() ;){
            auto wlen = nwrite(data_buffer.at(current_buffer_pos), congestion_window);
            if (wlen == -2){
                written_len = 0;
                break;
            }
            written_len += wlen;
            if (data_buffer[current_buffer_pos].left == 0 && (current_buffer_pos == data_buffer.size() - 1))
                break;
            if (data_buffer.at(current_buffer_pos).sent() == data_buffer.at(current_buffer_pos).len && (current_buffer_pos < data_buffer.size())){
                current_buffer_pos += 1;
            }
            if (written_len >= congestion_window)
                break;
            
            if (send_buffer.cap()<=0){
                break;
            }
        }
        
        // auto send_seq = pkt_num_spaces.at(1).updatepktnum();
        auto send_seq = 0;
        // consider add ack message at the end of the flow.
        iovecs.resize((send_buffer.data.size()) * 2);
        messages.resize(send_buffer.data.size());
        std::vector<uint64_t> ack_seq_vector;
        for (auto i = 0; ; ++i){
            if (i % MAX_ACK_NUM_PKTNUM == 0){
                send_seq = pkt_num_spaces.at(1).updatepktnum();
                ack_seq_vector.push_back(send_seq);
            }
            Packet_num_len out_len = 0; 
            Offset_len out_off = 0;
            bool s_flag = false;
            auto pn = 0;
            auto priority = 0;
            
            pn = pkt_num_spaces.at(0).updatepktnum();

            s_flag = send_buffer.emit(iovecs[i*2+1], out_len, out_off);
            out_off -= (uint16_t)out_len;
            sent_count += 1;
            sent_number += 1;
            
            priority = priority_calculation(out_off);
            Type ty = Type::Application;

            last_application_pktnum = pn;
    
            std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off, send_seq, 0, send_connection_difference, out_len);
            hdrs.push_back(hdr);
            iovecs[2*i].iov_base = (void *)hdr.get();
            iovecs[2*i].iov_len = HEADER_LENGTH;
            
            auto offset = out_off;
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


            if (s_flag){     
                stop_flag = true;
                if ((i+1) < send_buffer.data.size()){
                    iovecs.resize((i + 1) * 2);
                    messages.resize(i + 1);
                }
                auto end = std::chrono::high_resolution_clock::now();
                send_preparation = end - start;
                break;
            }

        }
        // auto ack_index = 0;
        // while (true){
        //     auto result = generate_elicit_ack_message(ack_seq_vector, ack_index);
        //     if (result == false){
        //         break;
        //     }
        //     ack_index++;
        // }

        auto index = 0;
        out_ack.resize((record2ack_pktnum.size() / MAX_ACK_NUM_PKTNUM) + 1);
        auto ioves_size = iovecs.size();
        auto message_size = messages.size();
        iovecs.resize(ioves_size + out_ack.size());
        messages.resize(message_size + out_ack.size());
        while (true){
            auto result = send_elicit_ack_message_pktnum_new(out_ack[index], send_seq);
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

  	    return written_len;
    };

    ssize_t send_elicit_ack_message_pktnum(std::vector<uint8_t> &out, uint64_t elicit_acknowledege_packet_number){
        auto ty = Type::ElicitAck;        
        auto preparenum = record2ack_pktnum.size();
        if(record2ack_pktnum.empty()){
            return -1;
        }

        size_t pktlen = 0;

        uint64_t start_pktnum = record2ack_pktnum[0];
        
        size_t sent_num = std::min(preparenum, MAX_ACK_NUM_PKTNUM);

        uint64_t end_pktnum = record2ack_pktnum[sent_num - 1];

        auto pn = elicit_acknowledege_packet_number;
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
        ack_set.insert(pn);
        keyToValues[pn].push_back(pn);
        valueToKeys[pn] = pn;
        ack_point += sent_num;
        std::vector<uint8_t> wait_ack(out.begin()+ HEADER_LENGTH, out.end());
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        retransmission_ack[pn] = std::make_pair(wait_ack, now);
        send_pkt_duration[pn] = std::make_pair(start_pktnum, end_pktnum);
        // std::cout<<"retransmission_ack.size:"<<retransmission_ack.size()<<" send_pkt_duration.size:"<<send_pkt_duration.size()<<std::endl;

        pktlen += HEADER_LENGTH;
        return pktlen;
    }

    size_t get_error_sent(){
        return dmludp_error_sent;
    }

    bool generate_elicit_ack_message(std::vector<uint64_t> ack_seq_vector, int index_){
        auto ty = Type::ElicitAck;        
        auto preparenum = record2ack_pktnum.size();
        if(record2ack_pktnum.empty()){
            return false;
        }

        size_t pktlen = 0;

        uint64_t start_pktnum = record2ack_pktnum[0];
        
        size_t sent_num = std::min(preparenum, MAX_ACK_NUM_PKTNUM);

        uint64_t end_pktnum = record2ack_pktnum[sent_num - 1];

        auto pn = ack_seq_vector[index_];
        // std::cout<<"[Elicit] Elicit acknowledge packet num:"<<pn<<std::endl;

        if(sent_num == record2ack_pktnum.size()){
            record2ack_pktnum.clear();
        }else{
            record2ack_pktnum.erase(record2ack_pktnum.begin(), record2ack_pktnum.begin() + sent_num);
        }

        ack_set.insert(pn);
        keyToValues[pn].push_back(pn);
        valueToKeys[pn] = pn;
        ack_point += sent_num;
        std::vector<uint8_t> wait_ack(2 * sizeof(uint64_t));
        memcpy(wait_ack.data(), &start_pktnum, sizeof(uint64_t));
        memcpy(wait_ack.data() + sizeof(uint64_t), &end_pktnum, sizeof(uint64_t));
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        retransmission_ack[pn] = std::make_pair(wait_ack, now);
        send_pkt_duration[pn] = std::make_pair(start_pktnum, end_pktnum);
        // std::cout<<"retransmission_ack.size:"<<retransmission_ack.size()<<" send_pkt_duration.size:"<<send_pkt_duration.size()<<std::endl;

        pktlen += HEADER_LENGTH;
        return true;
    }

    // ssize_t send_msg(std::vector<uint8_t> &padding, 
    //     std::vector<struct msghdr> &messages, 
    //     std::vector<struct iovec> &iovecs)
    // {
    //     auto sbuf = data_buffer.at(current_buffer_pos);
    //     ssize_t written_len = 0;
    //     if(!retransmission_ack.empty()){
    //         return -1;
    //     }
    //     if (get_dmludp_error() != 11){
    //         size_t congestion_window = 0;
    //         auto high_ratio = 0.0;
    //         if (high_priority == 0 && sent_number){
    //             high_ratio = 0;
    //         }else{
    //             high_ratio = (double)high_priority  / (double)sent_number;
    //         }
    //         high_priority = 0;
    //         sent_number = 0;
    //         if (high_ratio > CONGESTION_THREAHOLD){
    //             congestion_window = recovery.rollback();
    //         }else{
    //             congestion_window = recovery.cwnd();
    //         };
    //         record_win = congestion_window;

            
    //         for (auto i = current_buffer_pos ; i < data_buffer.size() ;){
    //             auto wlen = nwrite(data_buffer.at(current_buffer_pos), congestion_window);
    
    //             if (wlen == -2){
    //                 written_len = 0;
    //                 break;
    //             }
    //             written_len += wlen;
    //             if (data_buffer[current_buffer_pos].left == 0 && (current_buffer_pos == data_buffer.size() - 1))
    //                 break;
    //             if (data_buffer.at(current_buffer_pos).sent() == data_buffer.at(current_buffer_pos).len && (current_buffer_pos < data_buffer.size())){
    //                 current_buffer_pos += 1;
    //             }
    //             if (written_len >= congestion_window)
    //                 break;
                
    //             if (send_buffer.cap()<=0){
    //                 break;
    //             }
    //         }
    //     }else{
    //         send_buffer.sent = 0;
    //     }


    //     // consider add ack message at the end of the flow.
    //     iovecs.resize(send_buffer.data.size() * 2);
    //     messages.resize(send_buffer.data.size());

    //     // unlock memory allocation, and consider move this to function parameter.
    //     std::vector<std::shared_ptr<Header>> hdrs;
    //     for (auto i = 0; ; ++i){
    //         size_t out_len = 0; 
    //         uint64_t out_off = 0;
    //         bool s_flag = false;
    //         auto pn = 0;
	//         auto priority = 0;
	//         if (get_dmludp_error() == 0){
    //             s_flag = send_buffer.emit(iovecs[i*2+1], out_len, out_off);
    //             out_off -= (uint64_t)out_len;
    //             sent_count += 1;
    //             sent_number += 1;
    //             pn = pkt_num_spaces.at(0).updatepktnum();
    //             priority = priority_calculation(out_off);
    //             Type ty = Type::Application;
             
    //             std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off, (uint64_t)out_len);
    //             hdrs.push_back(hdr);
    //             iovecs[2*i].iov_base = (void *)hdr.get();
    //             iovecs[2*i].iov_len = HEADER_LENGTH;
	// 	    }else{
    //             if (i < dmludp_error_sent){
	// 		        send_buffer.emit(iovecs[0], out_len, out_off);
    //                 continue;
    //             }

    //             s_flag = send_buffer.emit(iovecs[(i-dmludp_error_sent)*2+1], out_len, out_off);
    //             out_off -= (uint64_t)out_len;

    //             pn = pkt_num_spaces.at(0).updatepktnum();
    //             priority = priority_calculation(out_off);
    //             Type ty = Type::Application;
          
    //             std::shared_ptr<Header> hdr= std::make_shared<Header>(ty, pn, priority, out_off , (uint64_t)out_len);
    //             hdrs.push_back(hdr);
    //             iovecs[2*(i-dmludp_error_sent)].iov_base = (void *)hdr.get();
    //             iovecs[2*(i-dmludp_error_sent)].iov_len = HEADER_LENGTH;
    //         }
    //         auto offset = out_off;
    //         if (sent_dic.find(out_off) != sent_dic.end()){
    //             if (sent_dic[out_off] != 3&& ((get_dmludp_error() != 11))){
    //                 sent_dic[out_off] -= 1;
    //             }
    //         }else{
    //             sent_dic[out_off] = priority;
    //         }

    //         // if (out_len != MAX_SEND_UDP_PAYLOAD_SIZE){
    //         //     std::cout<<"[Send] offset: "<<offset<<", len:"<<out_len<<", pn:"<<pn<<std::endl;
    //         // }
            

    //         if (get_dmludp_error() == 0){
    //             record_send.push_back(offset);
    //             record2ack.push_back(offset);
    //             messages[i].msg_iov = &iovecs[2*i];
    //             messages[i].msg_iovlen = 2;
    //         }else{
    //             messages[i-dmludp_error_sent].msg_iov = &iovecs[2*(i-dmludp_error_sent)];
    //             messages[i-dmludp_error_sent].msg_iovlen = 2;
    //         }


    //         if (s_flag){
    //             stop_flag = true;
    //             if ((i+1) < send_buffer.data.size()){
    //                 if (get_dmludp_error() == 0){
    //                     iovecs.resize((i+1) * 2);
    //                     messages.resize(i+1);
    //                 }else{
    //                     iovecs.resize((i + 1 - dmludp_error_sent) * 2);
    //                     messages.resize(i + 1 - dmludp_error_sent);
    //                 }
    //             }
    //             else{
    //                 if (get_dmludp_error() == 11){
    //                     iovecs.resize((i + 1 - dmludp_error_sent) * 2);
    //                     messages.resize(i + 1 - dmludp_error_sent);
    //                 }
    //             }
    //             break;
    //         }

    //     }
    //     if (written_len){
    //         stop_ack = false;
    //     }


    //     written_data_len += written_len;

  	//     return written_len;
    // };

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

        if (send_buffer.data.size() != 0 || send_buffer.offset_recv.size() != 0){
            result = false;
        }
        
        if (result == true){
            data_buffer.clear();
            current_buffer_pos = 0;
        }
        return result;
    }

    // ssize_t send_elicit_ack_message(std::vector<uint8_t> &out){
    //     auto ty = Type::ElicitAck;        
    //     auto preparenum = record2ack.size();
    //     if(record2ack.empty()){
    //         return -1;
    //     }

    //     size_t pktlen = 0;
        
    //     size_t sent_num = std::min(preparenum, MAX_ACK_NUM);
    //     pktlen = sent_num * sizeof(uint64_t);
    //     auto pn = pkt_num_spaces.at(1).updatepktnum();
    //     Header* hdr = new Header(ty, pn, 0, 0, pktlen);
    //     out.resize(pktlen + HEADER_LENGTH);
    //     memcpy(out.data(), hdr, HEADER_LENGTH);
    //     memcpy(out.data() + HEADER_LENGTH, record2ack.data(), record2ack.size());
    //     if(sent_num == record2ack.size()){
    //         record2ack.clear();
    //     }else{
    //         record2ack.erase(record2ack.begin(), record2ack.begin()+ sent_num);
    //     }

    //     delete hdr; 
    //     hdr = nullptr; 
    //     ack_set.insert(pn);
    //     keyToValues[pn].push_back(pn);
    //     valueToKeys[pn] = pn;
    //     ack_point += sent_num;
    //     std::vector<uint8_t> wait_ack(out.begin()+ HEADER_LENGTH, out.end());
    //     std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
    //     retransmission_ack[pn] = std::make_pair(wait_ack, now);
    //     pktlen += HEADER_LENGTH;
    //     return pktlen;
    // }

    ssize_t send_elicit_ack_message_pktnum(std::vector<uint8_t> &out){
        auto ty = Type::ElicitAck;        
        auto preparenum = record2ack_pktnum.size();
        if(record2ack_pktnum.empty()){
            return -1;
        }

        size_t pktlen = 0;

        uint64_t start_pktnum = record2ack_pktnum[0];
        
        size_t sent_num = std::min(preparenum, MAX_ACK_NUM_PKTNUM);

        uint64_t end_pktnum = record2ack_pktnum[sent_num - 1];

        auto pn = pkt_num_spaces.at(1).updatepktnum();
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
        ack_set.insert(pn);
        keyToValues[pn].push_back(pn);
        valueToKeys[pn] = pn;
        ack_point += sent_num;
        std::vector<uint8_t> wait_ack(out.begin()+ HEADER_LENGTH, out.end());
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        retransmission_ack[pn] = std::make_pair(wait_ack, now);
        send_pkt_duration[pn] = std::make_pair(start_pktnum, end_pktnum);
        // std::cout<<"retransmission_ack.size:"<<retransmission_ack.size()<<" send_pkt_duration.size:"<<send_pkt_duration.size()<<std::endl;

        pktlen += HEADER_LENGTH;
        return pktlen;
    }

    // Time our occurs, timerfd triger this function and send retranmssion elicit ack.
    ssize_t send_timeout_elicit_ack_message(std::vector<std::vector<uint8_t>> &out, std::set<std::chrono::high_resolution_clock::time_point> &timestamps){
        auto ty = Type::ElicitAck;

        std::vector<uint64_t> pn_list;
        timestamps.clear();
        // Cancel timerfd.
        if (retransmission_ack.empty()){
            return -1;
        }
        
        for (const auto& e : retransmission_ack){
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::nanoseconds duration((uint64_t)(get_rtt()));
            if ((e.second.second + duration) < now ){
                pn_list.push_back(e.first);
            }
            else{
                timestamps.insert(e.second.second + duration);
            }
        }
        // No timeout occur
        if (pn_list.size() == 0){
            return 0;
        }
        for(auto n : pn_list){
            std::vector<uint8_t> out_buffer;
            uint64_t pktnum = pkt_num_spaces.at(1).updatepktnum();
            // std::cout<<"[Elicit] Elicit acknowledge packet(time out) num:"<<pktnum<<std::endl;
            // auto ty = Type::ElicitAck;
            size_t pktlen = retransmission_ack.at(n).first.size();
            Header* hdr = new Header(ty, pktnum, 0, 0, pktnum, 0, send_connection_difference, pktlen);
            pktlen += HEADER_LENGTH;
            out_buffer.resize(pktlen);

            memcpy(out_buffer.data(),hdr, HEADER_LENGTH);

            std::vector<uint8_t> wait_ack(retransmission_ack.at(n).first.begin(), retransmission_ack.at(n).first.end());

            memcpy(out_buffer.data() + HEADER_LENGTH, retransmission_ack.at(n).first.data(), retransmission_ack.at(n).first.size());

            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            auto initial_pn = valueToKeys[n];
            keyToValues[initial_pn].push_back(pktnum);
            valueToKeys[pktnum] = initial_pn;
            // std::cout<<"1 pktnum:"<<pktnum<<" send_timeout_elicit_ack_message retransmission_ack"<<std::endl;
            // for (auto it = retransmission_ack.begin(); it != retransmission_ack.end(); ++it) {
            //     std::cout << "Key: " << it->first << std::endl;
            // }
            auto it = retransmission_ack.find(n);
            if (it != retransmission_ack.end()) {
                /// remove?
                timeout_ack.insert(*it);
                ///
                retransmission_ack.erase(it);
            }

            retransmission_ack[pktnum] = std::make_pair(wait_ack, now);
            // std::cout<<"2 pktnum:"<<pktnum<<" send_timeout_elicit_ack_message retransmission_ack"<<std::endl;
            // for (auto it = retransmission_ack.begin(); it != retransmission_ack.end(); ++it) {
            //     std::cout << "Key: " << it->first << std::endl;
            // }

            uint64_t start_send_pn;
            uint64_t end_send_pn;
            memcpy(&start_send_pn, wait_ack.data(), sizeof(uint64_t));
            memcpy(&end_send_pn, wait_ack.data() + sizeof(uint64_t), sizeof(uint64_t));
            // std::cout<<"1 pktnum:"<<pktnum<<" send_timeout_elicit_ack_message send_pkt_duration"<<std::endl;
            // for (auto it = send_pkt_duration.begin(); it != send_pkt_duration.end(); ++it) {
            //     std::cout << "Key: " << it->first << std::endl;
            // }

            send_pkt_duration.erase(n);
            send_pkt_duration[pktnum] = std::make_pair(start_send_pn, end_send_pn);

            // std::cout<<"2 pktnum:"<<pktnum<<" send_timeout_elicit_ack_message send_pkt_duration"<<std::endl;
            // for (auto it = send_pkt_duration.begin(); it != send_pkt_duration.end(); ++it) {
            //     std::cout << "Key: " << it->first << std::endl;
            // }
            
            delete hdr; 
            hdr = nullptr; 
            out.push_back(out_buffer);
        }

        // remove?
        if (timestamps.empty()){
            std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
            std::chrono::nanoseconds duration((uint64_t)(get_rtt()));
            timestamps.insert(now + duration);
        }
        return pn_list.size(); 
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
            psize = (uint64_t)(receive_result.size());
            Header* hdr = new Header(ty, send_num, 0, 0, send_num, 1, send_connection_difference, psize);
            memcpy(out, hdr, HEADER_LENGTH);
            memcpy(out + HEADER_LENGTH, &receive_range.first, sizeof(uint64_t));
            memcpy(out + HEADER_LENGTH + sizeof(uint64_t), &receive_range.second, sizeof(uint64_t));
            memcpy(out + HEADER_LENGTH + 2 * sizeof(uint64_t), receive_result.data(), receive_result.size());
            receive_result.clear();
            delete hdr; 
            hdr = nullptr; 
        }      

        if (ty == Type::Fin){
            memcpy(out, fin_header, HEADER_LENGTH);
            return total_len;
        }

        handshake = std::chrono::high_resolution_clock::now();

        total_len += (size_t)psize;

        return total_len;
    };


    size_t send_data_acknowledge(uint8_t* src, size_t src_len){
        auto ty = Type::ACK;
        auto start = current_loop_min;
        auto end = current_loop_max;
        uint64_t psize = end - start + 1;
        Header* hdr = new Header(ty, send_num, 0, 0, send_num, 0, receive_connection_difference, psize);
        memcpy(src, hdr, HEADER_LENGTH);
        memcpy(src + HEADER_LENGTH, &start, sizeof(Packet_num_len));
        memcpy(src + HEADER_LENGTH + sizeof(Packet_num_len), &end, sizeof(Packet_num_len));
        // memset(src + HEADER_LENGTH + 2 * sizeof(Packet_num_len), 0, psize);
        for (auto pn = start; pn <= end; pn++){
            auto off_ = HEADER_LENGTH + 2 * sizeof(Packet_num_len) + pn - start;
            if(receive_pktnum2offset.find(pn) == receive_pktnum2offset.end()){
                src[off_] = 1;
            }else{
                src[off_] = 0;
            }
        }
        psize += HEADER_LENGTH + 2 * sizeof(Packet_num_len);
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
        // out = handshake_header;
        memcpy(out, handshake_header, HEADER_LENGTH);
        return total_len;
    };

    // Add Wating for receving(12.28)
    bool is_waiting(){
        return waiting_flag;
    }

    bool enable_adding(){
        // return ack_set.empty() && (send_buffer.pos == 0);
        return stop_flag && stop_ack;
    }

    bool is_stopped(){
        return stop_flag && stop_ack && initial;
    };

    //Start updating congestion control window and sending new data.
    bool is_ack(){
        auto now = std::chrono::high_resolution_clock::now();
        auto interval = now - handshake;
        if (interval > rtt){
            return true;
        }
        return false;
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
        // return rec_buffer.first_item_len();
    }

    uint8_t priority_calculation(uint64_t off){
        auto real_index = (uint64_t)(off / MAX_SEND_UDP_PAYLOAD_SIZE);
        return norm2_vec[real_index];
    };

    void reset(){
        norm2_vec.clear();
        send_buffer.clear();
    };


    void check_loss_pktnum(uint8_t* src, size_t src_len){
        uint64_t start = *reinterpret_cast<uint64_t*>(src + HEADER_LENGTH);
        uint64_t end = *reinterpret_cast<uint64_t*>(src + HEADER_LENGTH + sizeof(uint64_t));
        // Used to debug.
        // std::map<uint64_t, uint8_t> ack_record;
        receive_range = std::make_pair(start, end);
        for (auto pn = start; pn <= end; pn++){
            if (receive_pktnum2offset.find(pn) != receive_pktnum2offset.end()){
                receive_result.push_back(0);
                // ack_record[pn] = 0;
            }else{
                receive_result.push_back(1);
                // ack_record[pn] = 1;
            }          
        }

        // RRD.add_acknowledeg_info(send_num, std::move(ack_record));
    }

    void set_handshake(){
        handshake = std::chrono::high_resolution_clock::now();
    };

    double get_rtt() {
        return rto.count() + 0.7 * send_preparation.count();
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

    // Send buffer is empty or not. If it is empty, send_all() will try to fill it with new data.
    bool data_is_empty(){
        return send_buffer.data.empty();
    };

    bool is_empty(){
        return send_data_buf.empty();
    };

    bool ack_clear(){
        return send_buffer.check_ack();
    }

    bool data_empty(){
        return ack_clear() && is_empty();
    }

    bool empty(){
        return data_is_empty() && is_empty();
    }
    
};

const uint8_t Connection::handshake_header[] = {2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

const uint8_t Connection::fin_header[] = {7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
}