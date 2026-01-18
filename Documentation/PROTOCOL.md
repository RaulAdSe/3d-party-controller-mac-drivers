# Bigben Controller USB Protocol

This document describes the USB protocol used by Bigben Interactive / NACON controllers.

## Device Information

| Property | Value |
|----------|-------|
| Vendor ID | `0x146b` |
| Product ID (PC Mode) | `0x0603` |
| Product ID (PS4 Mode) | `0x0d05` |
| USB Class | `0xFF` (Vendor Specific) |
| USB Subclass | `0x5D` (93) |
| USB Protocol | `0x01` |

## Endpoints

| Endpoint | Direction | Type | Size | Interval |
|----------|-----------|------|------|----------|
| EP1 | IN | Interrupt | 64 bytes | 4ms |
| EP2 | OUT | Interrupt | 8 bytes | 4ms |

## Input Report (64 bytes)

Sent from controller to host via EP1 IN.

```
Offset  Size  Description
------  ----  -----------
0       1     Report ID (0x01)
1       1     Left Stick X (0-255, 128=center)
2       1     Left Stick Y (0-255, 128=center)
3       1     Right Stick X (0-255, 128=center)
4       1     Right Stick Y (0-255, 128=center)
5       1     D-Pad / Hat Switch (see below)
6       2     Button Bitfield (little-endian)
8       1     Left Trigger Analog (0-255)
9       1     Right Trigger Analog (0-255)
10-63   54    Reserved / Padding
```

### D-Pad Values

| Value | Direction |
|-------|-----------|
| 0 | Up |
| 1 | Up-Right |
| 2 | Right |
| 3 | Down-Right |
| 4 | Down |
| 5 | Down-Left |
| 6 | Left |
| 7 | Up-Left |
| 8 | Neutral (no direction) |

### Button Bitfield

| Bit | Button | PlayStation | Xbox |
|-----|--------|-------------|------|
| 0 | A | Cross | A |
| 1 | B | Circle | B |
| 2 | X | Square | X |
| 3 | Y | Triangle | Y |
| 4 | LB | L1 | LB |
| 5 | RB | R1 | RB |
| 6 | LT | L2 (digital) | LT |
| 7 | RT | R2 (digital) | RT |
| 8 | Back | Share | Back |
| 9 | Start | Options | Start |
| 10 | L3 | L3 | LS |
| 11 | R3 | R3 | RS |
| 12 | Home | PS | Guide |
| 13-15 | Reserved | - | - |

## Output Reports

Sent from host to controller via EP2 OUT. All output reports are 8 bytes.

### LED Control Report

```
Byte  Value     Description
----  -----     -----------
0     0x01      Report ID (LED)
1     0x08      Reserved (always 0x08)
2     0x00-0x0F LED State Bitmask
3-7   0x00      Padding
```

**LED Bitmask:**
- Bit 0: LED 1
- Bit 1: LED 2
- Bit 2: LED 3
- Bit 3: LED 4

### Rumble Control Report

```
Byte  Value     Description
----  -----     -----------
0     0x02      Report ID (Rumble)
1     0x08      Reserved (always 0x08)
2     0x00-0x01 Right Motor (weak) - on/off
3     0x00-0xFF Left Motor (strong) - intensity
4     0xFF      Duration (0xFF = continuous)
5-7   0x00      Padding
```

## Example Code

### Parsing Input Report (C)

```c
void parseInputReport(const uint8_t *data, size_t length) {
    if (length < 10 || data[0] != 0x01) return;

    uint8_t lx = data[1];
    uint8_t ly = data[2];
    uint8_t rx = data[3];
    uint8_t ry = data[4];
    uint8_t dpad = data[5];
    uint16_t buttons = data[6] | (data[7] << 8);
    uint8_t lt = data[8];
    uint8_t rt = data[9];

    // Convert to signed for easier handling
    int8_t leftX = (int8_t)(lx - 128);
    int8_t leftY = (int8_t)(ly - 128);

    // Check button presses
    bool aPressed = (buttons & 0x0001) != 0;
    bool bPressed = (buttons & 0x0002) != 0;
    // ... etc
}
```

### Sending Rumble Command (C)

```c
void sendRumble(IOUSBHostPipe *pipe, uint8_t weak, uint8_t strong) {
    uint8_t report[8] = {
        0x02,       // Report ID
        0x08,       // Reserved
        weak ? 1 : 0,   // Right motor on/off
        strong,     // Left motor intensity
        0xFF,       // Duration (continuous)
        0x00, 0x00, 0x00  // Padding
    };

    pipe->IO(report, sizeof(report), ...);
}
```

## References

- [Linux hid-bigbenff.c](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-bigbenff.c)
- [Linux xpad.c](https://github.com/torvalds/linux/blob/master/drivers/input/joystick/xpad.c)
- [USB HID Usage Tables](https://usb.org/document-library/hid-usage-tables-14)
