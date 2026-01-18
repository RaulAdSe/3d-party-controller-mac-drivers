//
//  HIDReportDescriptor.h
//  BigbenController
//
//  Standard HID Gamepad Report Descriptor
//  This is exposed to macOS so applications can understand our virtual device
//

#ifndef HIDReportDescriptor_h
#define HIDReportDescriptor_h

#include <stdint.h>

// =============================================================================
// MARK: - HID Report Descriptor
// =============================================================================

// Standard gamepad HID report descriptor (71 bytes)
// Exposes: 16 buttons, 2 analog sticks (X/Y, Rx/Ry), 2 triggers (Z/Rz), 1 hat switch
static const uint8_t kBigbenHIDReportDescriptor[] = {
    // Usage Page (Generic Desktop)
    0x05, 0x01,

    // Usage (Game Pad)
    0x09, 0x05,

    // Collection (Application)
    0xA1, 0x01,

        // Report ID (1)
        0x85, 0x01,

        // =========================================
        // Buttons (16 buttons)
        // =========================================

        // Usage Page (Button)
        0x05, 0x09,

        // Usage Minimum (Button 1)
        0x19, 0x01,

        // Usage Maximum (Button 16)
        0x29, 0x10,

        // Logical Minimum (0)
        0x15, 0x00,

        // Logical Maximum (1)
        0x25, 0x01,

        // Report Size (1 bit per button)
        0x75, 0x01,

        // Report Count (16 buttons)
        0x95, 0x10,

        // Input (Data, Variable, Absolute)
        0x81, 0x02,

        // =========================================
        // Left Analog Stick (X, Y)
        // =========================================

        // Usage Page (Generic Desktop)
        0x05, 0x01,

        // Usage (X)
        0x09, 0x30,

        // Usage (Y)
        0x09, 0x31,

        // Logical Minimum (0)
        0x15, 0x00,

        // Logical Maximum (255)
        0x26, 0xFF, 0x00,

        // Report Size (8 bits)
        0x75, 0x08,

        // Report Count (2 axes)
        0x95, 0x02,

        // Input (Data, Variable, Absolute)
        0x81, 0x02,

        // =========================================
        // Right Analog Stick (Rx, Ry)
        // =========================================

        // Usage (Rx)
        0x09, 0x33,

        // Usage (Ry)
        0x09, 0x34,

        // Input (Data, Variable, Absolute) - inherits size/count
        0x81, 0x02,

        // =========================================
        // Triggers (Z = Left, Rz = Right)
        // =========================================

        // Usage (Z) - Left Trigger
        0x09, 0x32,

        // Usage (Rz) - Right Trigger
        0x09, 0x35,

        // Input (Data, Variable, Absolute) - inherits size/count
        0x81, 0x02,

        // =========================================
        // D-Pad (Hat Switch)
        // =========================================

        // Usage (Hat switch)
        0x09, 0x39,

        // Logical Minimum (0)
        0x15, 0x00,

        // Logical Maximum (7)
        0x25, 0x07,

        // Physical Minimum (0)
        0x35, 0x00,

        // Physical Maximum (315 degrees)
        0x46, 0x3B, 0x01,

        // Unit (English Rotation: Degrees)
        0x65, 0x14,

        // Report Size (4 bits)
        0x75, 0x04,

        // Report Count (1)
        0x95, 0x01,

        // Input (Data, Variable, Absolute, Null State)
        0x81, 0x42,

        // Padding (4 bits to complete the byte)
        0x75, 0x04,
        0x95, 0x01,
        0x81, 0x03,  // Input (Constant)

    // End Collection
    0xC0
};

static const size_t kBigbenHIDReportDescriptorSize = sizeof(kBigbenHIDReportDescriptor);

// =============================================================================
// MARK: - Virtual HID Report Structure
// =============================================================================

// This is the report format we send to macOS (matches descriptor above)
#pragma pack(push, 1)

typedef struct {
    uint8_t  reportId;          // 0x01
    uint16_t buttons;           // 16 buttons as bitfield
    uint8_t  leftStickX;        // 0-255
    uint8_t  leftStickY;        // 0-255
    uint8_t  rightStickX;       // 0-255
    uint8_t  rightStickY;       // 0-255
    uint8_t  leftTrigger;       // 0-255
    uint8_t  rightTrigger;      // 0-255
    uint8_t  hatSwitch;         // 0-7 (direction), 8 = neutral
} BigbenHIDReport;

#pragma pack(pop)

#define BIGBEN_HID_REPORT_SIZE  sizeof(BigbenHIDReport)

// =============================================================================
// MARK: - HID Usage Constants
// =============================================================================

// Generic Desktop Page (0x01)
#define HID_USAGE_PAGE_GENERIC_DESKTOP  0x01
#define HID_USAGE_GAMEPAD               0x05
#define HID_USAGE_X                     0x30
#define HID_USAGE_Y                     0x31
#define HID_USAGE_Z                     0x32
#define HID_USAGE_RX                    0x33
#define HID_USAGE_RY                    0x34
#define HID_USAGE_RZ                    0x35
#define HID_USAGE_HAT_SWITCH            0x39

// Button Page (0x09)
#define HID_USAGE_PAGE_BUTTON           0x09

#endif /* HIDReportDescriptor_h */
