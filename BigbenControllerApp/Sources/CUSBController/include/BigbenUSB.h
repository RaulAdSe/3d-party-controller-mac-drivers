// BigbenUSB.h - C interface for Bigben controller USB communication

#ifndef BIGBEN_USB_H
#define BIGBEN_USB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB Constants
#define BIGBEN_VID 0x146b
#define BIGBEN_PID_PC 0x0603
#define BIGBEN_PID_PS4 0x0d05

// XInput report structure (20 bytes)
typedef struct {
    uint8_t report_id;    // 0x00
    uint8_t report_size;  // 0x14 (20)
    uint16_t buttons;     // Button bits
    uint8_t left_trigger;
    uint8_t right_trigger;
    int16_t left_stick_x;
    int16_t left_stick_y;
    int16_t right_stick_x;
    int16_t right_stick_y;
    uint8_t reserved[6];
} __attribute__((packed)) BigbenInputReport;

// XInput button bits
#define BTN_DPAD_UP      0x0001
#define BTN_DPAD_DOWN    0x0002
#define BTN_DPAD_LEFT    0x0004
#define BTN_DPAD_RIGHT   0x0008
#define BTN_START        0x0010
#define BTN_BACK         0x0020
#define BTN_LEFT_THUMB   0x0040  // L3
#define BTN_RIGHT_THUMB  0x0080  // R3
#define BTN_LEFT_BUMPER  0x0100  // LB
#define BTN_RIGHT_BUMPER 0x0200  // RB
#define BTN_GUIDE        0x0400  // Home/Xbox button
#define BTN_A            0x1000
#define BTN_B            0x2000
#define BTN_X            0x4000
#define BTN_Y            0x8000

// Callback function types
typedef void (*BigbenInputCallback)(const BigbenInputReport* report, void* context);
typedef void (*BigbenConnectionCallback)(bool connected, void* context);

// Controller context (opaque)
typedef struct BigbenController BigbenController;

// Initialize the USB library
// Returns 0 on success, negative on error
int bigben_init(void);

// Cleanup the USB library
void bigben_cleanup(void);

// Create a controller instance
// Returns NULL on failure
BigbenController* bigben_create(void);

// Destroy a controller instance
void bigben_destroy(BigbenController* controller);

// Set callbacks
void bigben_set_input_callback(BigbenController* controller, BigbenInputCallback callback, void* context);
void bigben_set_connection_callback(BigbenController* controller, BigbenConnectionCallback callback, void* context);

// Open connection to the controller
// Returns 0 on success, negative on error
int bigben_open(BigbenController* controller);

// Close connection
void bigben_close(BigbenController* controller);

// Check if connected
bool bigben_is_connected(BigbenController* controller);

// Start reading input in a background thread
// Returns 0 on success, negative on error
int bigben_start_reading(BigbenController* controller);

// Stop reading input
void bigben_stop_reading(BigbenController* controller);

// Send rumble command
// weak_motor: 0-255 intensity for weak motor
// strong_motor: 0-255 intensity for strong motor
int bigben_set_rumble(BigbenController* controller, uint8_t weak_motor, uint8_t strong_motor);

// Poll for input once (blocking with timeout)
// Returns 0 on success, negative on error or timeout
int bigben_poll(BigbenController* controller, BigbenInputReport* report, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif // BIGBEN_USB_H
