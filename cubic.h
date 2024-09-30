#pragma once

#include <cmath>
#include <set>
namespace dmludp{

// Congestion Control
//  initial cwnd = min (10*MSS, max (2*MSS, 14600)) 
const size_t INITIAL_WINDOW_PACKETS = 10;

const size_t PACKET_SIZE = 1350;

const size_t INI_WIN = PACKET_SIZE * INITIAL_WINDOW_PACKETS;

const size_t INI_SSTHREAD = PACKET_SIZE * 400;

const double BETA = 0.7;

const double C = 0.4;

const double ROLLBACK_THRESHOLD_PERCENT = 0.8;

const double ALPHA_AIMD = 0.5; // 3.0 * (1.0 - BETA) / (1.0 + BETA) ~= 0.5

enum CongestionControlAlgorithm {
    /// CUBIC congestion control algorithm (default). `cubic` in a string form.
    NEWCUBIC = 1,

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

    std::chrono::system_clock::time_point cubic_time;

    size_t W_max;

    size_t W_last_max;

    double cwndprior;

    double aimdwindow;

    bool is_congestion;

    size_t cwnd_increment;

    size_t ssthread;

    bool is_slow_start;

    bool is_congestion_avoidance;
     
    bool is_recovery;

    bool no_loss;

    size_t parital_allowed;

    bool timeout_recovery;

    double K;

    double cwnd_free;

    Recovery():
    app_limit(false),
    bytes_in_flight(0),
    incre_win(0),
    decre_win(0),
    roll_back_flag(false),
    function_change(false),
    incre_win_copy(0),
    decre_win_copy(0),
    is_congestion(false),
    is_slow_start(true),
    is_congestion_avoidance(false),
    is_recovery(false),
    no_loss(true),
    parital_allowed(INI_WIN),
    timeout_recovery(false),
    cwnd_free(0),
    K(0.0),
    ssthread(INI_SSTHREAD){
        max_datagram_size = PACKET_SIZE;
        congestion_window = 0;
        last_cwnd = INI_WIN;
        W_max = INI_WIN;
        W_last_max = INI_WIN;
    };

    ~Recovery(){};


    void change_status(bool condition_flag){
        is_congestion = condition_flag;
        is_slow_start = !condition_flag;
        W_max = congestion_window;
    }

    void reset() {
        congestion_window = max_datagram_size * INITIAL_WINDOW_PACKETS;
    };

    // K = cubic_root(W_max * (1 - beta_cubic) / C) (Eq. 2)
    void cubic_k(){
        W_max = congestion_window;
        auto w_max = W_max / PACKET_SIZE;

        K = std::cbrt(w_max * (1 - BETA) / C);
        cubic_time = std::chrono::high_resolution_clock::now();
    }

    // W_cubic(t) = C * (t - K)^3 + w_max (Eq. 1)
    double w_cubic(const std::chrono::system_clock::time_point& now) {
        auto w_max = W_max / max_datagram_size;
        auto DeltaT = std::chrono::duration_cast<std::chrono::seconds>(now - cubic_time).count();

        return (C * std::pow(DeltaT - K, 3.0) + w_max) * PACKET_SIZE;
    }

    void update_cubic_status(int status){
        if(status == 1){
            is_slow_start = true;
            is_congestion_avoidance = false;
            is_recovery = false;
        }else if(status == 2){
            is_slow_start = false;
            is_congestion_avoidance = true;
            is_recovery = false;
        }else{
            is_slow_start = false;
            is_congestion_avoidance = false;
            is_recovery = true;
            cubic_k();
            ssthread = congestion_window * BETA;
        }
    }

    void update_win(bool update_cwnd, size_t instant_send = 0, bool timeout_ = false){
        if (update_cwnd){
            if (timeout_){
                no_loss = true;
                update_cubic_status(1);
            }

            if (instant_send * PACKET_SIZE < ROLLBACK_THRESHOLD_PERCENT * cwndprior){
                no_loss = false;

                update_cubic_status(3);
            }else{
                no_loss = true;
            }
        }else{
            cwnd_free += instant_send * PACKET_SIZE;
        }
    }

    size_t cwnd(double RTT){
        // slow start
        if(is_slow_start){
            if (congestion_window < ssthread){
                if (congestion_window == 0){
                    congestion_window = INI_WIN;
                }else{
                    congestion_window *= 2;
                }     
            }else{
                update_cubic_status(2);
            }
        }
        
        // congstion avoidance
        if(is_congestion_avoidance){
            congestion_window *= 1.25;
        }

        // cubic
        if(is_recovery){
            auto now = std::chrono::high_resolution_clock::now();
            auto cubic_cwnd = w_cubic(now);
            auto t = std::chrono::duration_cast<std::chrono::seconds>(now - cubic_time).count();
            auto est_cwn = W_max * BETA + (0.5 * (t / RTT)) * PACKET_SIZE;
            if(cubic_cwnd < 1.5 * W_max && cubic_cwnd > W_max){
                // Reno-friendly
                congestion_window = est_cwn;
            }else{
                // Concave or Convex region
                // [CongestionWindow, 1.5*CongestionWindow]
                congestion_window = std::max(1.5 * est_cwn, std::max(cubic_cwnd, est_cwn));
            }
        }
        cwndprior = congestion_window;
        return congestion_window;
    }
       
    bool transmission_check(){
        return (cwnd_increment == last_cwnd);
    }

    void set_recovery(bool recovery_signal){
        timeout_recovery = recovery_signal;
    };

    size_t cwnd_expect(){
        ssize_t expect_cwnd_ = INI_WIN;
        if(is_slow_start){
            if (congestion_window < ssthread){
		    expect_cwnd_ = congestion_window * 2;
            }else{
                expect_cwnd_ = congestion_window * 1.25;
            }
        }else{
            auto now = std::chrono::system_clock::now();

            // 计算0.1秒后的时间
            auto future_time = now + std::chrono::milliseconds(200);
            expect_cwnd_ = w_cubic(future_time);
        }
        if (expect_cwnd_ < INI_WIN){
            expect_cwnd_ = INI_WIN;
        }
        return expect_cwnd_;
    }


    size_t cwnd_available()  {
        return cwnd_increment;
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
