#pragma once
#include <gpiod.hpp>
#include "../core/interfaces.hpp"
#include "../core/door_types.hpp"
#include <chrono>
#include <thread>

class WiegandReader : public IDoorComponent, public IEventEmitter {
public:
    WiegandReader(const std::string& doorId, unsigned int data0Pin, unsigned int data1Pin)
        : doorId_(doorId), data0Pin_(data0Pin), data1Pin_(data1Pin) {}

    bool initialize() override {
        try {
            std::cout << "Initializing Wiegand reader on pins D0=" << data0Pin_ << " D1=" << data1Pin_ << std::endl;
            
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            d0_ = chip_->get_line(data0Pin_);
            d1_ = chip_->get_line(data1Pin_);

            // Verify initial state
            std::cout << "Initial pin states - D0: " << d0_.get_value() << " D1: " << d1_.get_value() << std::endl;

            running_ = true;
            readerThread_ = std::thread(&WiegandReader::readerLoop, this);
            std::cout << "Reader thread started successfully" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Reader initialization failed: " << e.what() << std::endl;
            return false;
        }
    }

    void cleanup() override {
        running_ = false;
        if (readerThread_.joinable()) {
            readerThread_.join();
        }
    }

    void registerCallback(std::function<void(const std::string&, const std::string&)> callback) override {
        eventCallback = std::move(callback);
    }

private:
    void readerLoop() {
        std::vector<int> bits;
        auto lastEvent = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(50);  // Standard Wiegand timing
        const auto interBitTimeout = std::chrono::microseconds(5000);  // 5ms between bits
        bool collecting = false;
        int debugPulseCount = 0;
        uint64_t d0Changes = 0, d1Changes = 0;

        // Request lines with event detection
        d0_.request({"door_reader", gpiod::line_request::EVENT_BOTH_EDGES, gpiod::line_request::FLAG_BIAS_PULL_UP});
        d1_.request({"door_reader", gpiod::line_request::EVENT_BOTH_EDGES, gpiod::line_request::FLAG_BIAS_PULL_UP});

        std::cout << "Reader started on D0=" << data0Pin_ << " D1=" << data1Pin_ << std::endl;
        std::cout << "Initial states - D0: " << d0_.get_value() << " D1: " << d1_.get_value() << std::endl;
        
        // Monitor initial states
        int last_d0 = d0_.get_value();
        int last_d1 = d1_.get_value();
        
        while (running_.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Wait for events on both lines with a short timeout
            auto ev_d0 = d0_.event_wait(std::chrono::microseconds(100));
            auto ev_d1 = d1_.event_wait(std::chrono::microseconds(100));
            
            if (ev_d0) {
                auto event = d0_.event_read();
                d0Changes++;
                
                if (event.event_type == gpiod::line_event::FALLING_EDGE) {
                    if (!collecting) {
                        std::cout << "\nStarting new bit collection (D0 pulse)" << std::endl;
                        bits.clear();
                        debugPulseCount = 0;
                        collecting = true;
                    }
                    bits.push_back(0);
                    lastEvent = now;
                    debugPulseCount++;
                    std::cout << "Added bit 0 (D0 pulse)" << std::endl;
                }
            }
            
            if (ev_d1) {
                auto event = d1_.event_read();
                d1Changes++;
                
                if (event.event_type == gpiod::line_event::FALLING_EDGE) {
                    if (!collecting) {
                        std::cout << "\nStarting new bit collection (D1 pulse)" << std::endl;
                        bits.clear();
                        debugPulseCount = 0;
                        collecting = true;
                    }
                    bits.push_back(1);
                    lastEvent = now;
                    debugPulseCount++;
                    std::cout << "Added bit 1 (D1 pulse)" << std::endl;
                }
            }

            // If we're collecting and timeout has passed, process what we have
            if (collecting && now - lastEvent > timeout) {
                if (!bits.empty()) {
                    std::cout << "\nCollection ended after " << debugPulseCount << " pulses" << std::endl;
                    std::cout << "Time since last bit: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEvent).count() << "ms" << std::endl;
                    std::cout << "Total D0 changes: " << d0Changes << ", D1 changes: " << d1Changes << std::endl;
                    
                    // Only process if we have enough bits for a valid card
                    if (bits.size() >= 20) {  // Lowered to catch 26-bit cards that might have missing bits
                        processCard(bits);
                    } else {
                        std::cout << "Discarding short read: " << bits.size() << " bits" << std::endl;
                        std::cout << "Bits received: ";
                        for (int bit : bits) std::cout << bit;
                        std::cout << std::endl;
                    }
                }
                bits.clear();
                collecting = false;
            }
            
            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    void processCard(const std::vector<int>& bits) {
        std::cout << "\nReceived bits: ";
        for(int b : bits) std::cout << b;
        std::cout << " (Length: " << bits.size() << ")\n";

        uint64_t value = 0;
        for (size_t i = 0; i < bits.size(); i++) {
            value = (value << 1) | bits[i];
        }

        std::cout << "Card read - Hex: 0x" << std::hex << std::setfill('0') << std::setw(8) << value 
                  << " Decimal: " << std::dec << value << std::endl;

        // Try to match with your known card format
        if (value == 0x9d3b9f40) {
            std::cout << "Matched known card format!" << std::endl;
        }

        if (eventCallback) {
            nlohmann::json event = {
                {"type", "card_read"},
                {"door_id", doorId_},
                {"card_data", std::to_string(value)},
                {"card_hex", "0x" + std::to_string(value)},
                {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
            };
            eventCallback("door/" + doorId_ + "/card_read", event.dump());
        }
    }

    std::string doorId_;
    unsigned int data0Pin_, data1Pin_;
    std::unique_ptr<gpiod::chip> chip_;
    gpiod::line d0_, d1_;
    std::atomic<bool> running_{false};
    std::thread readerThread_;
};