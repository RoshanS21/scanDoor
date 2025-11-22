# Pi5 and Pi Zero 2W Compatibility Solution

## Overview
The codebase now supports both Raspberry Pi 5 and Pi Zero 2W by using a compatibility layer that abstracts the differences between libgpiod v1.x (Pi5) and v2.x (Pi Zero 2W).

## Key Changes

### 1. **New Compatibility Layer** (`src/door/gpio_compat.hpp`)
- Provides a unified API wrapper around libgpiod for both versions
- Automatically detects the version at compile time using CMake
- Implements version-specific code paths for:
  - **Line access**: v1.x uses direct `chip_->get_line()`, v2.x uses getter with optional return
  - **Event configuration**: v1.x uses `line_request` structs with flags, v2.x uses `line_request_builder`
  - **Line requests**: v1.x uses direct line requests, v2.x uses request objects
  - **Event handling**: v1.x has `line_event`, v2.x has `line_event` with different accessors

### 2. **Updated GPIO Headers**
All three GPIO-using components now use `gpio_compat.hpp`:
- `wiegand_reader.hpp` - Changed from `gpiod::chip`/`gpiod::line` to `gpio_compat::Chip`/`gpio_compat::Line`
- `gpio_sensor.hpp` - Same conversion
- `door_lock.hpp` - Same conversion

**API Changes:**
- `chip_->get_line(pin)` → `chip_->get_line(pin)` (compatible wrapper)
- `line_.request(config)` → `line_.request_events()` or `line_.request()` with enum-based args
- `event.event_type == gpiod::line_event::FALLING_EDGE` → `event.value() == gpio_compat::EdgeEvent::FALLING_EDGE`

### 3. **CMakeLists.txt Detection**
Added automatic version detection:
```cmake
# Extracts libgpiod major version from pkg-config
# If version >= 2, defines GPIOD_V2_API preprocessor flag
# Version info is printed during build for verification
```

## Benefits of This Approach vs. Separate Branches

✅ **Single codebase** - One source of truth for both platforms
✅ **Easy maintenance** - Bug fixes automatically apply to both versions
✅ **Seamless switching** - Works correctly regardless of platform
✅ **Automatic detection** - No manual configuration needed
✅ **Clear separation** - All version-specific code in one place (gpio_compat.hpp)

## Building

### For Pi5 (libgpiod v1.x)
```bash
cd build
cmake ..
make -j4
```
Will automatically detect v1.x and compile with v1.x code paths.

### For Pi Zero 2W (libgpiod v2.x)
```bash
cd build
cmake ..
make -j4
```
Will automatically detect v2.x and compile with v2.x code paths.

## Testing

During build, CMake will print messages like:
```
libgpiod version: 2.0.0
libgpiod major version: 2
Using libgpiod v2.x API
```

This confirms the correct version was detected.

## Future Maintenance

If libgpiod receives future updates:
1. Add new version-specific code paths to `gpio_compat.hpp`
2. Update the CMakeLists.txt version detection logic
3. No changes needed to the main GPIO headers

---
**Note:** This solution eliminates the need for separate branches, making development and deployment simpler across different Raspberry Pi models.
