# Bigben/NACON macOS Controller Driver

A DriverKit-based macOS driver for the Bigben Interactive / NACON PS4 Compact Controller.

## Supported Controllers

| Controller | Vendor ID | Product ID | Status |
|------------|-----------|------------|--------|
| Bigben PC Compact Controller | 0x146b | 0x0603 | In Development |
| Bigben PS4 Compact Controller | 0x146b | 0x0d05 | Planned |

## Project Structure

```
BigbenController/
├── BigbenControllerApp/           # Host macOS application
│   ├── Sources/
│   │   ├── App/                   # SwiftUI app entry point
│   │   ├── Views/                 # UI components
│   │   └── Services/              # Driver communication
│   └── Resources/
│
├── BigbenControllerDriver/        # DriverKit extension (.dext)
│   ├── Sources/
│   │   ├── BigbenUSBDriver.iig    # USB device driver interface
│   │   ├── BigbenUSBDriver.cpp    # USB device implementation
│   │   ├── BigbenHIDDevice.iig    # Virtual HID device interface
│   │   ├── BigbenHIDDevice.cpp    # Virtual HID implementation
│   │   └── Protocol/              # USB protocol definitions
│   └── Resources/
│       └── Info.plist             # Device matching
│
├── Shared/                        # Shared code between app and driver
│   ├── BigbenProtocol.h           # Protocol constants
│   ├── ButtonMapping.h            # Button/axis mappings
│   └── Constants.h                # Shared constants
│
├── Tests/                         # Unit and integration tests
│   ├── DriverTests/
│   └── AppTests/
│
└── Documentation/
    ├── PROTOCOL.md                # USB protocol documentation
    ├── DEVELOPMENT.md             # Development setup guide
    └── ARCHITECTURE.md            # Architecture overview
```

## Requirements

- macOS 12.0 (Monterey) or later
- Xcode 14.0 or later
- Apple Developer Program membership (for DriverKit entitlements)

## Building

```bash
# Clone the repository
git clone https://github.com/RaulAdSe/3d-party-controller-mac-drivers.git
cd 3d-party-controller-mac-drivers

# Open in Xcode
open BigbenController.xcodeproj
```

## Development Setup

1. Enable Developer Mode:
   ```bash
   systemextensionsctl developer on
   ```

2. Disable SIP (for local testing only):
   - Boot into Recovery Mode (hold Power on Apple Silicon)
   - Open Terminal and run: `csrutil disable`

3. Build and run the app from Xcode

## Sprint Progress

### Sprint 1: Foundation (Current)
- [x] Project structure
- [x] USB protocol documentation
- [ ] Basic USB driver skeleton
- [ ] HID report descriptor
- [ ] Device matching

### Sprint 2: Core Functionality
- [ ] USB communication
- [ ] Input report parsing
- [ ] Virtual HID device
- [ ] Basic input forwarding

### Sprint 3: Features
- [ ] Force feedback/rumble
- [ ] LED control
- [ ] Button remapping
- [ ] Preferences UI

### Sprint 4: Polish
- [ ] Error handling
- [ ] Reconnection handling
- [ ] Performance optimization
- [ ] Documentation

## License

MIT License - See [LICENSE](LICENSE) for details.

## Acknowledgments

- [Linux hid-bigbenff.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-bigbenff.c)
- [360Controller](https://github.com/360Controller/360Controller)
- [Karabiner-DriverKit-VirtualHIDDevice](https://github.com/pqrs-org/Karabiner-DriverKit-VirtualHIDDevice)
