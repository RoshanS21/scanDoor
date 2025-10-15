#pragma once
#include <gpiod.hpp>
#include "../core/interfaces.hpp"
#include "../core/door_types.hpp"
#include <chrono>
#include <thread>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>

class WiegandReader : public IDoorComponent, public IEventEmitter {
private:
    std::string doorId_;
    unsigned int data0Pin_, data1Pin_;
    std::unique_ptr<gpiod::chip> chip_;
    gpiod::line d0_, d1_;
    std::atomic<bool> running_{false};
    std::thread readerThread_;
    std::function<void(const std::string&, const std::string&)> eventCallback;

public:
    WiegandReader(const std::string& doorId, unsigned int data0Pin, unsigned int data1Pin)
        : doorId_(doorId), data0Pin_(data0Pin), data1Pin_(data1Pin) {}

    bool initialize() override {
        try {
            data0Pin_ = 22;  // Using GPIO22 for D0 (more reliable than GPIO17)
            
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            d0_ = chip_->get_line(data0Pin_);
            d1_ = chip_->get_line(data1Pin_);

            // Configure GPIO for Wiegand reader
            gpiod::line_request config{
                .consumer = "door_reader",
                .request_type = gpiod::line_request::EVENT_BOTH_EDGES,
                .flags = gpiod::line_request::FLAG_BIAS_PULL_UP
            };

            d0_.request(config);
            d1_.request(config);

            spdlog::info("Wiegand reader initialized on D0={} D1={}", data0Pin_, data1Pin_);

            running_ = true;
            readerThread_ = std::thread(&WiegandReader::readerLoop, this);
            spdlog::info("Reader thread started successfully");
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Reader initialization failed: {}", e.what());
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
        bool collecting = false;

        spdlog::info("Reader started on D0={} D1={}", data0Pin_, data1Pin_);
        
        while (running_.load()) {
            auto now = std::chrono::steady_clock::now();
            
            // Wait for events on both lines
            auto ev_d0 = d0_.event_wait(std::chrono::microseconds(100));
            auto ev_d1 = d1_.event_wait(std::chrono::microseconds(100));
            
            if (ev_d0) {
                auto event = d0_.event_read();
                if (event.event_type == gpiod::line_event::FALLING_EDGE) {
                    if (!collecting) {
                        bits.clear();
                        collecting = true;
                    }
                    bits.push_back(0);
                    lastEvent = now;
                }
            }
            
            if (ev_d1) {
                auto event = d1_.event_read();
                if (event.event_type == gpiod::line_event::FALLING_EDGE) {
                    if (!collecting) {
                        bits.clear();
                        collecting = true;
                    }
                    bits.push_back(1);
                    lastEvent = now;
                }
            }

            // Process collected bits after timeout
            if (collecting && now - lastEvent > timeout) {
                if (bits.size() == 32) {
                    processCard(bits);
                }
                bits.clear();
                collecting = false;
            }
            
            // Prevent busy waiting
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    void processCard(const std::vector<int>& bits) {
        // Calculate parity bits and validate
        bool evenParity = bits[0];
        bool oddParity = bits[31];
        int evenCount = std::count_if(bits.begin(), bits.begin() + 16, [](int b) { return b == 1; });
        int oddCount = std::count_if(bits.begin() + 16, bits.end(), [](int b) { return b == 1; });
        bool parityValid = (evenCount % 2 == 0) && (oddCount % 2 == 1);

        // Extract card data
        uint16_t facilityCode = 0;
        for (int i = 1; i < 9; i++) {
            facilityCode = (facilityCode << 1) | bits[i];
        }

        uint32_t cardNumber = 0;
        for (int i = 9; i < 25; i++) {
            cardNumber = (cardNumber << 1) | bits[i];
        }

        uint32_t fullValue = std::accumulate(bits.begin(), bits.end(), 0u, 
            [](uint32_t acc, int bit) { return (acc << 1) | bit; });

        // Log card details
        std::stringstream hexValue;
        hexValue << "0x" << std::hex << std::setfill('0') << std::setw(8) << fullValue;

        spdlog::info("Card Read - FC:{} CN:{} Raw:{} Parity:{}",
            facilityCode, cardNumber, hexValue.str(), parityValid ? "Valid" : "Invalid");

        // Known card check
        bool isAuthorized = (fullValue == 0x9d3b9f40);
        spdlog::info("Access {}", isAuthorized ? "Granted" : "Denied");

        // Emit MQTT event
        if (eventCallback) {
            nlohmann::json event = nlohmann::json{
                {"event", "access_attempt"},
                {"door_id", doorId_},
                {"card", nlohmann::json{
                    {"raw", hexValue.str()},
                    {"facility_code", facilityCode},
                    {"number", cardNumber}
                }},
                {"access", nlohmann::json{
                    {"granted", fullValue == 0x9d3b9f40},
                    {"parity_valid", parityValid}
                }},
                {"timestamp", std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())}
            };
            eventCallback("access/" + doorId_, event.dump());
        }
    }
};
