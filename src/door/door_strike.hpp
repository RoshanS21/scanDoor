#pragma once
#include <gpiod.hpp>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "../core/interfaces.hpp"

class DoorStrike : public IDoorComponent, public IControllable {
public:
    DoorStrike(const std::string& doorId, unsigned int unsetPin, unsigned int setPin, unsigned int unlockDurationMs = 1000)
        : doorId_(doorId), unsetPin_(unsetPin), setPin_(setPin), unlockDurationMs_(unlockDurationMs) {}

    bool initialize() override {
        try {
            chip_ = std::make_unique<gpiod::chip>("/dev/gpiochip0");
            unsetLine_ = chip_->get_line(unsetPin_);
            setLine_ = chip_->get_line(setPin_);
            
            // Configure both lines as outputs
            unsetLine_.request({"door_strike_unset", gpiod::line_request::DIRECTION_OUTPUT});
            setLine_.request({"door_strike_set", gpiod::line_request::DIRECTION_OUTPUT});
            
            // Initialize both lines to low
            unsetLine_.set_value(0);
            setLine_.set_value(0);
            
            // Start in locked state
            setState(true);
            spdlog::info("Door strike {} initialized in locked state", doorId_);
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to initialize door strike {}: {}", doorId_, e.what());
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
                // Locked state: pulse UNSET pin (default/safe state)
                spdlog::info("Locking door strike {}", doorId_);
                unsetLine_.set_value(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms pulse
                unsetLine_.set_value(0);
                currentState_ = true;
            } else {
                // Unlocked state: pulse SET pin to engage strike
                spdlog::info("Unlocking door strike {} for {}ms", doorId_, unlockDurationMs_);
                setLine_.set_value(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(50)); // 50ms pulse
                setLine_.set_value(0);
                currentState_ = false;
                
                // Auto-lock after unlock duration
                std::thread([this]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(unlockDurationMs_));
                    if (!currentState_) {  // Only auto-lock if still in unlocked state
                        spdlog::info("Auto-locking door strike {} after timeout", doorId_);
                        unsetLine_.set_value(1);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        unsetLine_.set_value(0);
                        currentState_ = true;
                    }
                }).detach();
            }
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Failed to set door strike {} state: {}", doorId_, e.what());
            return false;
        }
    }

    bool getState() const override {
        return currentState_.load();
    }

private:
    std::string doorId_;
    unsigned int unsetPin_;         // Pin to set strike to locked state (default/safe)
    unsigned int setPin_;           // Pin to set strike to unlocked state (engaged)
    unsigned int unlockDurationMs_; // How long to keep strike engaged when unlocking
    std::unique_ptr<gpiod::chip> chip_;
    gpiod::line unsetLine_;         // Control line for UNSET (lock)
    gpiod::line setLine_;           // Control line for SET (unlock)
    std::atomic<bool> currentState_{true};  // true = locked, false = unlocked
};
