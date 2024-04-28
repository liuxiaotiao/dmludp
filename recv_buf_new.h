#pragma once

#include "RangeBuf.h"
#include <deque>
#include <map>
#include <vector>
#include <stdlib.h>
namespace dmludp{
    struct Interval {
        uint64_t start;
        uint64_t end;
    };

    struct IntervalNode {
        Interval intvl;
        uint64_t maxEnd;
        IntervalNode* left;
        IntervalNode* right;

        IntervalNode(Interval i) : intvl(i), maxEnd(i.end), left(nullptr), right(nullptr) {}
    };

    class IntervalTree {
    public:
        IntervalNode* root;

        IntervalTree() : root(nullptr) {}

        void insert(Interval i) {
            root = insert(root, i);
        }

        void clear() {
            clear(root);
            root = nullptr; // Reset the root pointer after clearing
        }

        std::vector<Interval> findMissingOffsets(uint64_t maxOffset) {
            std::set<uint64_t> missingOffsets;
            std::vector<Interval> coveredIntervals;
            inorder(root, coveredIntervals);

            uint64_t current = 0;
            for (const auto& intvl : coveredIntervals) {
                while (current < intvl.start && current <= maxOffset) {
                    missingOffsets.insert(current);
                    current++;
                }
                current = std::max(current, intvl.end + 1);
            }

            while (current <= maxOffset) {
                missingOffsets.insert(current);
                current++;
            }

            return consolidateIntervals(missingOffsets);
        }

    private:
        IntervalNode* insert(IntervalNode* node, Interval i) {
            if (!node) return new IntervalNode(i);

            if (i.start < node->intvl.start) {
                node->left = insert(node->left, i);
            } else {
                node->right = insert(node->right, i);
            }

            node->maxEnd = std::max(node->maxEnd, i.end);

            return node;
        }

        void inorder(IntervalNode* node, std::vector<Interval>& intervals) {
            if (!node) return;
            inorder(node->left, intervals);
            intervals.push_back(node->intvl);
            inorder(node->right, intervals);
        }

        std::vector<Interval> consolidateIntervals(const std::set<uint64_t>& missingOffsets) {
            std::vector<Interval> missingIntervals;
            if (missingOffsets.empty()) return missingIntervals;

            auto it = missingOffsets.begin();
            uint64_t start = *it;
            uint64_t end = start;

            while (++it != missingOffsets.end()) {
                if (*it == end + 1) {
                    end = *it;
                } else {
                    missingIntervals.push_back({start, end});
                    start = *it;
                    end = start;
                }
            }
            missingIntervals.push_back({start, end}); // Add the last range
            return missingIntervals;
        }

        void clear(IntervalNode* node) {
            if (!node) return;
            clear(node->left);
            clear(node->right);
            delete node;
        }
    };


    class RecvBuf{
    public:
        // std::map<uint64_t, std::shared_ptr<RangeBuf>> data;
        std::vector<uint8_t> data;

        void * src;

        size_t src_len;

        // Used to judge the new coming data stored at data or src;
        bool convert_flag;

        uint64_t off;

        uint64_t len;

        uint64_t last_maxoff;

        uint64_t max_recv_off;

        size_t removed;

        IntervalTree interval_record;

        RecvBuf():off(0), len(0), last_maxoff(0), max_recv_off(0), removed(0), convert_flag(false),src_len(0){};

        ~RecvBuf(){};

       
	    void write(std::vector<uint8_t> &out, uint64_t out_off){
            auto data_len = data.size();

            if(out_off > data_len){
		        data.resize(out_off+out.size());
		        memcpy(data.data() + data.size() - out.size(), out.data(), out.size() * sizeof(uint8_t));
            }
            else if(out_off == data_len){
		        data.resize(out_off + out.size());
                memcpy(data.data() + data.size() - out.size(), out.data(), out.size() * sizeof(uint8_t));
            }
            else{
                size_t startPos = out_off;
                size_t endPos = out_off+out.size();
                memcpy(data.data() + startPos, out.data(), out.size() * sizeof(uint8_t));
            }
            len += out.size();
	        if (len > data.size()){
				std::cout<<"[Debug] receive buffer len:"<<len<<" vector.size():"<<data.size()<<std::endl;
                _Exit(0);
            }
        }

        void set_src_len(size_t buf_size){
            src_len = buf_size;
        }

