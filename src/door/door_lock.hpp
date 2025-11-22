#pragma once
#include "gpio_compat.hpp"
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include "../core/interfaces.hpp"

class DoorLock : public IDoorComponent, public IControllable
{
public:
    DoorLock(const std::string& doorId, unsigned int setPin, unsigned int unsetPin)
        : doorId_(doorId)
        , setPin_(setPin)
        , unsetPin_(unsetPin)
    {
        // Set pin connects COM to NC
        // Unset pin connects COM to NO
    }

    bool initialize() override
    {
        try
        {
            chip_ = std::make_unique<gpio_compat::Chip>("/dev/gpiochip0");
            setLine_ = chip_->get_line(setPin_);
            unsetLine_ = chip_->get_line(unsetPin_);
            
            // Configure both lines as outputs
            setLine_.request("door_lock_set", gpio_compat::Direction::OUTPUT);
            unsetLine_.request("door_lock_unset", gpio_compat::Direction::OUTPUT);
            
            // Initialize both lines to low
            setLine_.set_value(0);
            unsetLine_.set_value(0);
            
            // Start in locked state
            setState(true);
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }

    void cleanup() override
    {
        setState(true); // Lock on cleanup
    }

    bool setState(bool locked) override
    {
        try
        {
            // Latching relay control - pulse the appropriate line
            // Since it's a latching relay, we only need to pulse the set or unset line briefly
            const int pulseDurationMs = 50;
            if (locked)
            {
                spdlog::info("Locking door {}", doorId_);
                setLine_.set_value(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(pulseDurationMs));
                setLine_.set_value(0);
            } else
            {
                spdlog::info("Unlocking door {}", doorId_);
                unsetLine_.set_value(1);
                std::this_thread::sleep_for(std::chrono::milliseconds(pulseDurationMs));
                unsetLine_.set_value(0);
            }
            currentState_ = locked;
            return true;
        }
        catch (const std::exception& e)
        {
            return false;
        }
    }

    bool getState() const override
    {
        return currentState_.load();
    }

private:
    std::string doorId_;
    unsigned int setPin_;
    unsigned int unsetPin_;
    std::unique_ptr<gpio_compat::Chip> chip_;
    gpio_compat::Line setLine_;
    gpio_compat::Line unsetLine_;
    std::atomic<bool> currentState_{true};
};
