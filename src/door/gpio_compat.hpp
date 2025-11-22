#pragma once
#include <gpiod.hpp>
#include <memory>
#include <chrono>
#include <optional>
#include <spdlog/spdlog.h>

// Compatibility layer for libgpiod v1.x and v2.x
// This provides a unified interface for both versions

namespace gpio_compat
{
    // Event types (normalized across versions)
    enum class EdgeEvent
    {
        RISING_EDGE,
        FALLING_EDGE
    };

    enum class Direction
    {
        INPUT,
        OUTPUT
    };

    class Line;

    // Abstraction for gpiod::chip
    class Chip
    {
    public:
        explicit Chip(const std::string& device_path);
        Line get_line(unsigned int offset);

    private:
        std::unique_ptr<gpiod::chip> chip_;
    };

    // Abstraction for gpiod::line with unified API
    class Line
    {
    public:
        Line() = default;

        void request(const std::string& consumer, Direction direction, bool bias_pull_up = false);
        void request_events(const std::string& consumer, bool bias_pull_up = false);
        
        int get_value() const;
        void set_value(int value);
        
        bool event_wait(std::chrono::microseconds timeout);
        std::optional<EdgeEvent> event_read();

    private:
        friend class Chip;
        unsigned int offset_ = 0;

#ifdef GPIOD_V2_API
        // libgpiod v2.x - using request objects
        gpiod::chip* chip_ptr_ = nullptr;
        std::shared_ptr<gpiod::line_request> request_;
        std::optional<gpiod::edge_event> last_event_;

        void init_v2(gpiod::chip* chip, unsigned int offset) 
        { 
            chip_ptr_ = chip; 
            offset_ = offset;
        }
#else
        // libgpiod v1.x - direct line objects
        gpiod::line line_;

        void init_v1(gpiod::line line) { line_ = line; }
#endif
    };

    // ============ Implementation ============

    // Chip implementation
    inline Chip::Chip(const std::string& device_path)
    {
        try
        {
            chip_ = std::make_unique<gpiod::chip>(device_path);
            spdlog::debug("GPIO chip initialized from {}", device_path);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to initialize GPIO chip: {}", e.what());
            throw;
        }
    }

#ifdef GPIOD_V2_API
    // ============ libgpiod v2.x Implementation ============

    inline Line Chip::get_line(unsigned int offset)
    {
        try
        {
            Line wrapped;
            wrapped.init_v2(chip_.get(), offset);
            return wrapped;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to get GPIO line {}: {}", offset, e.what());
            throw;
        }
    }

    inline void Line::request(const std::string& consumer, Direction direction, bool bias_pull_up)
    {
        try
        {
            gpiod::line_settings settings;
            
            if (direction == Direction::OUTPUT)
            {
                settings.set_direction(gpiod::line::direction::OUTPUT);
            }
            else
            {
                settings.set_direction(gpiod::line::direction::INPUT);
                if (bias_pull_up)
                {
                    settings.set_bias(gpiod::line::bias::PULL_UP);
                }
            }

            auto builder = chip_ptr_->prepare_request();
            builder.set_consumer(consumer);
            builder.add_line_settings(offset_, settings);
            request_ = std::make_shared<gpiod::line_request>(builder.do_request());
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to request GPIO line: {}", e.what());
            throw;
        }
    }

    inline void Line::request_events(const std::string& consumer, bool bias_pull_up)
    {
        try
        {
            gpiod::line_settings settings;
            settings.set_direction(gpiod::line::direction::INPUT);
            settings.set_edge_detection(gpiod::line::edge::BOTH);
            
            if (bias_pull_up)
            {
                settings.set_bias(gpiod::line::bias::PULL_UP);
            }

            auto builder = chip_ptr_->prepare_request();
            builder.set_consumer(consumer);
            builder.add_line_settings(offset_, settings);
            request_ = std::make_shared<gpiod::line_request>(builder.do_request());
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to request GPIO line for events: {}", e.what());
            throw;
        }
    }

    inline int Line::get_value() const
    {
        try
        {
            if (request_)
            {
                auto value = request_->get_value(offset_);
                return value == gpiod::line::value::ACTIVE ? 1 : 0;
            }
            return 0;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to read GPIO line: {}", e.what());
            return 0;
        }
    }

