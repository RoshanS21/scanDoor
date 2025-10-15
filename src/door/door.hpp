#pragma once
#include <memory>
#include <spdlog/spdlog.h>
#include "../core/door_types.hpp"
#include "../utils/logger.hpp"
#include <nlohmann/json.hpp>
#include "wiegand_reader.hpp"
#include "gpio_sensor.hpp"
#include "magnetic_lock.hpp"
#include "../mqtt/mqtt_client.hpp"

class Door {
public:
    Door(const DoorConfig& config, std::shared_ptr<MqttClient> mqtt)
        : config_(config), mqtt_(mqtt) {
        // Initialize components
        reader_ = std::make_unique<WiegandReader>(config.doorId, 
                                                config.reader.data0Pin,
                                                config.reader.data1Pin);
        
        doorSensor_ = std::make_unique<GpioSensor>(config.doorId,
                                                  config.doorSensor.pin,
                                                  config.doorSensor.activeHigh,
                                                  "door_sensor");
        
        proximitySensor_ = std::make_unique<GpioSensor>(config.doorId,
                                                       config.proximitySensor.pin,
                                                       config.proximitySensor.activeHigh,
                                                       "proximity");
        
        exitButton_ = std::make_unique<GpioSensor>(config.doorId,
                                                  config.exitButton.pin,
                                                  config.exitButton.activeHigh,
                                                  "exit_button");
        
        lock_ = std::make_unique<MagneticLock>(config.doorId,
                                              config.lock.pin,
                                              config.lock.activeLow);

        // Initialize logger
        Logger::initialize(config.doorId);
        logger_ = Logger::getDoorLogger(config.doorId);

        // Setup MQTT subscriptions
        setupMqttHandlers();
    }

    bool initialize() {
        bool success = true;
        
        // Reader is required
        if (!reader_->initialize()) {
            logger_->error("Door {} - Failed to initialize card reader", config_.doorId);
            return false;
        }
        
        // Optional components - log warnings but don't fail if they're not connected
        if (doorSensor_) {
            if (!doorSensor_->initialize()) {
                logger_->warn("Door {} - Door sensor initialization failed, continuing without it", config_.doorId);
            }
        }
        
        if (proximitySensor_) {
            if (!proximitySensor_->initialize()) {
                logger_->warn("Door {} - Proximity sensor initialization failed, continuing without it", config_.doorId);
            }
        }
        
        if (exitButton_) {
            if (!exitButton_->initialize()) {
                logger_->warn("Door {} - Exit button initialization failed, continuing without it", config_.doorId);
            }
        }
        
        if (lock_) {
            if (!lock_->initialize()) {
                logger_->warn("Door {} - Lock initialization failed, continuing without it", config_.doorId);
            }
        }

        setupEventHandlers();
        logger_->info("Door {} initialized with card reader", config_.doorId);
        return true;
    }

    void cleanup() {
        if (reader_) reader_->cleanup();
        if (doorSensor_) doorSensor_->cleanup();
        if (proximitySensor_) proximitySensor_->cleanup();
        if (exitButton_) exitButton_->cleanup();
        if (lock_) lock_->cleanup();
    }

private:
    void setupEventHandlers() {
        // Card reader events
        reader_->registerCallback([this](const std::string& topic, const std::string& message) {
            handleCardRead(message);
            mqtt_->publish(topic, message);
            logger_->info("Card read event on door {}: {}", config_.doorId, message);
        });

        // Door sensor events
        doorSensor_->registerCallback([this](const std::string& topic, const std::string& message) {
            state_.isDoorOpen = doorSensor_->getState();
            mqtt_->publish(topic, message);
            logger_->info("Door sensor event on door {}: {}", config_.doorId, message);
        });

        // Proximity sensor events
        proximitySensor_->registerCallback([this](const std::string& topic, const std::string& message) {
            state_.isProximityDetected = proximitySensor_->getState();
            handleProximityEvent();
            mqtt_->publish(topic, message);
            logger_->info("Proximity event on door {}: {}", config_.doorId, message);
        });

        // Exit button events
        exitButton_->registerCallback([this](const std::string& topic, const std::string& message) {
            state_.isExitButtonPressed = exitButton_->getState();
            handleExitButtonEvent();
            mqtt_->publish(topic, message);
            logger_->info("Exit button event on door {}: {}", config_.doorId, message);
        });
    }

    void setupMqttHandlers() {
        mqtt_->subscribe("door/" + config_.doorId + "/command");
        mqtt_->setMessageHandler([this](const std::string& topic, const std::string& payload) {
            handleMqttCommand(payload);
        });
    }

    void handleCardRead(const std::string& message) {
        // TODO: Implement card validation logic
        // For now, just unlock the door temporarily
        unlockTemporarily();
    }

    void handleProximityEvent() {
        if (state_.isProximityDetected) {
            unlockTemporarily();
        }
    }

    void handleExitButtonEvent() {
        if (state_.isExitButtonPressed) {
            unlockTemporarily();
        }
    }

    void handleMqttCommand(const std::string& payload) {
        try {
            auto cmd = nlohmann::json::parse(payload);
            if (cmd["action"] == "unlock") {
                unlockTemporarily();
            } else if (cmd["action"] == "lock") {
                lock_->setState(true);
            } else if (cmd["action"] == "status") {
                publishStatus();
            }
        } catch (const std::exception& e) {
            logger_->error("Failed to parse MQTT command: {}", e.what());
        }
    }

    void unlockTemporarily() {
        lock_->setState(false);
        state_.isLocked = false;
        publishStatus();

        // Start a thread to re-lock after delay
        std::thread([this]() {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            lock_->setState(true);
            state_.isLocked = true;
            publishStatus();
        }).detach();
    }

    void publishStatus() {
        nlohmann::json status = state_.toJson();
        mqtt_->publish("door/" + config_.doorId + "/status", status.dump());
    }

    DoorConfig config_;
    DoorState state_;
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<MqttClient> mqtt_;

    std::unique_ptr<WiegandReader> reader_;
    std::unique_ptr<GpioSensor> doorSensor_;
    std::unique_ptr<GpioSensor> proximitySensor_;
    std::unique_ptr<GpioSensor> exitButton_;
    std::unique_ptr<MagneticLock> lock_;
};