        void write_instant(const uint8_t* input, size_t input_len, uint64_t out_off){
            if (out_off == 0){
                if (data.size() > 48){
                    memcpy(src, data.data() + 48, (data.size() - 48));
                    data.resize(48);
                }
                convert_flag = true;
            }else{
                ///
                // interval_record.insert({out_off - 48 , out_off - 48 + input_len - 1});
                ///
                if (convert_flag){
                    memcpy(src + (out_off - 48), input, input_len);
                    len += input_len;
                }else{
                    // TODO
                    // When convert_flag is false, the packet will be stored at data vector first
                    // make sure that offset written data cannot be larger than maximum offset.
                    // reset convert_flag after emiiting.
                    auto data_len = data.size();

                    if(out_off > data_len){
                        data.resize(out_off + input_len);
                        memcpy(data.data() + data.size() - input_len, out.data(), input_len * sizeof(uint8_t));
                    }
                    else if(out_off == data_len){
                        data.resize(out_off + input_len);
                        memcpy(data.data() + data.size() - input_len, out.data(), input_len * sizeof(uint8_t));
                    }
                    else{
                        size_t startPos = out_off;
                        size_t endPos = out_off+input_len;

                        memcpy(data.data() + startPos, out.data(), input_len * sizeof(uint8_t));
                    }
                    len += input_len;
                }

                
                if (len > data.size() && !convert_flag){
                    std::cout<<"[Debug] receive buffer len:"<<len<<" vector.size():"<<data.size()<<std::endl;
                    _Exit(0);
                }
            }
        }

        size_t receive_length(){
            return len;
        }
        uint64_t max_ack(){
            return max_recv_off;
        }

        bool is_empty(){
            return data.empty();
        }

        size_t length(){
            return (data.size() - removed);
        }

        size_t first_item_len(size_t checkLength){
            size_t checkresult = 0;
            if (data.size() >= checkLength) {
                bool allZeros = std::all_of(data.begin(), data.begin() + checkLength,
                                                [](uint8_t val) { return val == 0; });
                if(!allZeros) {
                    checkresult = checkLength;
                }
            }
            return checkresult;
        }

        void data_padding(size_t paddingLength){
            if (data.size() < (removed + paddingLength)){
                data.resize(removed + paddingLength);
            }
        }

        size_t emit(uint8_t* out, bool iscopy, size_t output_len = 0){
            size_t emitLen = 0;
            if (iscopy){
                src_len = 0;
                if (output_len == 0){
                    // out = static_cast<uint8_t*>(data.data() + removed);
                    // memcpy(out, data.data() + removed, data.size());
                    convert_flag = false;
                    /*
                    auto missingIntervals = interval_record.findMissingOffsets(src_len);

                    for (const auto& interval : missingIntervals) {
                        memset(src + interval.start, 0, interval.end - interval.start + 1);
                    }
                    interval_record.clear();

                    */

                    // emitLen = data.size() - removed;
                    // removed = data.size();
                    emitLen = len - removed;
                    removed = len;
                    return emitLen;
                }

                if ((output_len + removed) > data.size()){
                    return emitLen;
                }

                if (removed == data.size()){
                    return emitLen;
                }

                memcpy(out, data.data() + removed, output_len);
                convert_flag = false;
                emitLen = output_len;
                removed += output_len;
            }
            return emitLen;
        }

        // when output_len is 0, left data will be emiited.
        // size_t emit(uint8_t* out, bool iscopy, size_t output_len = 0){
        //     size_t emitLen = 0;
        //     if (iscopy){
        //         src_len = 0;
        //         if (output_len == 0){
        //             // out = static_cast<uint8_t*>(data.data() + removed);
        //             memcpy(out, data.data() + removed, data.size());
        //             convert_flag = false;
        //             emitLen = data.size() - removed;
        //             removed = data.size();
        //             return emitLen;
        //         }

        //         if ((output_len + removed) > data.size()){
        //             return emitLen;
        //         }

        //         if (removed == data.size()){
        //             return emitLen;
        //         }

        //         memcpy(out, data.data() + removed, output_len);
        //         convert_flag = false;
        //         emitLen = output_len;
        //         removed += output_len;
        //     }else{
        //         if (output_len == 0){
        //             out = static_cast<uint8_t*>(data.data() + removed);
        //             emitLen = data.size() - removed;
        //             removed = data.size();
        //             return emitLen;
        //         }

        //         if ((output_len + removed) > data.size()){
        //             return emitLen;
        //         }

        //         if (removed == data.size()){
        //             return emitLen;
        //         }

        //         out = static_cast<uint8_t*>(data.data() + removed);
        //         emitLen = output_len;
        //         removed += output_len;
        //     }
        //     return emitLen;
        // }

        void reset() {
            data.clear();
            removed = 0;
            len = 0;
        };

        void shutdown()  {
            data.clear();
            len = 0;
        };

        /// Returns true if the stream has data to be read.
        bool ready() {
            return !data.empty();
        };
    };
}    
