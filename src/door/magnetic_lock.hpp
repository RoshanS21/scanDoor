#pragma once
#include <gpiod.hpp>
#include "../core/interfaces.hpp"

class MagneticLock : public IDoorComponent, public IControllable {
public:
    MagneticLock(const std::string& doorId, unsigned int pin, bool activeLow)
        : doorId_(doorId), pin_(pin), activeLow_(activeLow) {}

    bool initialize() override {
        try {
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            line_ = chip_->get_line(pin_);
            line_.request({"door_lock", gpiod::line_request::DIRECTION_OUTPUT});
            setState(true); // Start locked
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
            // Convert logical state to physical state based on active-low configuration
            int value = (locked != activeLow_) ? 1 : 0;
            line_.set_value(value);
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
    unsigned int pin_;
    bool activeLow_;
    std::unique_ptr<gpiod::chip> chip_;
    gpiod::line line_;
    std::atomic<bool> currentState_{true};
};
