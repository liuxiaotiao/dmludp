#pragma once

#include <cmath>
#include <set>
namespace dmludp{

// Congestion Control
//  initial cwnd = min (10*MSS, max (2*MSS, 14600)) 
const size_t INITIAL_WINDOW_PACKETS = 10;

const size_t PACKET_SIZE = 1350;

const size_t INI_WIN = INITIAL_WINDOW_PACKETS * PACKET_SIZE;

const size_t INI_SSTHREAD = 2000;

const double BETA = 0.7;

const double C = 0.4;

const double ROLLBACK_THRESHOLD_PERCENT = 0.8;

const double ALPHA_AIMD = 3.0 * (1.0 - BETA) / (1.0 + BETA); // 3.0 * (1.0 - BETA) / (1.0 + BETA) ~= 0.53

enum CongestionControlAlgorithm {
    /// CUBIC congestion control algorithm (default). `cubic` in a string form.
    NEWCUBIC = 1,

};


class Recovery{
    public:

    bool app_limit;

    size_t bytes_in_flight;

    size_t max_datagram_size;
    
    /*
    Last congestion event status
    prior_cwnd, prior_W_max, prior_cubic_k,
    prior_ssthresh, prior_epoch_start, prior_W_est
    */ 
    double prior_cwnd;

    double prior_W_max;

    double prior_cubic_k;

    double prior_ssthresh;

    std::chrono::system_clock::time_point prior_epoch_start;

    double prior_W_est;

    /*
    Current congestion evet status;
    congestion_window, W_max, K,
    ssthresh, epoch_start, W_est
    */
    double congestion_window;// Bytes

    double W_max;// Bytes

    double ssthresh; // Bytes

    double K; // Seconds

    std::chrono::system_clock::time_point epoch_start;

    double W_est; // Bytes

    size_t cwnd_inc;

    // 1 slow start, 2 congestion avoidance, 3 cubic
    ssize_t congestionEvent;

    double alpha_aimd;

    Recovery(size_t pkt_size):
    app_limit(false),
    bytes_in_flight(0),
    max_datagram_size(pkt_size),
    prior_cwnd(INITIAL_WINDOW_PACKETS * pkt_size),
    prior_W_max(INI_SSTHREAD),
    prior_cubic_k(0.0),
    prior_ssthresh(INI_SSTHREAD),
    prior_W_est(0),
    congestion_window(INITIAL_WINDOW_PACKETS * pkt_size),
    W_max(INI_WIN),
    ssthresh(INI_SSTHREAD * pkt_size),
    K(0.0),
    W_est(0),
    cwnd_inc(0),
    congestionEvent(1),
    alpha_aimd(1){
    };

    ~Recovery(){};


    void reset() {
        congestion_window = INITIAL_WINDOW_PACKETS;
    };

    // K = cubic_root(W_max * (1 - beta_cubic) / C) (Eq. 2)
    void cubic_k(const std::chrono::system_clock::time_point& now){
        W_max = congestion_window;
        auto w_max = W_max / max_datagram_size;

        K = std::cbrt(w_max * (1 - BETA) / C);
        epoch_start = now;
    }

    // W_cubic(t) = C * (t - K)^3 + w_max (Eq. 1)
    double w_cubic(const std::chrono::system_clock::time_point& now) {
        auto w_max = W_max / max_datagram_size;
        auto DeltaT = std::chrono::duration_cast<std::chrono::seconds>(now - epoch_start).count();

        return (C * std::pow(DeltaT - K, 3.0) + w_max) * max_datagram_size;
    }

    double w_est_inc(size_t acked){
        return alpha_aimd * ((double)acked * max_datagram_size /congestion_window) * max_datagram_size;
    }

    void check_point(){
        prior_cwnd = congestion_window;
        prior_W_max = W_max;
        prior_cubic_k = K;
        prior_ssthresh = ssthresh;
        prior_epoch_start = epoch_start;
        prior_W_est = W_est;
    }

