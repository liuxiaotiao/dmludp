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

        std::map<uint64_t, bool> offset_recv;

        uint64_t removed;

        size_t sent;

        // scenario: left not received data is more than cwnd.
        ssize_t send_partial;

        std::unordered_set<uint64_t> recv_count;

        SendBuf():
        pos(0),
        off(0),
        length(0),
        max_data(MIN_SENDBUF_INITIAL_LEN * 8),
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

            while (tmp_pos <= (data.size() - 1)){
                auto b = data.at(tmp_pos);
                if(b.second.second != 0){
                    return b.first;
                }
                tmp_pos += 1;
            }
        }

        uint64_t off_front(){
            
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
            sent = 0;

            // All data has been written into buffer, all buffer data has been sent
            if (write_data_len == 0){
                return 0;
            }

            auto written_length_;
            for (written_length_ = 0; written_length_ < window_size;){
                auto packet_len = std::min(write_data_len, SEND_BUFFER_SIZE);
                offset_recv[off] = true;
                data[off] = std::make_pair(src + start_off + written_length_, packet_len);
                off += (uint64_t) packet_len;
                length += (uint64_t) packet_len;
                used_length += packet_len;
                written_length_ += packet_len;
                write_data_len -= packet_len;
            }

            return written_length_;
        }

        bool emit(struct iovec& out, size_t& out_len, uint32_t& out_off){
            bool stop = false;
            out_len = 0;
            out_off = (uint32_t)off_front();
            while (ready()){
                if(pos >= data.size()){
                    break;
                }

                auto buf = data.at(pos);

                if (buf.second.second == 0){
                    pos += 1;
                    continue;
                }
                
                size_t buf_len = 0;
                
                bool partial;
                if(buf.second.second <= MIN_SENDBUF_INITIAL_LEN){
                    partial = true;
                }else{
                    partial = false;
                }

                // Copy data to the output buffer.
                out.iov_base = (void *)(buf.second.first);
                out.iov_len = buf.second.second;

                length -= (uint64_t)(buf.second.second);
                used_length -= (buf.second.second);

                out_len = buf.second.second;

                pos += 1;

                if (partial) {
                    // We reached the maximum capacity, so end here.
                    break;
                }

            }
            sent += out_len;

            // std::cout<<"sent:"<<sent<<", max_data:"<<max_data<<", pos:"<<pos<<", data.size()"<<data.size()<<std::endl;
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
            return stop;
        };

        void recovery_data(){
            for (auto i : data_copy){
                data[i.first].second = data_copy.second;
            }
            data_copy.clear();
        }
        
        // Explain: 
        // off_len = SEND_BUFFER_SIZE - sent_partial_off
        // write_data_len = length of all data
        // ssize_t write_old(uint8_t* src, size_t start_off, size_t &write_data_len, size_t window_size, size_t off_len, bool partial = false) {
        //     // All data in the buffer has been sent out, remove received data from the buffer.
        //     if (pos == 0) {
        //         //recv_and_drop();
        //         recv_count.clear();
        //     }
        //     sent = 0;
        //     if (!partial){
        //         max_data = (uint64_t)window_size;
        //     }
            
        //     auto capacity = cap();
        //     if (capacity <= 0) {
        //         send_partial = (ssize_t)window_size;
        //         return -2;
        //     }

        //     // All data has been written into buffer, all buffer data has been sent
        //     if (write_data_len == 0 && data.empty()){
        //         return -1;
        //     }
            
        //     // no new data, but still have some unack data.
        //     if (write_data_len == 0 && !data.empty()){
        //         return 0;
        //     }

        //     if (is_empty()){
        //         // max_data = (uint64_t)window_size;
        //         removed = 0;
        //         sent = 0;
        //         // Get the send capacity. This will return an error if the stream
        //         // was stopped.
        //         length = (uint64_t)len();
        //         used_length = len();
        //         //Addressing left data is greater than the window size
        //         if (length >= window_size){
        //             return 0;
        //         }
        
        //         if (write_data_len == 0){
        //             return 0;
        //         }
                                
        //         size_t ready_written = 0;
        //         if (write_data_len > capacity){
        //             ready_written = capacity;
        //         }else{
        //             ready_written = write_data_len;
        //         }
        
        //         // We already recorded the final offset, so we can just discard the
        //         // empty buffer now.
        
        //         size_t write_len = 0;

        //         // off_len represnts the length of a splited data
        //         if (off_len > 0){
        //             if (ready_written > off_len){
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off, off_len)));
        //                 off += (uint64_t)off_len;
        //                 length += (uint64_t)off_len;
        //                 used_length += off_len;
        //                 write_len += off_len;
        //             }
        //             else{
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off, ready_written)));
        //                 off += (uint64_t)ready_written;
        //                 length += (uint64_t)ready_written;
        //                 used_length += ready_written;
        //                 write_len += ready_written;

        //                 write_data_len -= write_len;
        //                 return write_len;
        //             }
        //         }
        
        //         for (auto it = off_len; it < ready_written; ){
        //             if ((ready_written - it) > SEND_BUFFER_SIZE){
        //                 write_len += SEND_BUFFER_SIZE;
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off + it, SEND_BUFFER_SIZE)));
        //                 off += (uint64_t)SEND_BUFFER_SIZE;
        //                 length += (uint64_t)SEND_BUFFER_SIZE;
        //                 used_length += SEND_BUFFER_SIZE;
        //                 it += SEND_BUFFER_SIZE;
        //             }else{
        //                 write_len += ready_written - it;
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off + it, ready_written - it)));
        //                 off += (uint64_t)(ready_written - it);
        //                 length += (uint64_t)(ready_written - it);
        //                 used_length += (ready_written - it);
        //                 it = ready_written;
        //             }
                    
        //         }  
        //         write_data_len -= write_len;
        //         return write_len;   
        //     }
        //     else{
        //         sent = 0;
        //         // Get the stream send capacity. This will return an error if the stream
        //         // was stopped.
        //         // length = (uint64_t)len();
        //         // used_length = len();
        //         //Addressing left data is greater than the window size
        //         if (len() >= window_size){
        //             return 0;
        //         }
        
        //         if (write_data_len == 0){
        //             return 0;
        //         }

        //         size_t ready_written = 0;
        //         if (write_data_len > capacity){
        //             ready_written = capacity;
        //         }else{
        //             ready_written = write_data_len;
        //         }

        //         size_t write_len = 0;

        //         if (off_len > 0){
        //             if (ready_written > off_len){
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off, (uint64_t)off_len)));
        //                 off += (uint64_t)off_len;
        //                 length += (uint64_t)off_len;
        //                 used_length += off_len;
        //                 write_len += off_len;
        //             }
        //             else{
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off, (uint64_t)ready_written)));
        //                 offset_recv[off] = true;
        //                 off += (uint64_t)ready_written;
        //                 length += (uint64_t)ready_written;
        //                 used_length += ready_written;
        //                 write_len += ready_written;

        //                 write_data_len -= write_len;
        //                 return write_len;
        //             }
        //         }

        //         for (auto it = off_len ; it < ready_written ; ){
        //             if((ready_written - it) > SEND_BUFFER_SIZE){
        //                 write_len += SEND_BUFFER_SIZE;
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off + it, SEND_BUFFER_SIZE)));
        //                 off += (uint64_t) SEND_BUFFER_SIZE;
        //                 length += (uint64_t) SEND_BUFFER_SIZE;
        //                 used_length += SEND_BUFFER_SIZE;
        //                 it += SEND_BUFFER_SIZE;
        //             }else{
        //                 write_len += (ready_written - it);
        //                 offset_recv[off] = true;
        //                 data.push_back(std::make_pair(off, std::make_pair(src + start_off + it, (uint64_t)(ready_written - it))));
        //                 off += (uint64_t) (ready_written - it);
        //                 length += (uint64_t) (ready_written - it);
        //                 used_length += (ready_written - it);
        //                 it = ready_written;
        //             }
        //         }
        //         write_data_len -= write_len;
        //         return write_len;
        //     }
    
        // };

        // bool emit_old(struct iovec & out, size_t& out_len, uint32_t& out_off){
        //     bool stop = false;
        //     out_len = 0;
        //     out_off = (uint32_t)off_front();
        //     while (ready()){
        //         if(pos >= data.size()){
        //             break;
        //         }

        //         auto buf = data.at(pos);

        //         if (buf.second.second == 0){
        //             pos += 1;
        //             continue;
        //         }
                
        //         size_t buf_len = 0;
                
        //         bool partial;
        //         if(buf.second.second <= MIN_SENDBUF_INITIAL_LEN){
        //             partial = true;
        //         }else{
        //             partial = false;
        //         }

        //         // Copy data to the output buffer.
        //         out.iov_base = (void *)(buf.second.first);
        //         out.iov_len = buf.second.second;

        //         length -= (uint64_t)(buf.second.second);
        //         used_length -= (buf.second.second);

        //         out_len = (buf.second.second);

        //         pos += 1;

        //         if (partial) {
        //             // We reached the maximum capacity, so end here.
        //             break;
        //         }

        //     }
        //     sent += out_len;

        //     if (send_partial > 0){
        //         send_partial -= out_len;
        //         if (send_partial <= 0){
        //             stop = true;
        //             pos = 0;
        //             send_partial = 0;
        //         }
        //     }
        //     // std::cout<<"sent:"<<sent<<", max_data:"<<max_data<<", pos:"<<pos<<", data.size()"<<data.size()<<std::endl;
        //     //All data in the congestion control window has been sent. need to modify
        //     if (sent >= max_data) {
        //         stop = true;
        //         pos = 0;
        //     }
        //     if (pos == data.size()){
        //         stop = true;
        //         pos = 0;
        //     }
            
        //     out_off += (uint32_t)out_len;
        //     return stop;
        // };

    };
    
}
