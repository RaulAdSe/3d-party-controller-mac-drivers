//
//  BigbenProtocol.h
//  BigbenController
//
//  USB Protocol definitions for Bigben Interactive controllers
//  Based on Linux hid-bigbenff.c driver
//

#ifndef BigbenProtocol_h
#define BigbenProtocol_h

#include <stdint.h>

// =============================================================================
// MARK: - USB Device Identifiers
// =============================================================================

#define BIGBEN_VENDOR_ID            0x146b

// Product IDs
#define BIGBEN_PRODUCT_PC_COMPACT   0x0603  // PC Compact Controller (XInput mode)
#define BIGBEN_PRODUCT_PS4_COMPACT  0x0d05  // PS4 Compact Controller
#define BIGBEN_PRODUCT_PS3_MINIPAD  0x0902  // PS3 Kid-friendly controller

// =============================================================================
// MARK: - Report IDs
// =============================================================================

#define BIGBEN_REPORT_ID_INPUT      0x01
#define BIGBEN_REPORT_ID_LED        0x01
#define BIGBEN_REPORT_ID_RUMBLE     0x02

// =============================================================================
// MARK: - Report Sizes
// =============================================================================

#define BIGBEN_INPUT_REPORT_SIZE    64
#define BIGBEN_OUTPUT_REPORT_SIZE   8

// =============================================================================
// MARK: - Input Report Structure (64 bytes)
// =============================================================================

#pragma pack(push, 1)

typedef struct {
    uint8_t  reportId;          // 0x01
    uint8_t  leftStickX;        // 0-255, 128 = center
    uint8_t  leftStickY;        // 0-255, 128 = center
    uint8_t  rightStickX;       // 0-255, 128 = center
    uint8_t  rightStickY;       // 0-255, 128 = center
    uint8_t  dpad;              // D-pad hat switch (0-7, 8 = neutral)
    uint16_t buttons;           // Button bitfield
    uint8_t  leftTrigger;       // 0-255
    uint8_t  rightTrigger;      // 0-255
    uint8_t  reserved[54];      // Padding to 64 bytes
} BigbenInputReport;

#pragma pack(pop)

// =============================================================================
// MARK: - Button Bit Definitions
// =============================================================================

typedef enum {
    BIGBEN_BTN_A            = (1 << 0),   // Cross
    BIGBEN_BTN_B            = (1 << 1),   // Circle
    BIGBEN_BTN_X            = (1 << 2),   // Square
    BIGBEN_BTN_Y            = (1 << 3),   // Triangle
    BIGBEN_BTN_LB           = (1 << 4),   // L1
    BIGBEN_BTN_RB           = (1 << 5),   // R1
    BIGBEN_BTN_LT           = (1 << 6),   // L2 (digital)
    BIGBEN_BTN_RT           = (1 << 7),   // R2 (digital)
    BIGBEN_BTN_BACK         = (1 << 8),   // Share/Select
    BIGBEN_BTN_START        = (1 << 9),   // Options/Start
    BIGBEN_BTN_LSTICK       = (1 << 10),  // Left stick click
    BIGBEN_BTN_RSTICK       = (1 << 11),  // Right stick click
    BIGBEN_BTN_HOME         = (1 << 12),  // PS/Guide button
} BigbenButton;

// =============================================================================
// MARK: - D-Pad Values
// =============================================================================

typedef enum {
    BIGBEN_DPAD_UP          = 0,
    BIGBEN_DPAD_UP_RIGHT    = 1,
    BIGBEN_DPAD_RIGHT       = 2,
    BIGBEN_DPAD_DOWN_RIGHT  = 3,
    BIGBEN_DPAD_DOWN        = 4,
    BIGBEN_DPAD_DOWN_LEFT   = 5,
    BIGBEN_DPAD_LEFT        = 6,
    BIGBEN_DPAD_UP_LEFT     = 7,
    BIGBEN_DPAD_NEUTRAL     = 8,
} BigbenDPad;

// =============================================================================
// MARK: - Output Report Structures
// =============================================================================

#pragma pack(push, 1)

// LED Control Report (8 bytes)
typedef struct {
    uint8_t reportId;           // 0x01
    uint8_t reserved1;          // 0x08
    uint8_t ledState;           // Bitmask: LED1=0x01, LED2=0x02, LED3=0x04, LED4=0x08
    uint8_t padding[5];         // 0x00
} BigbenLEDReport;

// Rumble Control Report (8 bytes)
typedef struct {
    uint8_t reportId;           // 0x02
    uint8_t reserved1;          // 0x08
    uint8_t rightMotorOn;       // 0 = off, 1 = on (weak motor)
    uint8_t leftMotorForce;     // 0-255 (strong motor intensity)
    uint8_t duration;           // 0xFF = continuous
    uint8_t padding[3];         // 0x00
} BigbenRumbleReport;

#pragma pack(pop)

// =============================================================================
// MARK: - LED Definitions
// =============================================================================

typedef enum {
    BIGBEN_LED_1            = (1 << 0),
    BIGBEN_LED_2            = (1 << 1),
    BIGBEN_LED_3            = (1 << 2),
    BIGBEN_LED_4            = (1 << 3),
    BIGBEN_LED_ALL          = 0x0F,
} BigbenLED;

// =============================================================================
// MARK: - Helper Macros
// =============================================================================

// Convert analog value (0-255) to signed (-127 to 127)
#define BIGBEN_ANALOG_TO_SIGNED(x)  ((int8_t)((x) - 128))

// Check if a button is pressed
#define BIGBEN_BTN_PRESSED(report, btn)  (((report)->buttons & (btn)) != 0)

// Get D-pad direction
#define BIGBEN_DPAD_VALUE(report)  ((report)->dpad)

#endif /* BigbenProtocol_h */
