//
//  InputTranslator.cpp
//  BigbenControllerDriver
//
//  Implementation of the input translation utility class.
//  Converts proprietary Bigben controller reports to standard HID format.
//
//  Copyright (c) 2024. Licensed under MIT License.
//

#include "InputTranslator.h"
#include <string.h>  // for memset

// =============================================================================
// MARK: - Construction
// =============================================================================

InputTranslator::InputTranslator()
    : m_deadzone(INPUT_TRANSLATOR_DEFAULT_DEADZONE)
    , m_triggerDeadzone(0)
{
}

InputTranslator::InputTranslator(uint8_t deadzone)
    : m_deadzone(deadzone)
    , m_triggerDeadzone(0)
{
    // Clamp deadzone to valid range
    if (m_deadzone > 127) {
        m_deadzone = 127;
    }
}

// =============================================================================
// MARK: - Configuration
// =============================================================================

void InputTranslator::setDeadzone(uint8_t deadzone)
{
    m_deadzone = (deadzone > 127) ? 127 : deadzone;
}

void InputTranslator::setTriggerDeadzone(uint8_t deadzone)
{
    m_triggerDeadzone = deadzone;
}

// =============================================================================
// MARK: - Main Translation
// =============================================================================

bool InputTranslator::translate(const BigbenInputReport* input, BigbenHIDReport* output) const
{
    if (input == nullptr || output == nullptr) {
        return false;
    }

    // Initialize output to known state
    memset(output, 0, sizeof(BigbenHIDReport));

    // Set report ID
    output->reportId = BIGBEN_REPORT_ID_INPUT;

    // Translate buttons (direct mapping for Bigben controllers)
    output->buttons = translateButtons(input->buttons);

    // Translate analog sticks with deadzone
    output->leftStickX = applyDeadzone(input->leftStickX, m_deadzone);
    output->leftStickY = applyDeadzone(input->leftStickY, m_deadzone);
    output->rightStickX = applyDeadzone(input->rightStickX, m_deadzone);
    output->rightStickY = applyDeadzone(input->rightStickY, m_deadzone);

    // Translate triggers with optional deadzone
    output->leftTrigger = applyTriggerDeadzone(input->leftTrigger, m_triggerDeadzone);
    output->rightTrigger = applyTriggerDeadzone(input->rightTrigger, m_triggerDeadzone);

    // Translate D-pad to hat switch
    output->hatSwitch = translateDPadToHat(input->dpad);

    return true;
}

// =============================================================================
// MARK: - Button Translation
// =============================================================================

uint16_t InputTranslator::translateButtons(uint16_t bigbenButtons)
{
    // The Bigben button layout maps directly to our HID button layout.
    // Both use the same bit positions for the main buttons.
    //
    // Bigben bits:          HID bits (target):
    // 0: A (Cross)          0: Button 1 (A)
    // 1: B (Circle)         1: Button 2 (B)
    // 2: X (Square)         2: Button 3 (X)
    // 3: Y (Triangle)       3: Button 4 (Y)
    // 4: LB (L1)            4: Button 5 (LB)
    // 5: RB (R1)            5: Button 6 (RB)
    // 6: LT digital (L2)    6: Button 7 (LT)
    // 7: RT digital (R2)    7: Button 8 (RT)
    // 8: Back (Select)      8: Button 9 (Back)
    // 9: Start              9: Button 10 (Start)
    // 10: L Stick Click     10: Button 11 (L3)
    // 11: R Stick Click     11: Button 12 (R3)
    // 12: Home (Guide)      12: Button 13 (Home)
    //
    // The mapping is 1:1, so we can return the value directly.
    // We mask to 13 bits since that's all Bigben uses.

    return bigbenButtons & 0x1FFF;  // Mask to 13 active buttons
}

// =============================================================================
// MARK: - D-Pad Translation
// =============================================================================

