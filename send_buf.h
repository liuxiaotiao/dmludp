#pragma once

#include <deque>
#include "RangeBuf.h"
#include <algorithm>
#include <unordered_set>
#include <stdlib.h>
#include <numeric>
#include <tuple>

namespace dmludp{
const size_t SEND_BUFFER_SIZE = 1350;

const size_t MIN_SENDBUF_INITIAL_LEN = SEND_BUFFER_SIZE;

    class SendBuf{
        public:
        std::deque<std::tuple<uint64_t, uint8_t*, uint64_t>> data;

        std::deque<std::tuple<uint64_t, uint8_t*, uint64_t>> data_copy;

        size_t pos;

        size_t last_pos;

        uint64_t off;

        uint64_t length;

        uint64_t max_data;

        size_t used_length;

        uint64_t removed;

        uint64_t written_packet;

        ssize_t sent;

        // scenario: left not received data is more than cwnd.

        std::vector<uint64_t> record_off;

        std::set<uint64_t> received_offset;

        std::set<uint64_t> received_check;

        size_t total_bytes;

        ssize_t written_bytes;

        SendBuf():
        pos(0),
        off(0),
        length(0),
        max_data(MIN_SENDBUF_INITIAL_LEN * 10),
        used_length(0),
        removed(0),
        sent(0),
        last_pos(0),
        total_bytes(0),
	    written_bytes(0),
        written_packet(0){};

        ~SendBuf(){};

        ssize_t cap(){
            used_length = len();
            return ((ssize_t)max_data - (ssize_t)used_length);
        };

        bool written_complete(){
            return written_bytes == total_bytes;
        }
            
	    ssize_t off_front(){
            ssize_t result = -1;
	        // std::cout<<"data.size():"<<data.size()<<std::endl;
            while(pos < data.size()){
                // std::cout<<"pos:"<<pos<<std::endl;
                if (received_offset.empty()){
                    result = std::get<0>(data[pos]);
                    // std::cout<<"pos:"<<pos<<", off:"<<std::get<0>(data[pos])<<std::endl;
                    // std::cout<<"result:"<<result<<", len:"<<std::get<2>(data[pos])<<std::endl;
                    last_pos = pos;
                    pos++;
                    break;
                }else{
                    // if (std::get<0>(data[pos]) > *received_offset.rbegin()){
                    //     result = std::get<0>(data[pos]);
                    //     last_pos = pos;
                    //     pos++;
                    //     break;
                    // }else{
                    auto check = received_offset.find(std::get<0>(data[pos]));
                    // std::cout<<"pos:"<<pos<<", off:"<<std::get<0>(data[pos])<<std::endl;
                    if(check == received_offset.end()){
                        result = std::get<0>(data[pos]);
                        // std::cout<<"pos:"<<pos<<", off:"<<std::get<0>(data[pos])<<std::endl;
			            last_pos = pos;
                        pos++;
                        break;
                    }else{
                        data.erase(data.begin() + pos);
                        removed++;
                        // std::cout<<"erase size:"<<data.size()<<std::endl;
                    }
                    //}
                    
                }
            }
            return result;
        }

        /// Returns true if there is data to be written.
        bool ready(){
            return !data.empty();
        };

        void acknowledege_and_drop(uint32_t in_offset, bool is_drop){
            if (is_drop){
                received_offset.insert(in_offset);
                received_check.insert(in_offset);
                // std::cout<<"received_offset.size:"<<received_offset.size()<<", received_check.size:"<<received_check.size()<<std::endl;
            }
        }

        void clear(){
		    data.clear();
            last_pos = 0;
            pos = 0;
            off = 0;
            length = 0;
            received_offset.clear();
            received_check.clear();
	        written_bytes = 0;
            removed = 0;
            written_packet = 0;
        };

        /// Returns the largest offset of data buffered.
        uint64_t off_back(){
            return off;
        };

        /// The maximum offset we are allowed to send to the peer.
        uint64_t max_off() {
            return max_data;
        };

        bool is_empty(){
            return data.empty();
        };

        // Length of stored data.
        size_t len(){
            size_t length_ = 0;
            if (data.empty()){
                return 0;
            }

            int length_accumulate = std::accumulate(data.begin(), data.end(), 0,
                [](int acc, const auto& x) {
                    return acc + std::get<2>(x);
                });
            
            return length_accumulate;
        };

        /// Updates the max_data limit to the given value.
        void update_max_data(uint64_t maxdata) {
            max_data = maxdata;
        };

        size_t last_congestion_window(){
            return max_data;
        }

        ////rewritetv
        /// Resets the stream at the current offset and clears all buffered data.
        uint64_t reset(){
            auto unsent_off = off_front();
            auto unsent_len = off_back() - unsent_off;

            // Drop all buffered data.
            data.clear();

            pos = 0;
            length = 0;
            off = unsent_off;
            return unsent_len;
        };
 

        size_t pkt_num(){
            return data.size();
        }

