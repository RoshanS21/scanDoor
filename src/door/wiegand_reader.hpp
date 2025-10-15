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
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            d0_ = chip_->get_line(data0Pin_);
            d1_ = chip_->get_line(data1Pin_);

            d0_.request({"door_reader", gpiod::line_request::EVENT_BOTH_EDGES});
            d1_.request({"door_reader", gpiod::line_request::EVENT_BOTH_EDGES});

            running_ = true;
            readerThread_ = std::thread(&WiegandReader::readerLoop, this);
            return true;
        } catch (const std::exception& e) {
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
        const auto timeout = std::chrono::milliseconds(30);
        bool collecting = false;

        while (running_.load()) {
            // Check both lines with minimal delay between them
            auto evd0 = d0_.event_wait(std::chrono::milliseconds(1));
            auto evd1 = d1_.event_wait(std::chrono::milliseconds(1));

            bool gotBit = false;

            if (evd0) {
                auto ev = d0_.event_read();
                if (ev.event_type == gpiod::line_event::FALLING_EDGE) {
                    bits.push_back(0);
                    lastEvent = std::chrono::steady_clock::now();
                    gotBit = true;
                    collecting = true;
                }
            }

            if (evd1) {
                auto ev = d1_.event_read();
                if (ev.event_type == gpiod::line_event::FALLING_EDGE) {
                    bits.push_back(1);
                    lastEvent = std::chrono::steady_clock::now();
                    gotBit = true;
                    collecting = true;
                }
            }

            // If we're collecting and haven't gotten a bit recently, process what we have
            if (collecting && std::chrono::steady_clock::now() - lastEvent > timeout) {
                if (!bits.empty()) {
                    processCard(bits);
                }
                bits.clear();
                collecting = false;
            }

            if (!bits.empty()) {
                auto now = std::chrono::steady_clock::now();
                if (now - lastEvent > timeout) {
                    processCard(bits);
                    bits.clear();
                }
            }
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