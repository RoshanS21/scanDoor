#pragma once

// Base interface for all door components
class IDoorComponent {
public:
    virtual ~IDoorComponent() = default;
    virtual bool initialize() = 0;
    virtual void cleanup() = 0;
};

// Base interface for components that can emit events
class IEventEmitter {
public:
    virtual ~IEventEmitter() = default;
    virtual void registerCallback(std::function<void(const std::string&, const std::string&)> callback) = 0;
protected:
    std::function<void(const std::string&, const std::string&)> eventCallback;
};

// Base interface for components that can be controlled
class IControllable {
public:
    virtual ~IControllable() = default;
    virtual bool setState(bool state) = 0;
    virtual bool getState() const = 0;
};