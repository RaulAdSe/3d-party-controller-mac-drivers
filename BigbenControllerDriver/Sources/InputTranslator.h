//
//  InputTranslator.h
//  BigbenControllerDriver
//
//  Utility class for translating proprietary Bigben controller input reports
//  to standard HID gamepad format. Handles button mapping, axis conversion,
//  and D-pad to hat switch translation.
//
//  Copyright (c) 2024. Licensed under MIT License.
//

#ifndef InputTranslator_h
#define InputTranslator_h

#include <stdint.h>
#include <stdbool.h>

#include "../../Shared/BigbenProtocol.h"
#include "../../Shared/HIDReportDescriptor.h"

// =============================================================================
// MARK: - Constants
// =============================================================================

/// Default deadzone for analog sticks (0-255 scale, centered at 128)
/// Values within 128 +/- DEADZONE are treated as neutral
#define INPUT_TRANSLATOR_DEFAULT_DEADZONE   12

/// Center value for analog axes
#define INPUT_TRANSLATOR_AXIS_CENTER        128

/// Minimum axis value
#define INPUT_TRANSLATOR_AXIS_MIN           0

/// Maximum axis value
#define INPUT_TRANSLATOR_AXIS_MAX           255

/// Hat switch neutral value (released state)
#define INPUT_TRANSLATOR_HAT_NEUTRAL        8

// =============================================================================
// MARK: - Button Mapping
// =============================================================================

/*!
 * @enum HIDButton
 * @abstract Standard HID button indices (1-based as per HID spec)
 * @discussion Maps to the 16-button bitfield in BigbenHIDReport
 */
typedef enum {
    HID_BTN_A           = 0,    // Button 1: A/Cross
    HID_BTN_B           = 1,    // Button 2: B/Circle
    HID_BTN_X           = 2,    // Button 3: X/Square
    HID_BTN_Y           = 3,    // Button 4: Y/Triangle
    HID_BTN_LB          = 4,    // Button 5: Left Bumper
    HID_BTN_RB          = 5,    // Button 6: Right Bumper
    HID_BTN_LT          = 6,    // Button 7: Left Trigger (digital)
    HID_BTN_RT          = 7,    // Button 8: Right Trigger (digital)
    HID_BTN_BACK        = 8,    // Button 9: Back/Select/Share
    HID_BTN_START       = 9,    // Button 10: Start/Options
    HID_BTN_LSTICK      = 10,   // Button 11: Left Stick Click
    HID_BTN_RSTICK      = 11,   // Button 12: Right Stick Click
    HID_BTN_HOME        = 12,   // Button 13: Home/Guide/PS
    HID_BTN_RESERVED_14 = 13,   // Button 14: Reserved
    HID_BTN_RESERVED_15 = 14,   // Button 15: Reserved
    HID_BTN_RESERVED_16 = 15,   // Button 16: Reserved
} HIDButton;

// =============================================================================
// MARK: - InputTranslator Class
// =============================================================================

/*!
 * @class InputTranslator
 * @abstract Converts proprietary Bigben input reports to standard HID format
 * @discussion This class handles all aspects of input translation:
 *
 *             - Button mapping: Converts Bigben button bitfield to HID buttons
 *             - Axis translation: Applies deadzone and centers analog sticks
 *             - D-pad conversion: Maps D-pad states to hat switch values
 *             - Trigger handling: Passes through trigger values with optional deadzone
 *
 *             The translator is designed to be stateless for the main translation
 *             path, with optional configuration for deadzone values.
 */
class InputTranslator {
public:
    // =========================================================================
    // MARK: - Construction
    // =========================================================================

    /*!
     * @function InputTranslator
     * @abstract Default constructor with default deadzone
     */
    InputTranslator();

    /*!
     * @function InputTranslator
     * @abstract Constructor with custom deadzone
     * @param deadzone Analog stick deadzone value (0-127)
     */
    explicit InputTranslator(uint8_t deadzone);

