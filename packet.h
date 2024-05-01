#pragma once
#include <iostream>
#include <vector>
#include <cstdint>
#include <iostream>
#include <vector>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <memory>

namespace dmludp{
    enum Type : uint8_t {
        /// Retry packet.
        Retry = 0x01,

        /// Handshake packet.
        Handshake = 0x02,

        /// application packet.
        Application = 0x03,

        /// server ask reciver
        ElicitAck = 0x04,

        ///ACK
        ACK = 0x05,

        /// STOP
        Stop = 0x06,

        /// Fin
        Fin = 0x07,

        // StartACK
        StartAck = 0x08,

        Unknown = 0x09,
    };

// Avoid memory alignment
#pragma pack(push, 1)
    class Header{
        public:
        /// The type of the packet.
        Type ty;

        uint64_t pkt_num;

        uint8_t priority;

        uint64_t offset;

        uint64_t pkt_length;

        Header(
            Type first, 
            uint64_t pktnum, 
            uint8_t priority, 
            uint64_t off, 
            uint64_t len) 
            : ty(first), 
            pkt_num(pktnum), 
            priority(priority), 
            offset(off), 
            pkt_length(len) {};

        ~Header() {};


        void to_bytes(std::vector<uint8_t> &out){
            uint8_t first = 0;
            size_t off = 0;
            if (ty == Type::Retry){
                first = 0x01;
            }else if (ty == Type::Handshake){
                first = 0x02;
            }else if (ty == Type::Application){
                first = 0x03;
            }else if (ty == Type::ElicitAck){
                first = 0x04;
            }else if (ty == Type::ACK){
                first = 0x05;
            }else if (ty == Type::Stop){
                first = 0x06;
            }else if (ty == Type::Fin){
                first = 0x07;
            }else if (ty == Type::StartAck){
                first = 0x08;
            }else{
                first = 0x09;
            }
            put_u8(out, first, off);
            
            off += sizeof(uint8_t);
            put_u64(out, pkt_num, off);
            off += sizeof(uint64_t);

            put_u8(out, priority, off);
            off += sizeof(uint8_t);
            put_u64(out, offset, off);
            off += sizeof(uint64_t);
            put_u64(out, pkt_length, off);

        };

        void put_u64(std::vector<uint8_t> &vec, uint64_t &input, size_t position){
            memcpy(vec.data() + position, input, sizeof(uint64_t));
        };

        void put_u8(std::vector<uint8_t> &vec, uint8_t input, size_t position){
            vec.at(position)= input;
        };

        size_t len(){
            return 26;
        };
    };

    class PktNumSpace{
        public:

        uint64_t next_pkt_num;

        std::unordered_map<uint64_t, uint64_t > priority_record;

        std::unordered_map<uint64_t, std::array<uint64_t, 2>> record;

        PktNumSpace():next_pkt_num(0){};

        ~PktNumSpace(){};

        uint64_t updatepktnum(){
            next_pkt_num += 1;
            return (next_pkt_num - 1);
        };

        void reset(){
            next_pkt_num = 0;
        };
    };
}
#pragma pack(pop)

