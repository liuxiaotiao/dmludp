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
#include "cubic.h"
#include "recv_buf.h"
#include "send_buf.h"
#include <cmath>

namespace dmludp {

const size_t HEADER_LENGTH = 26;

// The default max_datagram_size used in congestion control.
const size_t MAX_SEND_UDP_PAYLOAD_SIZE = 1350;

const size_t MAX_ACK_NUM_PKTNUM = MAX_SEND_UDP_PAYLOAD_SIZE;

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

    uint64_t send_num;

    std::unordered_map<uint64_t, uint64_t> sent_dic;

    bool bidirect;

    Recovery recovery;

    std::array<PktNumSpace, 3> pkt_num_spaces;

    std::chrono::nanoseconds rtt;

    std::chrono::nanoseconds srtt;

    std::chrono::nanoseconds minrtt;

    std::chrono::nanoseconds rto;

    std::chrono::nanoseconds rttvar;
    
    std::chrono::high_resolution_clock::time_point handshake;

    SendBuf send_buffer;

    RecvBuf rec_buffer;

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

    std::set<uint64_t> send_unack_packet_record;

    Packet_num_len last_elicit_ack_pktnum;

    Packet_num_len next_elicit_ack_pktnum;

    size_t received_packets;

    std::set<size_t> received_packets_dic;

    std::set<uint64_t> offset_dic;

    

    // key: seq_num, (std::pair(firstpkt, start_send), std::pair(lastpkt, end_send)
    std::map<uint64_t, std::map<uint64_t, std::chrono::system_clock::time_point>> seq2pkt;

    size_t send_status;

    std::pair<uint64_t, uint64_t> packet_info;

    std::vector<uint64_t> range4receive;

    uint64_t receive_start;

    uint64_t receive_end;

    std::pair<ssize_t, ssize_t> receive_info;

    std::map<uint16_t, std::pair<uint64_t, uint64_t>> send_range;

    ////////////////////////////////
    /*
    If using sendmsg send udp data, replace msghdr with msghdr.
    */
    std::vector<std::shared_ptr<Header>> send_hdrs;
    std::vector<uint8_t> send_hdr;
    std::vector<struct msghdr> send_messages;
    std::vector<struct iovec> send_iovecs;
    std::vector<uint8_t> send_ack;

    // Set at get_data()
    size_t data_gotten;    

    bool send_partial_signal;

    bool send_full_signal;

    bool send_phrase;

    // used to judge loss or not
    std::pair<uint64_t, uint64_t> sent_packet_range;

    std::pair<uint64_t, uint64_t> next_range;

    // Record how many packet sent.
    std::pair<uint64_t, uint64_t> last_range;

    bool difference_flag;

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
    send_num(0),
    rtt(0),
    srtt(0),
    minrtt(0),
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
    last_elicit_ack_pktnum(0),
    next_elicit_ack_pktnum(0),
    received_packets(0),
    recovery(MAX_SEND_UDP_PAYLOAD_SIZE),
    data_gotten(0),
    send_phrase(true),
    difference_flag(false),
    send_status(0)
    {
        send_ack.reserve(42);
        Header* hdr = reinterpret_cast<Header*>(send_ack.data());
        hdr->ty = Type::ElicitAck;
        hdr->pkt_num = 0;;
        hdr->priority = 0;
        hdr->offset = 0;
        hdr->seq = 0;
        hdr->ack_time = 0;
        hdr->difference = 0;
        hdr->pkt_length = 2 * sizeof(uint64_t);
        init();
    };

    ~Connection(){};

    void init(){
        send_hdr.resize(HEADER_LENGTH);
        send_hdrs.resize(1);
        send_messages.resize(1);
        send_iovecs.resize(2);
        Header* hdr = reinterpret_cast<Header*>(send_hdr.data());
        hdr->ty = Type::Application;
        hdr->pkt_num = 0;;
        hdr->priority = 0;
        hdr->offset = 0;
        hdr->seq = 0;
        hdr->ack_time = 0;
        hdr->difference = 0;
        send_iovecs.at(0).iov_base = (void *)send_hdr.data();
        send_iovecs.at(0).iov_len = HEADER_LENGTH;
        pkt_num_spaces.at(2).updatepktnum();
        receive_info = {-1, -1};
        next_range = sent_packet_range = last_range = {0,0};
    }

    void initial_rtt() {
        auto arrive_time = std::chrono::high_resolution_clock::now();
        srtt = arrive_time - handshake;
        rttvar = srtt / 2;
        rto = srtt + 4 * rttvar;
    }

