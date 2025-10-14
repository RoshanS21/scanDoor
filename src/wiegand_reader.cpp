#include <gpiod.hpp>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

// Default GPIO numbers (BCM). libgpiod uses chip lines; on Raspberry Pi the chip is usually gpiochip0
static unsigned int DATA0_LINE = 17;
static unsigned int DATA1_LINE = 27;

std::atomic<bool> running(true);

void print_bits(const std::vector<int>& bits) {
    std::cout << "Received " << bits.size() << " bits: ";
    for(int b : bits) std::cout << b;
    std::cout << '\n';

    if(bits.size() == 26) {
        uint32_t value = 0;
        for(size_t i=1;i<25;i++) {
            value <<= 1;
            value |= bits[i];
        }
        std::cout << "26-bit value: " << value << "\n";
    } else if(bits.size() == 34) {
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
