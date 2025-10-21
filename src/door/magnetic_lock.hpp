#pragma once
#include <gpiod.hpp>
#include <thread>
#include <chrono>
#include "../core/interfaces.hpp"

class MagneticLock : public IDoorComponent, public IControllable {
public:
    MagneticLock(const std::string& doorId, unsigned int setPin, unsigned int unsetPin)
        : doorId_(doorId), setPin_(setPin), unsetPin_(unsetPin) {}

    bool initialize() override {
        try {
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            setLine_ = chip_->get_line(setPin_);
            unsetLine_ = chip_->get_line(unsetPin_);
            
            // Configure both lines as outputs
            setLine_.request({"door_lock_set", gpiod::line_request::DIRECTION_OUTPUT});
            unsetLine_.request({"door_lock_unset", gpiod::line_request::DIRECTION_OUTPUT});
            
            // Initialize both lines to low
            setLine_.set_value(0);
            unsetLine_.set_value(0);
            
            // Start in locked state
            setState(true);
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    void cleanup() override {
        setState(true); // Lock on cleanup
    }

    bool setState(bool locked) override {
        try {
            // Latching relay control - pulse the appropriate line
            if (locked) {
                setLine_.set_value(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms pulse
                setLine_.set_value(0);
            } else {
                unsetLine_.set_value(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms pulse
                unsetLine_.set_value(0);
            }
            currentState_ = locked;
            return true;
        } catch (const std::exception& e) {
            return false;
        }
    }

    bool getState() const override {
        return currentState_.load();
    }

private:
    std::string doorId_;
    unsigned int setPin_;    // Pin to engage the lock (SET pin)
    unsigned int unsetPin_;  // Pin to disengage the lock (UNSET pin)
    std::unique_ptr<gpiod::chip> chip_;
    gpiod::line setLine_;    // Control line for SET
    gpiod::line unsetLine_;  // Control line for UNSET
    std::atomic<bool> currentState_{true};
};
