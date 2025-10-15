#pragma once
#include <gpiod.hpp>
#include "../core/interfaces.hpp"

class GpioSensor : public IDoorComponent, public IEventEmitter {
public:
    GpioSensor(const std::string& doorId, unsigned int pin, bool activeHigh, 
               const std::string& sensorType)
        : doorId_(doorId), pin_(pin), activeHigh_(activeHigh), sensorType_(sensorType) {}

    bool initialize() override {
        try {
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            line_ = chip_->get_line(pin_);
            line_.request({"door_sensor", gpiod::line_request::EVENT_BOTH_EDGES});
            
            running_ = true;
            sensorThread_ = std::thread(&GpioSensor::monitorLoop, this);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    void cleanup() override {
        running_ = false;
        if (sensorThread_.joinable()) {
            sensorThread_.join();
        }
    }

    void registerCallback(std::function<void(const std::string&, const std::string&)> callback) override {
        eventCallback = std::move(callback);
    }

    bool getState() const {
        return currentState_.load();
    }

private:
    void monitorLoop() {
        while (running_.load()) {
            auto ev = line_.event_wait(std::chrono::milliseconds(100));
            if (ev) {
                auto event = line_.event_read();
                bool newState = (line_.get_value() == 1) == activeHigh_;
                
                if (newState != currentState_) {
                    currentState_ = newState;
                    if (eventCallback) {
                        nlohmann::json event;
                        event["type"] = sensorType_ + "_change";
                        event["door_id"] = doorId_;
                        event["state"] = currentState_.load();  // Get value from atomic
                        event["timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                        eventCallback("door/" + doorId_ + "/" + sensorType_, event.dump());
                    }
                }
            }
        }
    }

    std::string doorId_;
    unsigned int pin_;
    bool activeHigh_;
    std::string sensorType_;
    std::unique_ptr<gpiod::chip> chip_;
    gpiod::line line_;
    std::atomic<bool> running_{false};
    std::atomic<bool> currentState_{false};
    std::thread sensorThread_;
};