uint8_t InputTranslator::translateDPadToHat(uint8_t dpad)
{
    // The Bigben D-pad values map directly to HID hat switch values.
    // Both use the same encoding:
    //
    //   0: Up          (0 degrees)
    //   1: Up-Right    (45 degrees)
    //   2: Right       (90 degrees)
    //   3: Down-Right  (135 degrees)
    //   4: Down        (180 degrees)
    //   5: Down-Left   (225 degrees)
    //   6: Left        (270 degrees)
    //   7: Up-Left     (315 degrees)
    //   8: Neutral     (null state)
    //
    // Validate and pass through

    if (dpad > BIGBEN_DPAD_NEUTRAL) {
        // Invalid value - treat as neutral
        return INPUT_TRANSLATOR_HAT_NEUTRAL;
    }

    return dpad;
}

// =============================================================================
// MARK: - Deadzone Application
// =============================================================================

uint8_t InputTranslator::applyDeadzone(uint8_t value, uint8_t deadzone)
{
    // Convert to signed offset from center (128)
    int16_t offset = (int16_t)value - INPUT_TRANSLATOR_AXIS_CENTER;

    // Check if within deadzone
    if (offset > -deadzone && offset < deadzone) {
        return INPUT_TRANSLATOR_AXIS_CENTER;
    }

    // Outside deadzone - scale the remaining range
    // This ensures smooth transition from deadzone edge to full deflection
    if (deadzone > 0 && deadzone < 128) {
        // Calculate the active range (from deadzone to edge)
        int16_t maxRange = 127;  // Maximum offset from center
        int16_t activeRange = maxRange - deadzone;

        if (activeRange > 0) {
            if (offset > 0) {
                // Positive direction
                int16_t adjusted = offset - deadzone;
                offset = (adjusted * maxRange) / activeRange;
            } else {
                // Negative direction
                int16_t adjusted = offset + deadzone;
                offset = (adjusted * maxRange) / activeRange;
            }
        }
    }

    // Convert back to unsigned (0-255) and clamp
    int16_t result = offset + INPUT_TRANSLATOR_AXIS_CENTER;

    if (result < INPUT_TRANSLATOR_AXIS_MIN) {
        return INPUT_TRANSLATOR_AXIS_MIN;
    }
    if (result > INPUT_TRANSLATOR_AXIS_MAX) {
        return INPUT_TRANSLATOR_AXIS_MAX;
    }

    return (uint8_t)result;
}

uint8_t InputTranslator::applyTriggerDeadzone(uint8_t value, uint8_t deadzone)
{
    // Triggers go from 0 (released) to 255 (fully pressed)
    // Deadzone cuts off low values

    if (value < deadzone) {
        return 0;
    }

    // Scale the remaining range if deadzone is set
    if (deadzone > 0 && deadzone < 255) {
        uint16_t activeRange = 255 - deadzone;
        uint16_t adjusted = value - deadzone;
        uint16_t scaled = (adjusted * 255) / activeRange;

        return (scaled > 255) ? 255 : (uint8_t)scaled;
    }

    return value;
}

// =============================================================================
// MARK: - Static Utilities
// =============================================================================

void InputTranslator::initializeNeutralReport(BigbenHIDReport* report)
{
    if (report == nullptr) {
        return;
    }

    memset(report, 0, sizeof(BigbenHIDReport));

    report->reportId = BIGBEN_REPORT_ID_INPUT;
    report->buttons = 0;
    report->leftStickX = INPUT_TRANSLATOR_AXIS_CENTER;
    report->leftStickY = INPUT_TRANSLATOR_AXIS_CENTER;
    report->rightStickX = INPUT_TRANSLATOR_AXIS_CENTER;
    report->rightStickY = INPUT_TRANSLATOR_AXIS_CENTER;
    report->leftTrigger = 0;
    report->rightTrigger = 0;
    report->hatSwitch = INPUT_TRANSLATOR_HAT_NEUTRAL;
}
