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
        explicit Chip(const std::string& device_path)
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

        Line get_line(unsigned int offset);

        gpiod::chip* get_raw() { return chip_.get(); }

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

#ifdef GPIOD_V2_API
        // libgpiod v2.x - using request objects
        gpiod::line line_;
        std::unique_ptr<gpiod::line_request> request_;
        std::optional<gpiod::line_event> last_event_;

        void init_v2(gpiod::line line) { line_ = line; }

#else
        // libgpiod v1.x - direct line objects
        gpiod::line line_;

        void init_v1(gpiod::line line) { line_ = line; }
#endif
    };

    // Implementation

#ifdef GPIOD_V2_API
    // ============ libgpiod v2.x Implementation ============

    inline Line Chip::get_line(unsigned int offset)
    {
        try
        {
            auto line = chip_->get_line(offset);
            Line wrapped;
            wrapped.init_v2(line);
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
            gpiod::line_request_builder builder(line_);
            builder.set_consumer(consumer);

            if (direction == Direction::OUTPUT)
            {
                builder.set_direction_output();
            }
            else
            {
                builder.set_direction_input();
                if (bias_pull_up)
                {
                    builder.set_bias_pull_up();
                }
            }

            request_ = std::make_unique<gpiod::line_request>(builder.request());
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
            gpiod::line_request_builder builder(line_);
            builder.set_consumer(consumer);
            builder.set_direction_input();
            builder.set_edge_detection(gpiod::edge::BOTH);

            if (bias_pull_up)
            {
                builder.set_bias_pull_up();
            }

            request_ = std::make_unique<gpiod::line_request>(builder.request());
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
                return request_->get_value() ? 1 : 0;
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
                request_->set_value(value ? 1 : 0);
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
                auto event = request_->wait_edge_event(timeout);
                if (event)
                {
                    last_event_ = event.value();
                    return true;
                }
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
            if (last_event_)
            {
                auto event_type = last_event_->get_event_type();
                last_event_.reset();

                if (event_type == gpiod::edge_event::FALLING_EDGE)
                {
                    return EdgeEvent::FALLING_EDGE;
                }
                else if (event_type == gpiod::edge_event::RISING_EDGE)
                {
                    return EdgeEvent::RISING_EDGE;
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

#endif

}  // namespace gpio_compat
