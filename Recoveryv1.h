#pragma once

#include <cmath>
#include <set>
#include "connection.h"
namespace dmludp{

// Congestion Control
const size_t INITIAL_WINDOW_PACKETS = 10;

const size_t INI_WIN = 1350 * INITIAL_WINDOW_PACKETS;

// const size_t PACKET_SIZE = 1200;
const size_t PACKET_SIZE = 1350;

enum CongestionControlAlgorithm {
    /// CUBIC congestion control algorithm (default). `cubic` in a string form.
    NEWCUBIC = 1,

};

class RecoveryConfig {
    public:
    size_t max_send_udp_payload_size;

    RecoveryConfig():max_send_udp_payload_size(PACKET_SIZE){

    };

    ~RecoveryConfig(){

    };

};

class Recovery{
    public:

    bool app_limit;

    size_t congestion_window;

    size_t bytes_in_flight;

    size_t max_datagram_size;
    // k:f64,
    size_t incre_win;

    size_t decre_win;

    std::set<size_t> former_win_vecter;

    bool roll_back_flag;

    bool function_change;

    size_t incre_win_copy;

    size_t decre_win_copy;

    size_t last_cwnd;

    Recovery():
    app_limit(false),
    // congestion_window(INI_WIN),
    bytes_in_flight(0),
    // max_datagram_size(PACKET_SIZE);
    incre_win(0),
    decre_win(0),
    roll_back_flag(false),
    function_change(false),
    incre_win_copy(0),
    decre_win_copy(0){
        max_datagram_size = PACKET_SIZE;
        congestion_window = INI_WIN;
        last_cwnd = INI_WIN;
    };

    ~Recovery(){};

    void on_init() {
        congestion_window = max_datagram_size * INITIAL_WINDOW_PACKETS;
    };

    void reset() {
        congestion_window = max_datagram_size * INITIAL_WINDOW_PACKETS;
    };

    
    // update patial cwnd size
    // y = 4x^2 − 16x + 8, y = 3x^2 - 12x + 4(current)
    void update_win(float weights, double num){
        double winadd = 0;
        roll_back_flag = false;
        if (weights > 0){
            winadd = (3 * pow((double)weights, 2) - 12 * (double)weights + 4) * (double)max_datagram_size;
            roll_back_flag = true;
        }else{
            winadd = num * (double)max_datagram_size; 
        }

        if (winadd > 0){
            incre_win += (size_t)winadd;
        }else {
            decre_win += (size_t)(-winadd);
        }
    };

    size_t cwnd(){
        size_t tmp_win = 0;
        if (2*incre_win > decre_win){
            tmp_win = 2*incre_win - decre_win;
        }else{
            tmp_win = 0;
        }
        
        if (!roll_back_flag && (tmp_win > INI_WIN)) {
            former_win_vecter.insert(tmp_win);
            if (tmp_win > last_cwnd){
                former_win_vecter.insert(last_cwnd);
            }
        }

        congestion_window = tmp_win;
        last_cwnd = tmp_win;
        parameter_reset();
        if (congestion_window < INI_WIN){
            /// Fix every time, cwnd will start from initial window;
            if (!former_win_vecter.empty()  && incre_win == max_datagram_size){
                congestion_window = *former_win_vecter.rbegin();
            }else{
                congestion_window = INI_WIN;
                tmp_win = INI_WIN;
            }
            return congestion_window;
            // congestion_window = INI_WIN;
            // tmp_win = INI_WIN;
            // return congestion_window;
        }
        return congestion_window;
    };

    size_t rollback(){
        if (former_win_vecter.empty()){
            congestion_window = INI_WIN;
        }else{
            congestion_window = *former_win_vecter.rbegin();
            former_win_vecter.erase(--former_win_vecter.end()); 
            if (last_cwnd == congestion_window && last_cwnd != INI_WIN){
                if (former_win_vecter.empty()){
                    congestion_window = INI_WIN;
                }
                else {
                    congestion_window = *former_win_vecter.rbegin();
                    former_win_vecter.erase(--former_win_vecter.end()); 
                }
            }
        }
        last_cwnd = congestion_window;
        parameter_reset();
        return congestion_window;
    };

    size_t cwnd_available()  {
        return (congestion_window - bytes_in_flight);
    };

    void collapse_cwnd() {
        congestion_window = INI_WIN;
    };

    void update_app_limited(bool v) {
        app_limit = v;
    };

    bool app_limited(){
        return app_limit;
    };
    
    void parameter_reset(){
        incre_win = 0;
        decre_win = 0;
        incre_win_copy = 0;
        decre_win_copy = 0;
    }

};

}