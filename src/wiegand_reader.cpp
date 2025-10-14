#include <gpiod.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <iomanip>  // for std::setfill, std::setw

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

void print_hex(const std::vector<int>& bits) {
    uint64_t value = 0;
    for(size_t i = 0; i < bits.size(); i++) {
        value = (value << 1) | bits[i];
    }
    
    // Print in hex format with leading zeros based on bit size
    size_t hex_digits = (bits.size() + 3) / 4;  // Round up bits/4
    std::cout << "Hex: 0x" << std::hex << std::setfill('0') << std::setw(hex_digits) << value << std::dec << std::endl;
}

void print_bits(const std::vector<int>& bits) {
    std::cout << "Received " << bits.size() << " bits: ";
    for(int b : bits) std::cout << b;
    std::cout << '\n';

    // Print hex representation for all formats
    print_hex(bits);

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
    } else if(bits.size() == 32) {
        uint32_t value = 0;
        for(size_t i = 0; i < 32; i++) {
            value = (value << 1) | bits[i];
        }
        std::cout << "32-bit format - Dec: " << value << std::endl;
    } else if(bits.size() == 34) {
        if(!check_parity_34bit(bits)) {
            std::cout << "Warning: 34-bit format parity check failed!\n";
        }
        uint64_t value = 0;
        for(size_t i=1;i<33;i++) {
            value <<= 1;
            value |= bits[i];
        }
        std::cout << "34-bit format - Dec: " << value << "\n";
    } else if(bits.size() == 64) {
        uint64_t value = 0;
        for(size_t i = 0; i < 64; i++) {
            value = (value << 1) | bits[i];
        }
        std::cout << "64-bit format - Dec: " << value << std::endl;
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
        const auto timeout = std::chrono::milliseconds(30);  // Wiegand spec: >20ms means end of frame
        const auto pulse_timeout = std::chrono::microseconds(100); // Typical Wiegand pulse is 40-100us
        
        std::cout << "Listening on D0=" << DATA0_LINE << " D1=" << DATA1_LINE << " (BCM) using /dev/gpiochip0" << std::endl;
        std::cout << "Waiting for Wiegand data..." << std::endl;

        while(running) {
            // Check both lines with short timeout
            auto evd0 = d0.event_wait(std::chrono::milliseconds(5));
            auto evd1 = d1.event_wait(std::chrono::microseconds(100));

            bool got_bit = false;
            if(evd0) {
                auto ev = d0.event_read();
                // Only count falling edges (start of pulse)
                if(ev.event_type == gpiod::line_event::FALLING_EDGE) {
                    bits.push_back(0);
                    got_bit = true;
                }
            }

            if(evd1) {
                auto ev = d1.event_read();
                if(ev.event_type == gpiod::line_event::FALLING_EDGE) {
                    bits.push_back(1);
                    got_bit = true;
                }
            }

            if(got_bit) {
                last_event = std::chrono::steady_clock::now();
            }

            // Check for frame end (timeout after last bit)
            if(!bits.empty()) {
                auto now = std::chrono::steady_clock::now();
                if(now - last_event > timeout) {
                    // Process all common Wiegand formats
                    if(bits.size() == 26 || bits.size() == 32 || bits.size() == 34 || bits.size() == 64) {
                        print_bits(bits);
                    } else {
                        std::cout << "Got " << bits.size() << " bits - Raw data: ";
                        for(int b : bits) std::cout << b;
                        std::cout << std::endl;
                        print_hex(bits);
                    }
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