        // write() will let input data serilize
        ssize_t write(uint8_t* src, size_t start_off, size_t &write_data_len, size_t window_size, size_t off_len){
            // All data has been written into buffer, all buffer data has been sent
            if (write_data_len == 0){
                return 0;
            }

            if(start_off == 0){
                off = 0;
                total_bytes = write_data_len;
            }

            int written_length_;
            for (written_length_ = 0; written_length_ < window_size;){
                auto packet_len = std::min(write_data_len, SEND_BUFFER_SIZE);
                // data[off] = std::make_pair(src + start_off + written_length_, packet_len);
                // auto meta_ = std::make_pair(src + start_off + written_length_, packet_len);
                // data.push_back(std::make_pair(off, meta_));
                // std::cout<<"send buffer off:"<<off<<std::endl;
                data.emplace_back(off, src + start_off + written_length_, (uint64_t)packet_len);
                written_packet++;
                length += (uint64_t) packet_len;
                used_length += packet_len;
                written_length_ += packet_len;
                write_data_len -= packet_len;
                off += (uint64_t) packet_len;
		        written_bytes += packet_len;
                if (write_data_len == 0){
                    break;
                }
            }

            if (start_off == 0){
                pos = 0;
                last_pos = 0;
            }

            return written_length_;
        }

        bool emit(struct iovec& out, ssize_t& out_len, uint32_t& out_off){
            bool stop = false;
            
            while (ready()){ 
                out_len = 0;
                auto tmp_off = off_front();
                if (tmp_off == -1){
                    out_len = -1;
                    stop = true;
                    break;
                }
                out_off = tmp_off;
                auto buf = data[last_pos];

                // std::cout<<"send_index.at(pos):"<<send_index.at(pos)<<", buf.second:"<<buf.second<<std::endl;
                
                size_t buf_len = 0;
                
                bool partial;
                if(std::get<2>(buf) <= MIN_SENDBUF_INITIAL_LEN){
                    partial = true;
                }else{
                    partial = false;
                }
                out_len = std::get<2>(buf);
                /* if (out_len == 0){
                    if (used_length != 0){
                        if (total_bytes - out_off >= MIN_SENDBUF_INITIAL_LEN){
                                out_len = MIN_SENDBUF_INITIAL_LEN;
                            }else{
                                out_len = total_bytes - out_off + 1;
                            }
                    }
                }*/
                // Copy data to the output buffer.
                out.iov_base = (void *)(std::get<1>(buf));
                out.iov_len = out_len;

                length -= (uint64_t)(out_len);
                used_length -= (out_len);

                // out_len = buf.second.second;

                // out_len = std::get<2>(buf);

                // std::cout<<"out_off:"<<out_off<<", out_len"<<out_len<<", data.len():"<<data.size()<<", pos:"<<pos<<std::endl;

                if (partial) {
                    // We reached the maximum capacity, so end here.
                    break;
                }

            }
            sent += out_len;
            // std::cout<<"out_len:"<<out_len<<std::endl;
            // All data in the congestion control window has been sent. need to modify
            if (sent >= max_data) {
                // std::cout<<"sent >= max_data, sent:"<<sent<<", max_data:"<<max_data<<std::endl;
                stop = true;
            }
            if (pos == data.size()){
                // std::cout<<"pos:"<<pos<<", data.size():"<<data.size()<<std::endl;
                stop = true;
            }

            if (data.empty()){
                // std::cout<<"data.empty()"<<std::endl;
                stop = true;
            }

            if (stop){
                sent = 0;
                // pos = 0;
                // std::cout<<"sent:"<<sent<<std::endl;
            }
            return stop;
        };

        // After sending. Move data to data_copy
        void data_restore(){
            while(true){
                data_copy.push_back(std::move(data[last_pos]));
                data.erase(data.begin() + last_pos);
                if (last_pos == 0){
                    break;
                }
                last_pos--;
            }
        }

        // call recovery_data when timeout or normal send().
        void recovery_data(){
            if (data_copy.empty()){
                reset_iterator();
                received_check.clear();
                return;
            }

            for(auto i = 0; i < data_copy.size(); i++){
                // if(received_check.find(std::get<0>(data_copy[i])) != received_check.end()){
                //     continue;
                // }
                if (3 * max_data > data.size()){
                    data.push_back(std::move(data_copy[i]));
                }else{
                    data.insert(data.begin() + 3 * max_data + i, std::move(data_copy[i]));
                }
            }
            data_copy.clear();
            received_check.clear();
        };

        void manage_recovery(){
            data_restore();
            recovery_data();
            reset_iterator();
        };
        
        void reset_iterator(){
            pos = 0;
        };

        // Remove received data from sending buffer.
        void remove_received_from_buffer(){
            while(ready()){
                auto tmp_off = off_front();
                if(pos == data.size()){
                    break;
                }
            }
            reset_iterator();
        }

    };

    
    
    
}

