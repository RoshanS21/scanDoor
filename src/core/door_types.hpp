#pragma once
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include "interfaces.hpp"

// Configuration structure for a door
struct DoorConfig
{
    std::string doorId;
    struct
    {
        unsigned int data0Pin;
        unsigned int data1Pin;
    } reader;

    struct
    {
        unsigned int pin;
        bool activeHigh;
    } doorSensor;

    struct
    {
        unsigned int pin;
        bool activeHigh;
    } proximitySensor;

    struct
    {
        unsigned int pin;
        bool activeHigh;
    } exitButton;

    struct
    {
        unsigned int setPin;
        unsigned int unsetPin;
    } lock;
};

class DoorState
{
public:
    bool isLocked{true};
    bool isDoorOpen{false};
    bool isProximityDetected{false};
    bool isExitButtonPressed{false};
    std::string lastCardRead;
    std::chrono::system_clock::time_point lastEventTime;

    nlohmann::json toJson() const
    {
        return
        {
            {"locked", isLocked},
            {"open", isDoorOpen},
            {"proximityDetected", isProximityDetected},
            {"exitButtonPressed", isExitButtonPressed},
            {"lastCard", lastCardRead},
            {"lastEventTime", std::chrono::system_clock::to_time_t(lastEventTime)}
        };
    }
};
