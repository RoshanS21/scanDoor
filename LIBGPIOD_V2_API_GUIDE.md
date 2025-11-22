# libgpiod v2.x C++ API - Correct Types and Usage

## Key Finding: `gpiod::line` is a Namespace, NOT a Type

In libgpiod v2.x C++ bindings, `gpiod::line` is indeed a **namespace** containing enums and types, not the line object class itself.

## Correct Type for Storing GPIO Lines

**The type you need to store GPIO line objects is: `gpiod::line_request`**

### What is `gpiod::line_request`?

`gpiod::line_request` is a class that:
- Stores the context of a set of requested GPIO lines
- Cannot be copied (deleted copy constructor)
- Can be moved (has move constructor)
- Manages the file descriptor and lifecycle of GPIO requests
- Provides methods to get/set line values

### How to Get a Line Request in v2.x

The pattern in libgpiod v2.x uses a **builder pattern**:

```cpp
// Get a chip and prepare a request
auto request = ::gpiod::chip(chip_path)
    .prepare_request()                           // Start builder
    .set_consumer("my-app")                      // Set consumer name
    .add_line_settings(
        line_offset,
        ::gpiod::line_settings()
            .set_direction(::gpiod::line::direction::INPUT)
    )
    .do_request();                               // Execute request
```

### Key Methods of `gpiod::line_request`

**Reading values:**
- `get_value(line::offset offset)` - Get value of single line
- `get_values()` - Get values of all requested lines
- `get_values(const line::offsets& offsets)` - Get specific lines

**Writing values:**
- `set_value(line::offset offset, line::value value)` - Set single line
- `set_values(const line::value_mappings& values)` - Set multiple lines
- `set_values(const line::offsets& offsets, const line::values& values)` - Set subset

**Management:**
- `release()` - Release the requested lines and free resources
- `fd()` - Get file descriptor
- `chip_name()` - Get chip name
- `num_lines()` - Number of requested lines
- `offsets()` - Get list of offsets

**Edge detection:**
- `wait_edge_events(timeout)` - Wait for edge events
- `read_edge_events(buffer)` - Read edge events

## Important Types in `gpiod::line` Namespace

```cpp
// Enums (in gpiod::line namespace)
enum class direction { AS_IS, INPUT, OUTPUT };
enum class value { INACTIVE = 0, ACTIVE = 1 };
enum class edge { NONE, RISING, FALLING, BOTH };
enum class bias { AS_IS, UNKNOWN, DISABLED, PULL_UP, PULL_DOWN };
enum class drive { PUSH_PULL, OPEN_DRAIN, OPEN_SOURCE };
enum class clock { MONOTONIC, REALTIME, HTE };

// Wrapper types
class offset;                                    // Wraps unsigned int

// Vector types
using offsets = ::std::vector<offset>;
using values = ::std::vector<value>;
using value_mapping = ::std::pair<offset, value>;
using value_mappings = ::std::vector<value_mapping>;
```

## Complete Example - Reading a Line

```cpp
#include <gpiod.hpp>
#include <iostream>
#include <filesystem>

int main() {
    const std::filesystem::path chip_path("/dev/gpiochip0");
    const gpiod::line::offset line_offset = 5;
    
    // Create request using builder pattern
    auto request = gpiod::chip(chip_path)
        .prepare_request()
        .set_consumer("read-example")
        .add_line_settings(
            line_offset,
            gpiod::line_settings().set_direction(
                gpiod::line::direction::INPUT
            )
        )
        .do_request();
    
    // Read the value
    auto value = request.get_value(line_offset);
    
    std::cout << "Line " << line_offset << " = "
              << (value == gpiod::line::value::ACTIVE ? "Active" : "Inactive")
              << std::endl;
    
    return 0;
}
```

## Complete Example - Writing a Line

```cpp
#include <gpiod.hpp>
#include <iostream>
#include <filesystem>

int main() {
    const std::filesystem::path chip_path("/dev/gpiochip0");
    const gpiod::line::offset line_offset = 5;
    
    // Create request for OUTPUT
    auto request = gpiod::chip(chip_path)
        .prepare_request()
        .set_consumer("write-example")
        .add_line_settings(
            line_offset,
            gpiod::line_settings().set_direction(
                gpiod::line::direction::OUTPUT
            )
        )
        .do_request();
    
    // Set the line to active
    request.set_value(line_offset, gpiod::line::value::ACTIVE);
    
    return 0;
}
```

## Summary of the Correct Architecture

| Concept | v1.x | v2.x |
|---------|------|------|
| **Type for GPIO line** | `gpiod::line` class | `gpiod::line_request` class |
| **Get chip** | `gpiod::chip` | `gpiod::chip` |
| **Get line from chip** | `chip.get_line(offset)` | Use builder: `chip.prepare_request()...do_request()` |
| **Request pattern** | Direct line retrieval | Builder pattern with `prepare_request()` |
| **Read value** | `line.get_value()` | `request.get_value(offset)` |
| **Write value** | `line.set_value()` | `request.set_value(offset, value)` |
| **Namespace** | `gpiod::line` is a type | `gpiod::line` is a namespace with enums/types |

## Key Differences from v1.x

1. **No individual line objects**: You don't store individual `gpiod::line` objects anymore
2. **Bulk request model**: Everything uses `line_request` which can manage multiple lines
3. **Builder pattern**: Configuration happens through a builder rather than direct function calls
4. **No copy semantics**: `line_request` cannot be copied (move-only type)
5. **Consumer string**: You must set a consumer name when creating a request
6. **Line settings**: Configuration uses `line_settings` class passed to the builder

## Migration Notes

If migrating from v1.x:
- Replace individual `gpiod::line` storage with `gpiod::line_request`
- Use builder pattern instead of direct method calls
- Handle the move-only semantics (use `std::move` or pass by reference)
- Set a consumer name for each request
- Pass line offsets to request methods instead of having individual line objects
