# Pi5 and Pi Zero 2W Compatibility - Final Solution

## Problem
The original codebase used libgpiod v1.x API which doesn't work on Pi Zero 2W (libgpiod v2.x). The APIs have significant differences:

- **v1.x**: Direct line objects (`gpiod::line`) with struct-based configuration
- **v2.x**: Line request builder pattern, individual lines managed through requests

## Solution: Unified Compatibility Layer

### Architecture Changes

**Old approach** (would require separate branches):
```
Pi5 branch (libgpiod v1.x) ← separate code
Pi Zero 2W branch (libgpiod v2.x) ← separate code
```

**New approach** (single codebase):
```
All code using gpio_compat API
    ↓
gpio_compat.hpp (detects version)
    ├→ v1.x code path (Pi5)
    └→ v2.x code path (Pi Zero 2W)
```

### Key Implementation Details

**1. Member Variables** - Stored at class level to support both versions:
```cpp
class Line {
private:
    unsigned int offset_;           // For v2.x (need offset with shared chip)
    
#ifdef GPIOD_V2_API
    std::shared_ptr<gpiod::chip> chip_;
    std::shared_ptr<gpiod::line_request> request_;
    std::optional<gpiod::edge_event> last_event_;
#else
    gpiod::line line_;              // Direct line object in v1.x
#endif
};
```

**2. Unified API**:
- `Line::request(consumer, direction, pull_up)` - Configure as input/output
- `Line::request_events(consumer, pull_up)` - Configure for edge detection
- `Line::get_value()` / `set_value()` - Read/write GPIO
- `Line::event_wait(timeout)` / `event_read()` - Handle edge events

**3. Version Detection** (`CMakeLists.txt`):
- Searches for `line-request.hpp` in libgpiod include directories
- Checks if file contains `line_request_builder` (v2.x indicator)
- Sets `GPIOD_V2_API` preprocessor flag automatically
- No manual configuration needed

### libgpiod v2.x API Key Differences

| Feature | v1.x | v2.x |
|---------|------|------|
| Line type | `gpiod::line` (class) | `gpiod::line_request` (class) + `gpiod::line::*` (namespace) |
| Configuration | Struct: `line_request config{...}` | Builder: `line_settings().set_*()` |
| Line access | Direct per line | Through request with offset |
| Event handling | `line_.event_wait()` | `request_->wait_edge_events()` |
| Value access | `line_.get_value()` | `request_->get_value(offset)` |

### Updated Files

1. **`src/door/gpio_compat.hpp`** - New compatibility layer
   - Stores chip as shared_ptr for v2.x (allows multiple lines from same chip)
   - Implements all methods twice: once for v1.x, once for v2.x
   - Normalizes event types and API

2. **`wiegand_reader.hpp`**, **`gpio_sensor.hpp`**, **`door_lock.hpp`**
   - Replaced `gpiod::chip` → `gpio_compat::Chip`
   - Replaced `gpiod::line` → `gpio_compat::Line`
   - Updated API calls to use compatibility layer

3. **`CMakeLists.txt`**
   - Added automatic version detection
   - Sets `GPIOD_V2_API` define when v2.x detected
   - Prints detection result to user

### Build Instructions

Same for both platforms:
```bash
cd build
cmake ..
make -j4
```

CMake will automatically detect libgpiod version and compile appropriately.

### Build Output

**Pi Zero 2W (v2.x):**
```
-- Detected libgpiod v2.x API (found line_request_builder)
-- Configuring done
```

**Pi5 (v1.x):**
```
-- Using libgpiod v1.x API (line_request_builder not found)
-- Configuring done
```

## Benefits

✅ **Single codebase** works on both Pi5 and Pi Zero 2W  
✅ **Automatic detection** - no manual switching  
✅ **Easy maintenance** - version-specific code isolated in one file  
✅ **Future-proof** - can add v3.x support without forking  
✅ **No separate branches needed**  

## Technical Details

### Why not use `gpiod::line` directly in v2.x?

In libgpiod v2.x, `gpiod::line` is a **namespace**, not a class. The actual line request object is `gpiod::line_request`, which manages one or more GPIO lines together. This is a fundamental design difference - v2.x uses request batching for efficiency.

Our solution stores:
- The chip reference (shared among multiple lines)
- The offset (which line within the chip)
- The line_request object (manages the requested lines)

This allows the compatibility layer to provide per-line semantics matching v1.x's API while working with v2.x's request-based architecture.

---
**Status**: Ready for testing on both platforms. Version detection is automatic via CMake header inspection.
