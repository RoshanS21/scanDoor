#include <iostream>
#include <vector>
#include <signal.h>
#include "door/door.hpp"
#include "mqtt/mqtt_client.hpp"
#include "utils/logger.hpp"

std::atomic<bool> running(true);

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        running.store(false);
    }
}

int main(int argc, char** argv) {
    // Setup signal handling
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // Initialize global logger
    auto logger = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(logger);
    logger->info("Door Control System Starting...");

    try {
        // Initialize MQTT client
        auto mqtt = std::make_shared<MqttClient>("door_controller");
        if (!mqtt->connect()) {
            logger->error("Failed to connect to MQTT broker");
            return 1;
        }

        // Configure doors
        std::vector<DoorConfig> doorConfigs = {
            {
                .doorId = "front",
                .reader = {17, 27},      // DATA0, DATA1
                .doorSensor = {22, true}, // GPIO22, active high
                .proximitySensor = {23, true}, // GPIO23, active high
                .exitButton = {24, true}, // GPIO24, active high
                .lock = {25, true}        // GPIO25, active low
            }
            // Add more doors as needed
        };

        // Initialize doors
        std::vector<std::unique_ptr<Door>> doors;
        for (const auto& config : doorConfigs) {
            auto door = std::make_unique<Door>(config, mqtt);
            if (!door->initialize()) {
                logger->error("Failed to initialize door {}", config.doorId);
                return 1;
            }
            doors.push_back(std::move(door));
        }

        logger->info("All doors initialized. Running main loop...");

        // Main loop
        while (running) {
            mqtt->loop();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // Cleanup
        logger->info("Shutting down...");
        for (auto& door : doors) {
            door->cleanup();
        }

    } catch (const std::exception& e) {
        logger->error("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}