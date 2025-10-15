#pragma once
#include <mosquitto.h>
#include <string>
#include <nlohmann/json.hpp>
#include <functional>
#include <memory>
#include <spdlog/spdlog.h>

class MqttClient {
public:
    MqttClient(const std::string& clientId, const std::string& host = "localhost", int port = 1883)
        : clientId_(clientId), host_(host), port_(port) {
        mosquitto_lib_init();
        mosq_ = mosquitto_new(clientId_.c_str(), true, this);
        if (!mosq_) {
            throw std::runtime_error("Failed to create mosquitto client");
        }
        mosquitto_connect_callback_set(mosq_, onConnect);
        mosquitto_message_callback_set(mosq_, onMessage);
    }

    ~MqttClient() {
        if (mosq_) {
            mosquitto_disconnect(mosq_);
            mosquitto_destroy(mosq_);
        }
        mosquitto_lib_cleanup();
    }

    bool connect() {
        return mosquitto_connect(mosq_, host_.c_str(), port_, 60) == MOSQ_ERR_SUCCESS;
    }

    bool publish(const std::string& topic, const std::string& message) {
        return mosquitto_publish(mosq_, nullptr, topic.c_str(), message.length(),
                               message.c_str(), 0, false) == MOSQ_ERR_SUCCESS;
    }

    bool subscribe(const std::string& topic) {
        return mosquitto_subscribe(mosq_, nullptr, topic.c_str(), 0) == MOSQ_ERR_SUCCESS;
    }

    void setMessageHandler(std::function<void(const std::string&, const std::string&)> handler) {
        messageHandler_ = std::move(handler);
    }

    void loop() {
        mosquitto_loop(mosq_, 0, 1);
    }

private:
    static void onConnect(struct mosquitto* mosq, void* obj, int rc) {
        if (rc == 0) {
            spdlog::info("MQTT Connected successfully");
        } else {
            spdlog::error("MQTT Connect failed with code {}", rc);
        }
    }

    static void onMessage(struct mosquitto* mosq, void* obj, const struct mosquitto_message* msg) {
        auto* client = static_cast<MqttClient*>(obj);
        if (client->messageHandler_) {
            std::string topic(msg->topic);
            std::string payload(static_cast<char*>(msg->payload), msg->payloadlen);
            client->messageHandler_(topic, payload);
        }
    }

    std::string clientId_;
    std::string host_;
    int port_;
    struct mosquitto* mosq_;
    std::function<void(const std::string&, const std::string&)> messageHandler_;
};