    inline void Line::set_value(int value)
    {
        try
        {
            if (request_)
            {
                auto val = value ? gpiod::line::value::ACTIVE : gpiod::line::value::INACTIVE;
                request_->set_value(offset_, val);
            }
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to write GPIO line: {}", e.what());
        }
    }

    inline bool Line::event_wait(std::chrono::microseconds timeout)
    {
        try
        {
            if (request_)
            {
                // v2.x: wait_edge_events returns bool, true if event is available
                return request_->wait_edge_events(timeout);
            }
            return false;
        }
        catch (const std::exception& e)
        {
            spdlog::debug("Event wait timeout or error: {}", e.what());
            return false;
        }
    }

    inline std::optional<EdgeEvent> Line::event_read()
    {
        try
        {
            if (request_)
            {
                // v2.x: read_edge_events requires a buffer
                gpiod::edge_event_buffer buffer;
                auto num_events = request_->read_edge_events(buffer, 1);
                
                if (num_events > 0)
                {
                    // Get the event type from the buffer
                    for (const auto& event : buffer)
                    {
                        // event_type is a member variable, not a method
                        auto et = event.type;
                        if (et == gpiod::edge_event::event_type::FALLING_EDGE)
                        {
                            return EdgeEvent::FALLING_EDGE;
                        }
                        else if (et == gpiod::edge_event::event_type::RISING_EDGE)
                        {
                            return EdgeEvent::RISING_EDGE;
                        }
                    }
                }
            }
            return std::nullopt;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to read GPIO event: {}", e.what());
            return std::nullopt;
        }
    }

#else
    // ============ libgpiod v1.x Implementation ============

    inline Line Chip::get_line(unsigned int offset)
    {
        try
        {
            auto line = chip_->get_line(offset);
            Line wrapped;
            wrapped.init_v1(line);
            return wrapped;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to get GPIO line {}: {}", offset, e.what());
            throw;
        }
    }

    inline void Line::request(const std::string& consumer, Direction direction, bool bias_pull_up)
    {
        try
        {
            gpiod::line_request config{};
            config.consumer = consumer.c_str();

            if (direction == Direction::OUTPUT)
            {
                config.request_type = gpiod::line_request::DIRECTION_OUTPUT;
            }
            else
            {
                config.request_type = gpiod::line_request::DIRECTION_INPUT;
                if (bias_pull_up)
                {
                    config.flags = gpiod::line_request::FLAG_BIAS_PULL_UP;
                }
            }

            line_.request(config);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to request GPIO line: {}", e.what());
            throw;
        }
    }

    inline void Line::request_events(const std::string& consumer, bool bias_pull_up)
    {
        try
        {
            gpiod::line_request config{};
            config.consumer = consumer.c_str();
            config.request_type = gpiod::line_request::EVENT_BOTH_EDGES;

            if (bias_pull_up)
            {
                config.flags = gpiod::line_request::FLAG_BIAS_PULL_UP;
            }

            line_.request(config);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to request GPIO line for events: {}", e.what());
            throw;
        }
    }

    inline int Line::get_value() const
    {
        try
        {
            return line_.get_value();
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to read GPIO line: {}", e.what());
            return 0;
        }
    }

    inline void Line::set_value(int value)
    {
        try
        {
            line_.set_value(value);
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to write GPIO line: {}", e.what());
        }
    }

    inline bool Line::event_wait(std::chrono::microseconds timeout)
    {
        try
        {
            return static_cast<bool>(line_.event_wait(timeout));
        }
        catch (const std::exception& e)
        {
            spdlog::debug("Event wait timeout or error: {}", e.what());
            return false;
        }
    }

    inline std::optional<EdgeEvent> Line::event_read()
    {
        try
        {
            auto event = line_.event_read();
            if (event.event_type == gpiod::line_event::FALLING_EDGE)
            {
                return EdgeEvent::FALLING_EDGE;
            }
            else if (event.event_type == gpiod::line_event::RISING_EDGE)
            {
                return EdgeEvent::RISING_EDGE;
            }
            return std::nullopt;
        }
        catch (const std::exception& e)
        {
            spdlog::error("Failed to read GPIO event: {}", e.what());
            return std::nullopt;
        }
    }
        }
    }

#endif

}  // namespace gpio_compat