    // =========================================================================
    // MARK: - Configuration
    // =========================================================================

    /*!
     * @function setDeadzone
     * @abstract Set the analog stick deadzone
     * @param deadzone Deadzone value (0-127)
     */
    void setDeadzone(uint8_t deadzone);

    /*!
     * @function getDeadzone
     * @abstract Get the current deadzone value
     * @return Current deadzone (0-127)
     */
    uint8_t getDeadzone() const;

    /*!
     * @function setTriggerDeadzone
     * @abstract Set the trigger deadzone
     * @param deadzone Trigger deadzone value (0-255)
     */
    void setTriggerDeadzone(uint8_t deadzone);

    /*!
     * @function getTriggerDeadzone
     * @abstract Get the current trigger deadzone
     * @return Current trigger deadzone (0-255)
     */
    uint8_t getTriggerDeadzone() const;

    // =========================================================================
    // MARK: - Translation
    // =========================================================================

    /*!
     * @function translate
     * @abstract Translate a Bigben input report to standard HID format
     * @param input Pointer to proprietary BigbenInputReport
     * @param output Pointer to BigbenHIDReport to fill
     * @return true if translation succeeded, false on error
     */
    bool translate(const BigbenInputReport* input, BigbenHIDReport* output) const;

    // =========================================================================
    // MARK: - Static Utilities
    // =========================================================================

    /*!
     * @function translateButtons
     * @abstract Convert Bigben button bitfield to HID button bitfield
     * @param bigbenButtons Bigben button state (16-bit)
     * @return HID button bitfield (16-bit)
     */
    static uint16_t translateButtons(uint16_t bigbenButtons);

    /*!
     * @function translateDPadToHat
     * @abstract Convert Bigben D-pad value to HID hat switch
     * @param dpad Bigben D-pad value (0-8)
     * @return HID hat switch value (0-7, 8=neutral)
     */
    static uint8_t translateDPadToHat(uint8_t dpad);

    /*!
     * @function applyDeadzone
     * @abstract Apply deadzone to an axis value
     * @param value Raw axis value (0-255)
     * @param deadzone Deadzone amount (0-127)
     * @return Axis value with deadzone applied (0-255)
     */
    static uint8_t applyDeadzone(uint8_t value, uint8_t deadzone);

    /*!
     * @function applyTriggerDeadzone
     * @abstract Apply deadzone to a trigger value
     * @param value Raw trigger value (0-255)
     * @param deadzone Deadzone threshold (0-255)
     * @return Trigger value with deadzone applied (0-255)
     */
    static uint8_t applyTriggerDeadzone(uint8_t value, uint8_t deadzone);

    /*!
     * @function initializeNeutralReport
     * @abstract Initialize a HID report to neutral/centered state
     * @param report Pointer to report to initialize
     */
    static void initializeNeutralReport(BigbenHIDReport* report);

    /*!
     * @function isButtonPressed
     * @abstract Check if a specific button is pressed in a report
     * @param report The HID report to check
     * @param button The button index (HIDButton enum)
     * @return true if pressed
     */
    static bool isButtonPressed(const BigbenHIDReport* report, HIDButton button);

private:
    // =========================================================================
    // MARK: - Private Members
    // =========================================================================

    uint8_t m_deadzone;         ///< Analog stick deadzone (0-127)
    uint8_t m_triggerDeadzone;  ///< Trigger deadzone (0-255)
};

// =============================================================================
// MARK: - Inline Implementations
// =============================================================================

inline uint8_t InputTranslator::getDeadzone() const
{
    return m_deadzone;
}

inline uint8_t InputTranslator::getTriggerDeadzone() const
{
    return m_triggerDeadzone;
}

inline bool InputTranslator::isButtonPressed(const BigbenHIDReport* report, HIDButton button)
{
    if (report == nullptr || button > HID_BTN_RESERVED_16) {
        return false;
    }
    return (report->buttons & (1 << button)) != 0;
}

#endif /* InputTranslator_h */
