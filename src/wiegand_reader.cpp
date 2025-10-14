#include <gpiod.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>

// Default GPIO numbers (BCM). libgpiod uses chip lines; on Raspberry Pi the chip is usually gpiochip0
static unsigned int DATA0_LINE = 17;
static unsigned int DATA1_LINE = 27;

std::atomic<bool> running(true);

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        running.store(false);
    }
}

bool check_parity_26bit(const std::vector<int>& bits) {
    if(bits.size() != 26) return false;
    
    // Even parity (bit 0) over bits 1-12
    bool even_parity = bits[0];
    int count1 = 0;
    for(int i = 1; i <= 12; i++) {
        if(bits[i]) count1++;
    }
    if((count1 % 2) == even_parity) return false;
    
    // Odd parity (bit 25) over bits 13-24
    bool odd_parity = bits[25];
    count1 = 0;
    for(int i = 13; i <= 24; i++) {
        if(bits[i]) count1++;
    }
    if((count1 % 2) != odd_parity) return false;
    
    return true;
}

bool check_parity_34bit(const std::vector<int>& bits) {
    if(bits.size() != 34) return false;
    
    // Even parity (bit 0) over bits 1-16
    bool even_parity = bits[0];
    int count1 = 0;
    for(int i = 1; i <= 16; i++) {
        if(bits[i]) count1++;
    }
    if((count1 % 2) == even_parity) return false;
    
    // Odd parity (bit 33) over bits 17-32
    bool odd_parity = bits[33];
    count1 = 0;
    for(int i = 17; i <= 32; i++) {
        if(bits[i]) count1++;
    }
    if((count1 % 2) != odd_parity) return false;
    
    return true;
}

void print_bits(const std::vector<int>& bits) {
    std::cout << "Received " << bits.size() << " bits: ";
    for(int b : bits) std::cout << b;
    std::cout << '\n';

    if(bits.size() == 26) {
        if(!check_parity_26bit(bits)) {
            std::cout << "Warning: 26-bit format parity check failed!\n";
        }
        uint32_t facility = 0;
        uint32_t card = 0;
        // Facility code is bits 1-8
        for(size_t i=1;i<=8;i++) {
            facility <<= 1;
            facility |= bits[i];
        }
        // Card number is bits 9-24
        for(size_t i=9;i<=24;i++) {
            card <<= 1;
            card |= bits[i];
        }
        std::cout << "26-bit format - Facility: " << facility << " Card: " << card << "\n";
    } else if(bits.size() == 34) {
        if(!check_parity_34bit(bits)) {
            std::cout << "Warning: 34-bit format parity check failed!\n";
        }
        uint64_t value = 0;
        for(size_t i=1;i<33;i++) {
            value <<= 1;
            value |= bits[i];
        }
        std::cout << "34-bit value: " << value << "\n";
    }
}

int main(int argc, char** argv) {
    if(argc >= 3) {
        DATA0_LINE = std::stoul(argv[1]);
        DATA1_LINE = std::stoul(argv[2]);
    }

    // Install signal handlers for clean shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    try {
        gpiod::chip chip("/dev/gpiochip0");

        // Request both lines as inputs with both-edge events
        gpiod::line d0 = chip.get_line(DATA0_LINE);
        gpiod::line d1 = chip.get_line(DATA1_LINE);

        d0.request({"wiegand", gpiod::line_request::EVENT_BOTH_EDGES});
        d1.request({"wiegand", gpiod::line_request::EVENT_BOTH_EDGES});

        std::vector<int> bits;
        auto last_event = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(30);

        std::cout << "Listening on D0=" << DATA0_LINE << " D1=" << DATA1_LINE << " (BCM) using /dev/gpiochip0" << std::endl;

        while(running) {
            // Wait for events with short timeout so we can check for frame end
            auto evd0 = d0.event_wait(std::chrono::milliseconds(50));
            if(evd0) {
                auto ev = d0.event_read();
                // D0 low pulse indicates a 0 bit in Wiegand
                if(ev.type == gpiod::line_event::FALLING_EDGE || ev.type == gpiod::line_event::RISING_EDGE) {
                    // Many readers pull the line low briefly for a bit; interpret falling as the bit
                    // We'll treat any event on D0 as a 0
                    bits.push_back(0);
                    last_event = std::chrono::steady_clock::now();
                }
            }

            auto evd1 = d1.event_wait(std::chrono::milliseconds(1));
            if(evd1) {
                auto ev = d1.event_read();
                // D1 event -> bit 1
                bits.push_back(1);
                last_event = std::chrono::steady_clock::now();
            }

            // Check for timeout (no new bits)
            if(!bits.empty()) {
                auto now = std::chrono::steady_clock::now();
                if(now - last_event > timeout) {
                    print_bits(bits);
                    bits.clear();
                }
            }
        }

    } catch(const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 2;
    }

    return 0;
}
