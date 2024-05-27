#pragma once

#include <deque>
#include "RangeBuf.h"
#include <algorithm>
#include <unordered_set>
#include <stdlib.h>
#include <numeric>

namespace dmludp{
const size_t SEND_BUFFER_SIZE = 1350;

const size_t MIN_SENDBUF_INITIAL_LEN = 1350;

    class SendBuf{
        public:
        // Date: 8th Jan, 2024
        // Data pair store offset 
        std::map<uint64_t, std::pair<uint8_t*, uint64_t>> data;

        // data_copy used to store last round unsent data.
        std::unordered_map<uint64_t, uint64_t> data_copy;

        size_t pos;

        uint64_t off;

        uint64_t length;

        uint64_t max_data;

        size_t used_length;

        std::vector<uint64_t> send_index;

        uint64_t removed;

        size_t sent;

        // scenario: left not received data is more than cwnd.
        ssize_t send_partial;

        std::unordered_set<uint64_t> recv_count;

        std::vector<uint64_t> record_off;

        SendBuf():
        pos(0),
        off(0),
        length(0),
        max_data(MIN_SENDBUF_INITIAL_LEN * 10),
        used_length(0),
        removed(0),
        sent(0),
        send_partial(0){};

        ~SendBuf(){};

        ssize_t cap(){
            used_length = len();
            return ((ssize_t)max_data - (ssize_t)used_length);
        };

        /// Returns the lowest offset of data buffered.
        uint64_t off_front() {
            auto tmp_pos = pos;  
            while (tmp_pos <= (send_index.size() - 1)){
                auto b = send_index.at(tmp_pos);
     
                if(data[b].second != 0){
                    record_off.push_back(tmp_pos);
                    return b;
                }
                tmp_pos += 1;
            }
        }

        /// Returns true if there is data to be written.
        bool ready(){
            return !data.empty();
        };


        void remove_element(uint32_t in_offset){
            data.erase(in_offset);
            if (auto search = data_copy.find(in_offset); search != data_copy.end()){
                data_copy.erase(in_offset);
            }
        }

        void data_restore(uint32_t in_offset){
            data_copy[in_offset] = data[in_offset].second;
            data[in_offset].second = 0;
        }

        void acknowledege_and_drop(uint32_t in_offset, bool is_drop){
            if (is_drop){
                remove_element(in_offset);
            }else{
                data_restore(in_offset);
            }
        }

        void clear(){
            data.clear();
            pos = 0;
            off = 0;
            length = 0;
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
                    return acc + x.second.second;
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

            int written_length_;
            for (written_length_ = 0; written_length_ < window_size;){
                auto packet_len = std::min(write_data_len, SEND_BUFFER_SIZE);
                data[off] = std::make_pair(src + start_off + written_length_, packet_len);
                off += (uint64_t) packet_len;
                length += (uint64_t) packet_len;
                used_length += packet_len;
                written_length_ += packet_len;
                write_data_len -= packet_len;
                send_index.push_back(off);
            }

            std::sort(send_index.begin(), send_index.end()); 

            return written_length_;
        }

        bool emit(struct iovec& out, size_t& out_len, uint32_t& out_off){
            bool stop = false;
            out_len = 0;
            out_off = (uint32_t)off_front();
            while (ready()){
                if(pos >= send_index.size()){
                    break;
                }

                auto buf = data[send_index.at(pos)];

                if (buf.second == 0){
                    pos += 1;
                    continue;
                }
                
                size_t buf_len = 0;
                
                bool partial;
                if(buf.second <= MIN_SENDBUF_INITIAL_LEN){
                    partial = true;
                }else{
                    partial = false;
                }

                // Copy data to the output buffer.
                out.iov_base = (void *)(buf.first);
                out.iov_len = buf.second;

                length -= (uint64_t)(buf.second);
                used_length -= (buf.second);

                out_len = buf.second;

                pos += 1;

                if (partial) {
                    // We reached the maximum capacity, so end here.
                    break;
                }

            }
            sent += out_len;

            //All data in the congestion control window has been sent. need to modify
            if (sent >= max_data) {
                stop = true;
                pos = 0;
            }
            if (pos == data.size()){
                stop = true;
                pos = 0;
            }
            
            out_off += (uint32_t)out_len;
            if (stop){
                send_index.clear();
                remove_indexes(send_index, record_off);
            }
            return stop;
        };

        void recovery_data(){
            for (auto i : data_copy){
                data[i.first].second = i.second;
                send_index.push_back(i.first);
            }
            data_copy.clear();
            std::sort(send_index.begin(), send_index.end()); 
        }

    };
    
    void remove_indexes(std::vector<int>& target, const std::vector<size_t>& indexes) {
        std::vector<size_t> sorted_indexes = indexes;
        std::sort(sorted_indexes.begin(), sorted_indexes.end(), std::greater<size_t>());

        for (size_t index : sorted_indexes) {
            if (index < target.size()) {
                target.erase(target.begin() + index); 
            } else {
                std::cerr << "Index " << index << " out of range." << std::endl;
            }
        }
    }
}
