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

        while (running_.load()) {
            auto evd0 = d0_.event_wait(std::chrono::milliseconds(50));
            if (evd0) {
                auto ev = d0_.event_read();
                if (ev.event_type == gpiod::line_event::FALLING_EDGE) {
                    bits.push_back(0);
                    lastEvent = std::chrono::steady_clock::now();
                }
            }

            auto evd1 = d1_.event_wait(std::chrono::microseconds(100));
            if (evd1) {
                auto ev = d1_.event_read();
                if (ev.event_type == gpiod::line_event::FALLING_EDGE) {
                    bits.push_back(1);
                    lastEvent = std::chrono::steady_clock::now();
                }
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
        if (bits.size() != 32) return; // We're only handling 32-bit cards for now

        uint32_t value = 0;
        for (size_t i = 0; i < 32; i++) {
            value = (value << 1) | bits[i];
        }

        if (eventCallback) {
            nlohmann::json event = {
                {"type", "card_read"},
                {"door_id", doorId_},
                {"card_data", std::to_string(value)},
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