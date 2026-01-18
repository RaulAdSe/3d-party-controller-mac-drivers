# Bigben/NACON macOS Controller Driver

A macOS driver/mapper for the Bigben Interactive / NACON controllers that converts controller input to keyboard and mouse events, allowing you to play any game with your controller!

## Why This Exists

The Bigben/NACON controllers use a proprietary USB protocol (vendor-specific class 0xFF) instead of standard HID, which means macOS doesn't recognize them as game controllers. This tool:

1. Communicates directly with the controller via USB (using libusb)
2. Reads the XInput-format data from the controller
3. Converts controller inputs to keyboard/mouse events
4. Works with **any game** that supports keyboard/mouse

## Supported Controllers

| Controller | Vendor ID | Product ID | Status |
|------------|-----------|------------|--------|
| Bigben PC Compact Controller | 0x146b | 0x0603 | **Working** |
| Bigben PS4 Compact Controller | 0x146b | 0x0d05 | Planned |

## Quick Start

### Requirements

- macOS 12.0 (Monterey) or later
- [Homebrew](https://brew.sh) package manager
- Xcode Command Line Tools (`xcode-select --install`)

### Installation

```bash
# Clone the repository
git clone https://github.com/RaulAdSe/3d-party-controller-mac-drivers.git
cd 3d-party-controller-mac-drivers

# Run the installer
chmod +x install.sh
./install.sh
```

### Usage

```bash
# Start the mapper
bigben-mapper

# Start with debug output (shows controller data)
bigben-mapper --debug
```

When first run, macOS will ask for **Accessibility permission**. Grant it in:
- System Settings → Privacy & Security → Accessibility

## Default Controls (FPS Preset)

| Controller | Keyboard/Mouse |
|------------|----------------|
| Left Stick | W/A/S/D (Movement) |
| Right Stick | Mouse (Look around) |
| A (Cross) | Space (Jump) |
| B (Circle) | C (Crouch) |
| X (Square) | R (Reload) |
| Y (Triangle) | F (Interact) |
| LB (L1) | Q |
| RB (R1) | E |
| LT (L2) | Right Click (ADS) |
| RT (R2) | Left Click (Fire) |
| L3 | Shift (Sprint) |
| R3 | V (Melee) |
| Back/Share | Tab (Scoreboard) |
| Start | Escape (Menu) |
| D-Pad Up | 1 |
| D-Pad Down | 2 |
| D-Pad Left | 3 |
| D-Pad Right | 4 |

## Building from Source

```bash
# Install dependencies
brew install libusb

# Build
swift build -c release

# The binary is at .build/release/bigben-mapper
```

## Project Structure

```
3d-party-controller-mac-drivers/
├── BigbenControllerApp/
│   └── Sources/
│       ├── main.swift                    # Entry point
│       ├── CUSBController/               # C library for USB
│       │   ├── BigbenUSB.c               # libusb implementation
│       │   └── include/BigbenUSB.h       # C API header
│       └── Services/
│           ├── USBController.swift       # Swift USB wrapper
│           └── KeyboardEmulator.swift    # Keyboard/mouse emulation
├── Package.swift                         # Swift Package Manager
├── install.sh                            # Installation script
└── README.md
```

## How It Works

1. **USB Communication**: Uses libusb to directly communicate with the controller over USB
2. **Protocol**: The controller uses XInput protocol (SubClass 0x5d, Protocol 0x01)
3. **Data Format**: 20-byte reports with buttons, triggers, and stick positions
4. **Emulation**: CGEvent API to inject keyboard and mouse events

## Troubleshooting

### Controller not detected

1. Make sure the controller is plugged in
2. Check USB connection: `system_profiler SPUSBDataType | grep -A5 Bigben`
3. Try disconnecting and reconnecting

### Accessibility permission denied

1. Open System Settings → Privacy & Security → Accessibility
2. Find and enable `bigben-mapper` (or Terminal if running from there)
3. Restart the mapper

### Keys not working in game

1. Make sure the game window is focused
2. Try running the mapper with `--debug` to see if inputs are detected
3. Some games may need additional configuration

## Contributing

Pull requests welcome! Areas that could use improvement:

- [ ] Racing preset mapping
- [ ] Custom key remapping
- [ ] GUI configuration app
- [ ] PS4 mode support (PID 0x0d05)
- [ ] Force feedback/rumble support

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

- [Linux hid-bigbenff.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-bigbenff.c) - Protocol reference
- [libusb](https://libusb.info/) - USB library