    void update_rtt2(std::chrono::high_resolution_clock::time_point tmp_handeshake){
        auto arrive_time = std::chrono::high_resolution_clock::now();
        rtt = arrive_time - tmp_handeshake;
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(arrive_time.time_since_epoch()).count();
        auto tmp_srtt = std::chrono::duration<double, std::nano>(srtt.count() * alpha + (1 - alpha) * rtt.count());
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_srtt);
        auto diff = srtt - rtt;
        auto tmp_rttvar = std::chrono::duration<double, std::nano>((1 - beta) * rttvar.count() + beta * std::abs(diff.count()));
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_rttvar);
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

        RTTVAR <- (1 - beta) * RTTVAR + beta * |SRTT - R'|
        SRTT <- (1 - alpha) * SRTT + alpha * R'
        RTO <- SRTT + max (G, K*RTTVAR)
        */
        auto arrive_time = std::chrono::high_resolution_clock::now();
        rtt = arrive_time - handshake;    
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(arrive_time.time_since_epoch()).count();
        auto tmp_srtt = std::chrono::duration<double, std::nano>(srtt.count() * alpha + (1 - alpha) * rtt.count());
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_srtt);
        auto diff = srtt - rtt;
        auto tmp_rttvar = std::chrono::duration<double, std::nano>((1 - beta) * rttvar.count() + beta * std::abs(diff.count()));
        srtt = std::chrono::duration_cast<std::chrono::nanoseconds>(tmp_rttvar);
        if (srtt.count() < minrtt.count()){
            minrtt = srtt;
        }
        rto = srtt + 4 * rttvar;
    };

    // Merge to intial_rtt
    void set_rtt(uint64_t inter){
        if (bidirect){
            rtt = std::chrono::nanoseconds(inter);
        }
        if (srtt.count() == 0){
            srtt = rtt;
            minrtt = rtt;
        }
        if (rttvar.count() == 0){
            rttvar = rtt / 2;
        }
    };

    // Check timeout or not
    bool on_timeout(){
        bool timeout_;
        std::chrono::nanoseconds duration((uint64_t)(get_rtt()));
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        if (handshake + duration < now){
            timeout_ = true;
        }else{
            timeout_ = false;
        }
        return timeout_;
    }

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
        }
        
        // All side can send data.
        if (pkt_ty == Type::ACK){
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
            send_num = pkt_num;
            check_loss_pktnum(src, src_len);
        }

        if (pkt_ty == Type::Application){
            if (receive_pktnum2offset.find(pkt_num) != receive_pktnum2offset.end()){
                if(receive_pktnum2offset.at(pkt_num).second == 1){
                    return 0;
		        }
            }

            if (pkt_difference != receive_connection_difference){
                clear_recv_setting();
            }
            if (pkt_num > current_loop_max){
                current_loop_max = pkt_num;
            }

            reset_single_receive_parameter(pkt_seq);
        
            if (recv_dic.find(pkt_offset) == recv_dic.end() && pkt_difference == receive_connection_difference){
                rec_buffer.write(src + HEADER_LENGTH, pkt_len, pkt_offset);
                recv_dic.insert(std::make_pair(pkt_offset, pkt_priorty));
            }

            receive_pktnum2offset.insert(std::make_pair(pkt_num, std::make_pair(pkt_offset, 1)));                     
        }

        if (pkt_ty == Type::Stop){
            stop_flag = false;
            return 0;
        }

        return read_;
    };

    size_t check_status(){
        if (data_gotten == 0){
            // First add data to buffer.
            data_preparation();
            data_gotten = 1;
        }else if(data_gotten == 1){
            // Skip
        }else if(data_gotten == 2){
            data_preparation();
        }else{

        }

        if (recovery.cwnd_available()){
            return 1;
        }

        if (send_status == 4){
            send_status = 3;
            return 1;
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        auto handshake_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(handshake.time_since_epoch()).count();
        if (handshake + 1.05 * srtt < now && send_status == 1){
            std::cout<<"now - handshake:"<<(now_ns - handshake_ns)<<", srtt:"<<srtt.count()<<std::endl;
            send_status = 3;
            // if(last_elicit_ack_pktnum == pkt_num_spaces.at(2).getpktnum()){
            //     auto elicit_pn = pkt_num_spaces.at(2).updatepktnum();
            // }
            double total_sent = sent_packet_range.second - sent_packet_range.first + 1;
            double acked = received_packets_dic.size();
            double unacked = total_sent - acked;
            double total_lost = total_sent - acked;
            double loss_ratio = total_lost / total_sent;
            auto now = std::chrono::system_clock::now();
            if(loss_ratio < 0.1){
                recovery.on_packet_ack(acked, total_sent, now, std::chrono::duration_cast<std::chrono::seconds>(minrtt));
            }else{
                recovery.check_point();
                recovery.congestion_event(now);
            }
            return 1;
        }

        return 0;
    }

    // Find the receive range.
    void findRanges(uint64_t packet_) {
        if (receive_info.first == -1){
            receive_info = {packet_, packet_};
        }

        if (packet_ == receive_info.second + 1){
            receive_info.second++;
        }else if(packet_ < receive_info.second){
            //
        }else{
            range4receive.push_back(receive_info.first);
            range4receive.push_back(receive_info.second);
            receive_info = {packet_, packet_};
        }
    }


    int pre_process_application_packet(uint8_t* data, size_t buf_len, uint32_t &off, uint64_t &pn){
        auto result = reinterpret_cast<Header *>(data)->ty;
        pn = reinterpret_cast<Header *>(data)->pkt_num;
        // auto pkt_priorty = reinterpret_cast<Header *>(data)->priority;
        off = reinterpret_cast<Header *>(data)->offset;
        // auto pkt_len = reinterpret_cast<Header *>(data)->pkt_length;
        Difference_len pkt_difference = reinterpret_cast<Header *>(data)->difference;
        auto pkt_seq = reinterpret_cast<Header *>(data)->seq;

        if (result == Type::Application){
            // std::cout<<"[Debug] application packet:"<<pn<<", off:"<<off<<", len:"<<pkt_len<<", difference:"<<(int)pkt_difference<<", receive_connection_difference:"<<(int)receive_connection_difference<<std::endl;
            if (receive_pktnum2offset.find(pn) != receive_pktnum2offset.end()){
                std::cout<<"[Error] Duplicate application packet"<<std::endl;
                return 0;
            }
                
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

            // Debug
            if (recv_dic.find(off) != recv_dic.end()){
                receive_pktnum2offset.insert(std::make_pair(pn, std::make_pair(off,0)));
            }else{
                recv_count += 1;
                // optimize to reduce copy time.
                receive_pktnum2offset.insert(std::make_pair(pn, std::make_pair(off,0)));
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
        size_t received_check = 0;
        uint8_t tmp_send_connection_difference = send_connection_difference + 1;

        if(send_connection_difference != pkt_difference){
            if (tmp_send_connection_difference == pkt_difference){
                send_buffer.clear();
            }
            return;
        }
        auto first_pkt = *(uint64_t *)(src + HEADER_LENGTH);
        auto end_pkt = *(uint64_t *)(src + HEADER_LENGTH + sizeof(uint64_t));

        double total_sent = sent_packet_range.second - sent_packet_range.first + 1;
        double acked = received_packets_dic.size();
        double unacked = total_sent - acked;

        // When sender received 1st last elicit acknowledge packet, sender update new sequence number and 
        // 
        // if(pkt_num == last_elicit_ack_pktnum){
        //     auto seq_pn = pkt_num_spaces.at(2).updatepktnum();
        //     send_status = 0;
        // }

        if ((pkt_num + 1) == last_elicit_ack_pktnum){
            if (auto search = seq2pkt.find(pkt_num) != seq2pkt.end()){
                for(auto check_pn = first_pkt; check_pn <= end_pkt; check_pn++){
                    seq2pkt.at(pkt_num).erase(check_pn);
                }
                if(seq2pkt.at(pkt_num).size() <= (last_range.second - last_range.first + 1)*0.05){
                    recovery.rollback();
                }
                return;
            }
        }else{
            for (auto check_pn = first_pkt; check_pn <= end_pkt; check_pn++){
                auto check_offset = pktnum2offset[check_pn];
                
                auto received_ = check_pn - first_pkt + 2 * sizeof(Packet_num_len) + HEADER_LENGTH;
        
                if (src[received_] == 0){
                    send_buffer.acknowledege_and_drop(check_offset, true);
                    if (check_pn <= sent_packet_range.second && check_pn >= sent_packet_range.first){
                        received_packets++;
                        received_packets_dic.insert(check_pn);
                        received_check++;
                    }
                }else{
                    // if(sent_dic.find(check_offset) != sent_dic.end()){
                    //     if (sent_dic.at(check_offset) == 0){
                    //         send_buffer.acknowledege_and_drop(check_offset, true);
                    //     }
                    // }
                }
            }
        }
        
        if (end_pkt == sent_packet_range.second){
            update_rtt();
            pkt_ack_time = 1;
        }

        if(received_packets_dic.size() == (sent_packet_range.second - sent_packet_range.first + 1)){
            pkt_ack_time = 1;
            seq2pkt.erase(pkt_num);
        }

        if (pkt_ack_time == 1){
            double total_lost = total_sent - received_packets_dic.size();
            double loss_ratio = total_lost / total_sent;
            auto now = std::chrono::system_clock::now();
            send_status = 4;
            last_elicit_ack_pktnum = next_elicit_ack_pktnum;
            // suprious congestion
            if(loss_ratio < 0.1){
                recovery.on_packet_ack(received_check - acked, total_sent, now, std::chrono::duration_cast<std::chrono::seconds>(minrtt));
            }else{
                recovery.check_point();
                recovery.congestion_event(now);
                for (const auto pkt: received_packets_dic){
                    seq2pkt[pkt_num].erase(pkt);
                }
            }
        }else{
            auto now = std::chrono::system_clock::now();
            recovery.on_packet_ack(received_check, total_sent, now, std::chrono::duration_cast<std::chrono::seconds>(minrtt));
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

        current_buffer_pos = 0;

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
        seq2pkt.clear();
        data_gotten = 0;
        return completed;
    }

    size_t get_once_data_len(){
        return written_data_once;
    }

    void clear_sent_once(){
        written_data_once = 0;
    }

    void data_preparation(){
        ssize_t expect_win = recovery.cwnd_available();

        if(data_gotten != 0){
            ssize_t left_space = 0;
            for (auto i = 0; i < data_buffer.size(); i++){
                left_space += data_buffer.at(i).left;
            }
            expect_win = left_space;
            data_gotten = 4;
        }
 
        for (; current_buffer_pos < data_buffer.size(); current_buffer_pos++){
            auto result = data2buffer(data_buffer.at(current_buffer_pos), expect_win);
            expect_win -= result;
            if(expect_win <= 0){
                break;
            }

            if (result == -1){
                continue;
            }

            if (result < expect_win && (current_buffer_pos + 1) < data_buffer.size()){
                continue;
            }
        }
    }

    void recovery_send_buffer(){
        send_buffer.recovery_data();
    }

    ssize_t data2buffer(sbuffer &send_data, ssize_t expected_cwnd_){
        ssize_t result = 0;
        size_t out_len = 0;

        if (send_data.left == 0){
            result = -1;
        }else{
            result = send_buffer.write(send_data.src, send_data.sent(), send_data.left, expected_cwnd_, out_len);
        }
        return result;
    }


    ssize_t send_packet(){
        auto check0 = std::chrono::high_resolution_clock::now();
        auto send_seq = 0;
        size_t written_len = 0;
        // If has EAGAIN, continue sending, not get new packet.
        // auto check1 = std::chrono::high_resolution_clock::now();
        if (get_dmludp_error()){
            return send_iovecs.at(0).iov_len + send_iovecs.at(1).iov_len;
        }
        // auto check2 = std::chrono::high_resolution_clock::now();
        auto available_cwnd = recovery.cwnd_available();
        if (available_cwnd <= 0){
            return 0;
        }
        // auto check3 = std::chrono::high_resolution_clock::now();
        send_buffer.update_max_data(MAX_SEND_UDP_PAYLOAD_SIZE);
        // auto check4 = std::chrono::high_resolution_clock::now();

        auto pn = pkt_num_spaces.at(0).updatepktnum();

        // TODO
        // If cwnd shrink, process update seq.
        send_seq = pkt_num_spaces.at(2).getpktnum();
        next_elicit_ack_pktnum = send_seq;

        if(send_status == 0){
            if (pn == 0){
                last_elicit_ack_pktnum = next_elicit_ack_pktnum = send_seq;
            }
            // last_range = sent_packet_range;
            // sent_packet_range.first = pn;
            send_status = 1;
        }

        if (next_range == std::make_pair<uint64_t, uint64_t>(0, 0)){
            if(pn == 0){
                next_range = {0, 1};
            }else{
                next_range = {pn, pn};
            }
        }
        if (pn != 0)
            next_range.second = pn;

        ssize_t out_len = 0; 
        Offset_len out_off = 0;
        auto priority = 0;
        
        // auto check5 = std::chrono::high_resolution_clock::now();
        
        // auto check6 = std::chrono::high_resolution_clock::now();
        auto s_flag = send_buffer.emit(send_iovecs[1], out_len, out_off);

        sent_count += 1;
        // auto check7 = std::chrono::high_resolution_clock::now();
        // priority = priority_calculation(out_off);
        // auto check8 = std::chrono::high_resolution_clock::now();
        Type ty = Type::Application;

        Header *hdr;
        hdr = reinterpret_cast<dmludp::Header*>(send_hdr.data());
        hdr->ty = ty;
        hdr->pkt_num = pn;
        hdr->priority = 3;
        hdr->offset = out_off;
        hdr->seq = send_seq;
        hdr->difference = send_connection_difference;
        hdr->pkt_length = (Packet_num_len)out_len;
        send_iovecs.at(0).iov_base = (void *)send_hdr.data();
        send_iovecs.at(0).iov_len = HEADER_LENGTH;
        recovery.on_packet_sent(out_len);
        // auto check9 = std::chrono::high_resolution_clock::now();

        // Each sequcence coressponds to the packet range.
        // if (sent_dic.find(out_off) != sent_dic.end()){
        //     if (sent_dic[out_off] != 3){
        //         sent_dic[out_off] -= 1;
        //     }
        // }else{
        //     sent_dic[out_off] = priority;
        // }

        // auto check10 = std::chrono::high_resolution_clock::now();
        pktnum2offset[pn] = out_off;
        pktnum2offset.insert(std::make_pair(pn, out_off));
        send_messages[0].msg_iov = &send_iovecs[0];
        send_messages[0].msg_iovlen = 2;
        written_len += out_len;
        // auto check11 = std::chrono::high_resolution_clock::now();
        if (s_flag){     
            // sent_packet_range.second = pn;
            // auto seq_next = pkt_num_spaces.at(2).updatepktnum();
            send_status = 2;
            data_gotten = 2;
        }

        // auto check12 = std::chrono::high_resolution_clock::now();
        if (written_len){
            stop_ack = false;
        }
        // packet_info.first = send_seq;
        // packet_info.second = pn;
        packet_info = {send_seq, pn};
        written_data_len += written_len;
        // auto check13 = std::chrono::high_resolution_clock::now();
        // std::cout << "check1: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check2 - check1).count() << "ns" << 
        // ", check2: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check3 - check2).count() << "ns" <<
        // ", check3: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check4 - check3).count() << "ns" <<
        // ", check4: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check5 - check4).count() << "ns" <<
        // ", check5: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check6 - check5).count() << "ns" <<
        // ", check6: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check7 - check6).count() << "ns" <<
        // ", check7: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check8 - check7).count() << "ns" <<
        // ", check8: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check9 - check8).count() << "ns" <<
        // ", check9: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check10 - check9).count() << "ns" <<
        // ", check10: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check11 - check10).count() << "ns" <<
        // ", check11: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check12 - check11).count() << "ns" <<
        // ", check13: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check13 - check12).count() << "ns" <<
        // ", total: " << std::chrono::duration_cast<std::chrono::nanoseconds>(check13 - check0).count() << "ns" <<std::endl;
  	    return written_len;
    }

    void send_packet_complete(size_t err_ = 0){
        set_error2(err_);
        if (err_ != 0){
            return;
        }
        
        if(send_status == 2){
            send_status = 3;
        }else if(send_status == 3){
            // Send elicit packet
            set_handshake();
            send_status = 0;
            last_range = sent_packet_range;
            sent_packet_range = next_range;
            next_range = {0, 0};
        }
        else{
            auto now = std::chrono::system_clock::now();
            seq2pkt[packet_info.first][packet_info.second] = now;
        }
    }

    ssize_t send_elicit_ack_message_pktnum_new3(){
        auto ty = Type::ElicitAck;        

        size_t pktlen = 0;

        uint64_t start_pktnum = next_range.first;
        uint64_t end_pktnum =  next_range.second;

        auto pn = pkt_num_spaces.at(1).updatepktnum();
        send_ack.resize(HEADER_LENGTH + 2 * sizeof(uint64_t));

        Header* hdr = reinterpret_cast<Header*>(send_ack.data());
        hdr->ty = Type::ElicitAck;
        hdr->pkt_num = pn;
        hdr->seq = pn;
        hdr->difference = send_connection_difference;

        uint64_t* start_ptr = reinterpret_cast<uint64_t*>(send_ack.data() + HEADER_LENGTH);
        *start_ptr = start_pktnum;
        
        uint64_t* end_ptr = reinterpret_cast<uint64_t*>(send_ack.data() + HEADER_LENGTH + sizeof(uint64_t));
        *end_ptr = end_pktnum;
        std::cout<<"start_pktnum:"<<start_pktnum<<", end_pktnum:"<<end_pktnum<<std::endl;
        
        pktlen = HEADER_LENGTH + 2 * sizeof(uint64_t);
        return pktlen;
    }

    ssize_t send_elicit_ack_message_pktnum_new2(uint64_t elicit_acknowledege_packet_number, uint64_t range_ = 0){
        auto ty = Type::ElicitAck;        

        size_t pktlen = 0;

        uint64_t start_pktnum = sent_packet_range.first;
        uint64_t end_pktnum = sent_packet_range.second;

        auto pn = elicit_acknowledege_packet_number;
        send_ack.resize(HEADER_LENGTH + 2 * sizeof(uint64_t));

        Header* hdr = reinterpret_cast<Header*>(send_ack.data());
        hdr->ty = Type::ElicitAck;
        hdr->pkt_num = pn;
        hdr->seq = pn;
        hdr->difference = send_connection_difference;

        uint64_t* start_ptr = reinterpret_cast<uint64_t*>(send_ack.data() + 26);
        *start_ptr = start_pktnum;
        
        uint64_t* end_ptr = reinterpret_cast<uint64_t*>(send_ack.data() + 26 + sizeof(uint64_t));
        *end_ptr = end_pktnum;
        
        pktlen += HEADER_LENGTH;
        return pktlen;
    }


    size_t get_error_sent(){
        return dmludp_error_sent;
    }

    size_t get_dmludp_error(){
        return dmludp_error;
    }

    void set_error2(size_t err){
        dmludp_error = err;
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

        if (!send_buffer.is_empty()){
            result = false;
        }
        
        if (result == true){
            data_buffer.clear();
            current_buffer_pos = 0;
        }

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
            initial = true;
        }

        if (ty == Type::ACK){
            psize = (uint64_t)(receive_result.size()) + 2 * sizeof(uint64_t);
            Header* hdr = new Header(ty, send_num, 0, 0, send_num, 1, receive_connection_difference, psize);
            if (difference_flag){
                uint8_t tmp = receive_connection_difference - 1;
                hdr->difference = tmp;
            }

            // Recevie info clear.
            receive_info = {-1, -1};
            range4receive.clear();

            memcpy(out, hdr, HEADER_LENGTH);
            memcpy(out + HEADER_LENGTH, &receive_range.first, sizeof(uint64_t));
            memcpy(out + HEADER_LENGTH + sizeof(uint64_t), &receive_range.second, sizeof(uint64_t));
            memcpy(out + HEADER_LENGTH + 2 * sizeof(uint64_t), receive_result.data(), receive_result.size());
            receive_result.clear();
            difference_flag = false;
            delete hdr; 
            hdr = nullptr; 
            update_receive_parameter();
        }      

        if (ty == Type::Fin){
            memcpy(out, fin_header, HEADER_LENGTH);
            return total_len;
        }

        total_len += (size_t)psize;

        return total_len;
    };

    void reset_single_receive_parameter(Acknowledge_sequence_len pkt_seq){
        if (send_num != pkt_seq){
            send_num = pkt_seq;
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
        update_receive_parameter();
        delete hdr; 
        hdr = nullptr; 
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
        receive_range = std::make_pair(start, end);
        if(end - start > MAX_SEND_UDP_PAYLOAD_SIZE){
            start = end - MAX_SEND_UDP_PAYLOAD_SIZE + 1;
        }
	    if(trail_send_difference == receive_connection_difference){
            difference_flag = true;
            if(end - start < MAX_SEND_UDP_PAYLOAD_SIZE){
                receive_result.resize((end - start + 1), 0);
            }
        }else{
            for (auto pn = start; pn <= end; pn++){
                if (receive_pktnum2offset.find(pn) != receive_pktnum2offset.end()){
                    receive_result.push_back(0);
                }else{
                    receive_result.push_back(1);
                }
            }
        }
    }

    void set_handshake(){
        handshake = std::chrono::high_resolution_clock::now();
        auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(handshake.time_since_epoch()).count();
        std::cout<<"handshake:"<<now_ns<<std::endl;
        received_packets = 0;
        received_packets_dic.clear();
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
        if (rto.count() == 0 && is_server == true){
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