    bool rollback() {
        // Don't go back to slow start.
        if (prior_cwnd < prior_ssthresh) {
            return false;
        }

        if (congestion_window >= prior_cwnd) {
            return false;
        }

        congestion_window = prior_cwnd;
        ssthresh = prior_ssthresh;
        W_max = prior_W_max;
        K = prior_cubic_k;
        epoch_start = prior_epoch_start;

        return true;
    }

    void on_packet_ack(int received_packets, int total_packets, 
        const std::chrono::system_clock::time_point& now, std::chrono::seconds min_rtt){
        if(congestionEvent == 1){
            congestion_window += received_packets * max_datagram_size;
            bytes_in_flight -= received_packets * max_datagram_size;
            if (congestion_window >= ssthresh){
                congestionEvent = 2;
            }
        }else if(congestionEvent == 2){
            congestion_window += 1.25 * received_packets * max_datagram_size;
            bytes_in_flight -= received_packets * max_datagram_size;
        }else{
            auto target = w_cubic(now + min_rtt);
            target = std::max(target, congestion_window);
            target = std::min(target, congestion_window * 1.5);
            
            auto west_inc = w_est_inc(received_packets);
            W_est += west_inc;

            if (W_est >= W_max) {
                alpha_aimd = 1.0;
            }

            auto cubic_cwnd = congestion_window;
            auto t = std::chrono::high_resolution_clock::now();
            if (w_cubic(t) < W_est) {
                // AIMD friendly region (W_cubic(t) < W_est)
                cubic_cwnd = std::max(cubic_cwnd, W_est);
            } else {
                // Concave region or convex region use same increment.
                auto cubic_inc = max_datagram_size * (target - cubic_cwnd) / cubic_cwnd;

                cubic_cwnd += cubic_inc;
            }

            // Update the increment and increase cwnd by MSS.
            cwnd_inc += cubic_cwnd - congestion_window;

            if (cwnd_inc >= max_datagram_size) {
                congestion_window += max_datagram_size;
                cwnd_inc -= max_datagram_size;
            }
            if (bytes_in_flight > received_packets * max_datagram_size){
                bytes_in_flight -= received_packets * max_datagram_size;
            }else{
                bytes_in_flight = 0;
            }
        }
    }

    void congestion_event(const std::chrono::system_clock::time_point& now) {
        if(congestion_window < W_max){
            W_max = congestion_window * (1.0 + BETA) / 2.0;
        }else{
            W_max = congestion_window;
        }

        ssthresh = congestion_window * BETA;
        ssthresh = std::max(ssthresh, (double)INI_WIN);
        
        congestion_window = ssthresh;
        if(W_max < congestion_window){
            K = 0;
        }else{
            cubic_k(now);
        }
        cwnd_inc = cwnd_inc * BETA;
        W_est = congestion_window;
        alpha_aimd = ALPHA_AIMD;
        congestionEvent = 3;
        if (congestion_window == INI_WIN){
            congestionEvent = 1;
        }
    }
    /*
    For ack,
    cubic will computes w_cubic(t) and w_est(t) at the same time.

    AIMD-friendly Region
    w_est += alpha_amid * segments_acked / cwnd
    If w_est >= w_max, alpha_amid = 1, otherwise alpha_amid = 0.53 (3.0 * (1.0 - BETA) / (1.0 + BETA))

    concave:
    (target-cwnd)/cwnd
    convex
    (target-cwnd)/cwnd


    When congestion occurs, 
    If congestion_window > w_max,
        w_max = congestion_window
    else
        w_max = (1 + Beta) / 2 * congestion_window


    Time out
    w_max = Beta * cwnd
    k = 0
    */

    // if congestion < last w_max, resize to wmax=(1 + beta)*cwnd
    void on_packet_sent(size_t pktlen){
        bytes_in_flight += pktlen;
    }

    size_t cwnd_available()  {
        if(bytes_in_flight > congestion_window){
            return 0;
        }
        return congestion_window - bytes_in_flight;
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

};

}
