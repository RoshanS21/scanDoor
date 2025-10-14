# Multi-Door Access Control System

A scalable door access control system for Raspberry Pi 5 supporting multiple doors with RFID readers, sensors, and magnetic locks.

## Dependencies Installation

```bash
# Update package list
sudo apt update

# Install required development packages
sudo apt install -y \
    cmake \
    build-essential \
    git \
    libgpiod-dev \
    libgpiod2 \
    mosquitto \
    mosquitto-dev \
    libmosquitto-dev \
    libspdlog-dev \
    nlohmann-json3-dev

# Start and enable MQTT broker
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

## Building

```bash
# Clean build (if needed)
./clean.sh

# Create build directory and build
mkdir build
cd build
cmake ..
make -j4
```

## Running

```bash
# Start the door controller (needs sudo for GPIO access)
sudo ./door_controller
```

## Directory Structure

```
src/
├── core/           # Core interfaces and types
├── door/           # Door component implementations
├── mqtt/           # MQTT client and message handling
└── utils/          # Logging and utility functions
```

## MQTT Topics

### Publishing Topics
- `door/{doorId}/card_read` - Card read events
- `door/{doorId}/door_sensor` - Door open/close events
- `door/{doorId}/proximity` - Proximity detection events
- `door/{doorId}/exit_button` - Exit button events
- `door/{doorId}/status` - Door status updates

### Subscription Topics
- `door/{doorId}/command` - Control commands

## Cleaning Build

To clean the build directory, you can either:

1. Use the provided script:
```bash
./clean.sh
```

2. Or manually:
```bash
rm -rf build/
```

## Testing MQTT

You can test MQTT functionality using mosquitto command line tools:

```bash
# Subscribe to all door events
mosquitto_sub -t "door/#" -v

# Send a command (e.g., unlock door)
mosquitto_pub -t "door/front/command" -m '{"action": "unlock"}'
```

## GPIO Pin Configuration

Default pin configuration (BCM numbering):
- Wiegand DATA0: GPIO17
- Wiegand DATA1: GPIO27
- Door Sensor: GPIO22
- Proximity Sensor: GPIO23
- Exit Button: GPIO24
- Magnetic Lock: GPIO25

## Log Files

Log files are stored in the `logs` directory:
- Each door has its own log file: `logs/door_{doorId}.log`
- System-wide logs go to console and syslog
