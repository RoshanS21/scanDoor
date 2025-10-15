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

            // Configure with pull-ups and falling edge detection
            gpiod::line_request req;
            req.consumer = "door_reader";
            req.request_type = gpiod::line_request::EVENT_FALLING_EDGE;
            req.flags = gpiod::line_request::FLAG_BIAS_PULL_UP;

            d0_.request(req);
            d1_.request(req);

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
        const auto timeout = std::chrono::milliseconds(50);
        const auto debounceTime = std::chrono::microseconds(500);
        bool collecting = false;
        int debugPulseCount = 0;

        std::cout << "Reader started on D0=" << data0Pin_ << " D1=" << data1Pin_ << std::endl;
        
        // Monitor initial states
        int last_d0 = d0_.get_value();
        int last_d1 = d1_.get_value();
        
        while (running_.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Read current values
            int current_d0 = d0_.get_value();
            int current_d1 = d1_.get_value();
            
            // Debug state changes
            if (current_d0 != last_d0) {
                std::cout << "D0 changed: " << last_d0 << " -> " << current_d0 << std::endl;
                last_d0 = current_d0;
            }
            if (current_d1 != last_d1) {
                std::cout << "D1 changed: " << last_d1 << " -> " << current_d1 << std::endl;
                last_d1 = current_d1;
            }

            // Only process if we're not in debounce time
            if (now - lastEvent > debounceTime) {
                // D0 = 0 bit
                if (current_d0 == 0) {
                    if (!collecting) {
                        std::cout << "\nStarting new bit collection (D0 triggered)" << std::endl;
                        bits.clear();
                        debugPulseCount = 0;
                        collecting = true;
                    }
                    bits.push_back(0);
                    lastEvent = now;
                    debugPulseCount++;
                }

                // D1 = 1 bit
                if (current_d1 == 0) {
                    if (!collecting) {
                        std::cout << "\nStarting new bit collection (D1 triggered)" << std::endl;
                        bits.clear();
                        debugPulseCount = 0;
                        collecting = true;
                    }
                    bits.push_back(1);
                    lastEvent = now;
                    debugPulseCount++;
                }
            }

            // If we're collecting and haven't gotten a bit recently, process what we have
            if (collecting && now - lastEvent > timeout) {
                if (!bits.empty()) {
                    std::cout << "Pulse count during collection: " << debugPulseCount << std::endl;
                    std::cout << "Time since last bit: " << std::chrono::duration_cast<std::chrono::milliseconds>(now - lastEvent).count() << "ms" << std::endl;
                    
                    // Only process if we have enough bits for a valid card
                    if (bits.size() >= 26) {
                        processCard(bits);
                    } else {
                        std::cout << "Discarding short read: " << bits.size() << " bits" << std::endl;
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