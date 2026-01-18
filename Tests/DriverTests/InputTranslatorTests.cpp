//
//  InputTranslatorTests.cpp
//  BigbenControllerDriver Tests
//
//  Unit tests for the InputTranslator class.
//  Tests button mapping, axis translation, D-pad conversion, and edge cases.
//
//  Copyright (c) 2024. Licensed under MIT License.
//

// Simple test framework macros (can be replaced with Google Test or Catch2)
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Include the classes under test
#include "../../BigbenControllerDriver/Sources/InputTranslator.h"

// =============================================================================
// MARK: - Test Framework Macros
// =============================================================================

#define TEST_CASE(name) \
    void test_##name(); \
    static TestRegistrar registrar_##name(#name, test_##name); \
    void test_##name()

#define ASSERT_TRUE(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ \
                      << " - Expected true: " #expr << std::endl; \
            testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_FALSE(expr) \
    do { \
        if (expr) { \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ \
                      << " - Expected false: " #expr << std::endl; \
            testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(expected, actual) \
    do { \
        auto _expected = (expected); \
        auto _actual = (actual); \
        if (_expected != _actual) { \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ \
                      << " - Expected " << (int)_expected << " but got " << (int)_actual \
                      << " (" #expected " vs " #actual ")" << std::endl; \
            testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NE(val1, val2) \
    do { \
        if ((val1) == (val2)) { \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ \
                      << " - Expected different values: " #val1 " and " #val2 << std::endl; \
            testsFailed++; \
            return; \
        } \
    } while(0)

#define ASSERT_NEAR(expected, actual, tolerance) \
    do { \
        auto _expected = (expected); \
        auto _actual = (actual); \
        if (std::abs(_expected - _actual) > (tolerance)) { \
            std::cerr << "FAIL: " << __FILE__ << ":" << __LINE__ \
                      << " - Expected " << (int)_expected << " +/- " << (tolerance) \
                      << " but got " << (int)_actual << std::endl; \
            testsFailed++; \
            return; \
        } \
    } while(0)

// =============================================================================
// MARK: - Test Registry
// =============================================================================

static int testsFailed = 0;
static int testsRun = 0;

typedef void (*TestFunc)();

struct TestEntry {
    const char* name;
    TestFunc func;
    TestEntry* next;
};

static TestEntry* testListHead = nullptr;

struct TestRegistrar {
    TestRegistrar(const char* name, TestFunc func) {
        TestEntry* entry = new TestEntry{name, func, testListHead};
        testListHead = entry;
    }
};

// =============================================================================
// MARK: - Helper Functions
// =============================================================================

static BigbenInputReport createNeutralInput()
{
    BigbenInputReport input;
    memset(&input, 0, sizeof(input));
    input.reportId = BIGBEN_REPORT_ID_INPUT;
    input.leftStickX = 128;
    input.leftStickY = 128;
    input.rightStickX = 128;
    input.rightStickY = 128;
    input.dpad = BIGBEN_DPAD_NEUTRAL;
    input.buttons = 0;
    input.leftTrigger = 0;
    input.rightTrigger = 0;
    return input;
}

// =============================================================================
// MARK: - Construction Tests
// =============================================================================

TEST_CASE(DefaultConstructor_SetsDefaultDeadzone)
{
    InputTranslator translator;
    ASSERT_EQ(INPUT_TRANSLATOR_DEFAULT_DEADZONE, translator.getDeadzone());
    ASSERT_EQ(0, translator.getTriggerDeadzone());
}

TEST_CASE(CustomDeadzoneConstructor)
{
    InputTranslator translator(25);
    ASSERT_EQ(25, translator.getDeadzone());
}

TEST_CASE(DeadzoneConstructor_ClampsToMax)
{
    InputTranslator translator(200);  // Over max of 127
    ASSERT_EQ(127, translator.getDeadzone());
}

TEST_CASE(SetDeadzone_Works)
{
    InputTranslator translator;
    translator.setDeadzone(50);
    ASSERT_EQ(50, translator.getDeadzone());
}

TEST_CASE(SetDeadzone_ClampsToMax)
{
    InputTranslator translator;
    translator.setDeadzone(255);
    ASSERT_EQ(127, translator.getDeadzone());
}

TEST_CASE(SetTriggerDeadzone_Works)
{
    InputTranslator translator;
    translator.setTriggerDeadzone(30);
    ASSERT_EQ(30, translator.getTriggerDeadzone());
}

// =============================================================================
// MARK: - Button Mapping Tests
// =============================================================================

TEST_CASE(TranslateButtons_NoButtons_ReturnsZero)
{
    uint16_t result = InputTranslator::translateButtons(0);
    ASSERT_EQ(0, result);
}

TEST_CASE(TranslateButtons_ButtonA)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_A);
    ASSERT_TRUE((result & (1 << HID_BTN_A)) != 0);
}

TEST_CASE(TranslateButtons_ButtonB)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_B);
    ASSERT_TRUE((result & (1 << HID_BTN_B)) != 0);
}

TEST_CASE(TranslateButtons_ButtonX)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_X);
    ASSERT_TRUE((result & (1 << HID_BTN_X)) != 0);
}

TEST_CASE(TranslateButtons_ButtonY)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_Y);
    ASSERT_TRUE((result & (1 << HID_BTN_Y)) != 0);
}

TEST_CASE(TranslateButtons_LeftBumper)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_LB);
    ASSERT_TRUE((result & (1 << HID_BTN_LB)) != 0);
}

TEST_CASE(TranslateButtons_RightBumper)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_RB);
    ASSERT_TRUE((result & (1 << HID_BTN_RB)) != 0);
}

TEST_CASE(TranslateButtons_LeftTriggerDigital)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_LT);
    ASSERT_TRUE((result & (1 << HID_BTN_LT)) != 0);
}

TEST_CASE(TranslateButtons_RightTriggerDigital)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_RT);
    ASSERT_TRUE((result & (1 << HID_BTN_RT)) != 0);
}

TEST_CASE(TranslateButtons_Back)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_BACK);
    ASSERT_TRUE((result & (1 << HID_BTN_BACK)) != 0);
}

TEST_CASE(TranslateButtons_Start)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_START);
    ASSERT_TRUE((result & (1 << HID_BTN_START)) != 0);
}

TEST_CASE(TranslateButtons_LeftStickClick)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_LSTICK);
    ASSERT_TRUE((result & (1 << HID_BTN_LSTICK)) != 0);
}

TEST_CASE(TranslateButtons_RightStickClick)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_RSTICK);
    ASSERT_TRUE((result & (1 << HID_BTN_RSTICK)) != 0);
}

TEST_CASE(TranslateButtons_Home)
{
    uint16_t result = InputTranslator::translateButtons(BIGBEN_BTN_HOME);
    ASSERT_TRUE((result & (1 << HID_BTN_HOME)) != 0);
}

TEST_CASE(TranslateButtons_AllButtons)
{
    // All 13 Bigben buttons pressed
    uint16_t allButtons = BIGBEN_BTN_A | BIGBEN_BTN_B | BIGBEN_BTN_X | BIGBEN_BTN_Y |
                          BIGBEN_BTN_LB | BIGBEN_BTN_RB | BIGBEN_BTN_LT | BIGBEN_BTN_RT |
                          BIGBEN_BTN_BACK | BIGBEN_BTN_START |
                          BIGBEN_BTN_LSTICK | BIGBEN_BTN_RSTICK | BIGBEN_BTN_HOME;

    uint16_t result = InputTranslator::translateButtons(allButtons);
    ASSERT_EQ(0x1FFF, result);  // All 13 bits set
}

TEST_CASE(TranslateButtons_MaskesExtraBits)
{
    // Input with bits set beyond 13
    uint16_t result = InputTranslator::translateButtons(0xFFFF);
    ASSERT_EQ(0x1FFF, result);  // Only 13 bits should be set
}

// =============================================================================
// MARK: - D-Pad to Hat Switch Tests
// =============================================================================

TEST_CASE(TranslateDPad_Up)
{
    ASSERT_EQ(0, InputTranslator::translateDPadToHat(BIGBEN_DPAD_UP));
}

TEST_CASE(TranslateDPad_UpRight)
{
    ASSERT_EQ(1, InputTranslator::translateDPadToHat(BIGBEN_DPAD_UP_RIGHT));
}

TEST_CASE(TranslateDPad_Right)
{
    ASSERT_EQ(2, InputTranslator::translateDPadToHat(BIGBEN_DPAD_RIGHT));
}

TEST_CASE(TranslateDPad_DownRight)
{
    ASSERT_EQ(3, InputTranslator::translateDPadToHat(BIGBEN_DPAD_DOWN_RIGHT));
}

TEST_CASE(TranslateDPad_Down)
{
    ASSERT_EQ(4, InputTranslator::translateDPadToHat(BIGBEN_DPAD_DOWN));
}

TEST_CASE(TranslateDPad_DownLeft)
{
    ASSERT_EQ(5, InputTranslator::translateDPadToHat(BIGBEN_DPAD_DOWN_LEFT));
}

TEST_CASE(TranslateDPad_Left)
{
    ASSERT_EQ(6, InputTranslator::translateDPadToHat(BIGBEN_DPAD_LEFT));
}

TEST_CASE(TranslateDPad_UpLeft)
{
    ASSERT_EQ(7, InputTranslator::translateDPadToHat(BIGBEN_DPAD_UP_LEFT));
}

TEST_CASE(TranslateDPad_Neutral)
{
    ASSERT_EQ(8, InputTranslator::translateDPadToHat(BIGBEN_DPAD_NEUTRAL));
}

TEST_CASE(TranslateDPad_InvalidValue_ReturnsNeutral)
{
    // Invalid D-pad values should return neutral
    ASSERT_EQ(INPUT_TRANSLATOR_HAT_NEUTRAL, InputTranslator::translateDPadToHat(9));
    ASSERT_EQ(INPUT_TRANSLATOR_HAT_NEUTRAL, InputTranslator::translateDPadToHat(100));
    ASSERT_EQ(INPUT_TRANSLATOR_HAT_NEUTRAL, InputTranslator::translateDPadToHat(255));
}

// =============================================================================
// MARK: - Axis Deadzone Tests
// =============================================================================

TEST_CASE(ApplyDeadzone_CenterValue_RemainsCenter)
{
    uint8_t result = InputTranslator::applyDeadzone(128, 12);
    ASSERT_EQ(128, result);
}

TEST_CASE(ApplyDeadzone_WithinDeadzone_SnapsToCenter)
{
    // Values within deadzone should snap to center
    ASSERT_EQ(128, InputTranslator::applyDeadzone(128 + 5, 12));
    ASSERT_EQ(128, InputTranslator::applyDeadzone(128 - 5, 12));
    ASSERT_EQ(128, InputTranslator::applyDeadzone(128 + 11, 12));
    ASSERT_EQ(128, InputTranslator::applyDeadzone(128 - 11, 12));
}

TEST_CASE(ApplyDeadzone_OutsideDeadzone_ScalesCorrectly)
{
    // Value just outside deadzone should scale from 0
    uint8_t result = InputTranslator::applyDeadzone(128 + 13, 12);
    ASSERT_NE(128, result);  // Should not be center
    ASSERT_TRUE(result > 128);  // Should be positive direction
}

TEST_CASE(ApplyDeadzone_MaxValue_RemainsMax)
{
    uint8_t result = InputTranslator::applyDeadzone(255, 12);
    ASSERT_EQ(255, result);
}

TEST_CASE(ApplyDeadzone_MinValue_RemainsMin)
{
    uint8_t result = InputTranslator::applyDeadzone(0, 12);
    ASSERT_EQ(0, result);
}

TEST_CASE(ApplyDeadzone_ZeroDeadzone_PassesThrough)
{
    ASSERT_EQ(100, InputTranslator::applyDeadzone(100, 0));
    ASSERT_EQ(200, InputTranslator::applyDeadzone(200, 0));
    ASSERT_EQ(50, InputTranslator::applyDeadzone(50, 0));
}

TEST_CASE(ApplyDeadzone_LargeDeadzone_WorksCorrectly)
{
    // With a large deadzone, only extreme values should pass through
    uint8_t result = InputTranslator::applyDeadzone(128 + 64, 60);
    ASSERT_NE(128, result);
    ASSERT_TRUE(result > 128);
}

// =============================================================================
// MARK: - Trigger Deadzone Tests
// =============================================================================

TEST_CASE(ApplyTriggerDeadzone_ZeroValue_RemainsZero)
{
    ASSERT_EQ(0, InputTranslator::applyTriggerDeadzone(0, 30));
}

TEST_CASE(ApplyTriggerDeadzone_BelowDeadzone_ReturnsZero)
{
    ASSERT_EQ(0, InputTranslator::applyTriggerDeadzone(10, 30));
    ASSERT_EQ(0, InputTranslator::applyTriggerDeadzone(29, 30));
}

TEST_CASE(ApplyTriggerDeadzone_AboveDeadzone_ScalesCorrectly)
{
    uint8_t result = InputTranslator::applyTriggerDeadzone(31, 30);
    ASSERT_NE(0, result);
    ASSERT_TRUE(result > 0);
}

TEST_CASE(ApplyTriggerDeadzone_MaxValue_RemainsMax)
{
    ASSERT_EQ(255, InputTranslator::applyTriggerDeadzone(255, 30));
}

TEST_CASE(ApplyTriggerDeadzone_NoDeadzone_PassesThrough)
{
    ASSERT_EQ(50, InputTranslator::applyTriggerDeadzone(50, 0));
    ASSERT_EQ(100, InputTranslator::applyTriggerDeadzone(100, 0));
}

// =============================================================================
// MARK: - Full Translation Tests
// =============================================================================

TEST_CASE(Translate_NullInput_ReturnsFalse)
{
    InputTranslator translator;
    BigbenHIDReport output;

    ASSERT_FALSE(translator.translate(nullptr, &output));
}

TEST_CASE(Translate_NullOutput_ReturnsFalse)
{
    InputTranslator translator;
    BigbenInputReport input = createNeutralInput();

    ASSERT_FALSE(translator.translate(&input, nullptr));
}

TEST_CASE(Translate_NeutralInput_ProducesNeutralOutput)
{
    InputTranslator translator;
    BigbenInputReport input = createNeutralInput();
    BigbenHIDReport output;

    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(BIGBEN_REPORT_ID_INPUT, output.reportId);
    ASSERT_EQ(0, output.buttons);
    ASSERT_EQ(128, output.leftStickX);
    ASSERT_EQ(128, output.leftStickY);
    ASSERT_EQ(128, output.rightStickX);
    ASSERT_EQ(128, output.rightStickY);
    ASSERT_EQ(0, output.leftTrigger);
    ASSERT_EQ(0, output.rightTrigger);
    ASSERT_EQ(INPUT_TRANSLATOR_HAT_NEUTRAL, output.hatSwitch);
}

TEST_CASE(Translate_ButtonsAreTranslated)
{
    InputTranslator translator;
    BigbenInputReport input = createNeutralInput();
    input.buttons = BIGBEN_BTN_A | BIGBEN_BTN_START;

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_TRUE((output.buttons & (1 << HID_BTN_A)) != 0);
    ASSERT_TRUE((output.buttons & (1 << HID_BTN_START)) != 0);
    ASSERT_FALSE((output.buttons & (1 << HID_BTN_B)) != 0);
}

TEST_CASE(Translate_AxesWithinDeadzone_SnapToCenter)
{
    InputTranslator translator(20);  // 20-unit deadzone
    BigbenInputReport input = createNeutralInput();
    input.leftStickX = 128 + 10;  // Within deadzone
    input.leftStickY = 128 - 15;  // Within deadzone

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(128, output.leftStickX);
    ASSERT_EQ(128, output.leftStickY);
}

TEST_CASE(Translate_AxesOutsideDeadzone_Translate)
{
    InputTranslator translator(10);
    BigbenInputReport input = createNeutralInput();
    input.leftStickX = 200;  // Well outside deadzone

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_NE(128, output.leftStickX);
    ASSERT_TRUE(output.leftStickX > 128);
}

TEST_CASE(Translate_DPadIsTranslated)
{
    InputTranslator translator;
    BigbenInputReport input = createNeutralInput();
    input.dpad = BIGBEN_DPAD_RIGHT;

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(2, output.hatSwitch);  // Right = 2
}

TEST_CASE(Translate_TriggersAreTranslated)
{
    InputTranslator translator;
    BigbenInputReport input = createNeutralInput();
    input.leftTrigger = 200;
    input.rightTrigger = 100;

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(200, output.leftTrigger);
    ASSERT_EQ(100, output.rightTrigger);
}

TEST_CASE(Translate_TriggersWithDeadzone)
{
    InputTranslator translator;
    translator.setTriggerDeadzone(50);

    BigbenInputReport input = createNeutralInput();
    input.leftTrigger = 30;   // Below deadzone
    input.rightTrigger = 100; // Above deadzone

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(0, output.leftTrigger);  // Should be zero
    ASSERT_NE(0, output.rightTrigger); // Should be non-zero
}

// =============================================================================
// MARK: - Edge Case Tests
// =============================================================================

TEST_CASE(Translate_AllButtonsPressed)
{
    InputTranslator translator;
    BigbenInputReport input = createNeutralInput();
    input.buttons = 0x1FFF;  // All 13 buttons

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(0x1FFF, output.buttons);
}

TEST_CASE(Translate_AllAxesMaximum)
{
    InputTranslator translator(0);  // No deadzone
    BigbenInputReport input = createNeutralInput();
    input.leftStickX = 255;
    input.leftStickY = 255;
    input.rightStickX = 255;
    input.rightStickY = 255;
    input.leftTrigger = 255;
    input.rightTrigger = 255;

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(255, output.leftStickX);
    ASSERT_EQ(255, output.leftStickY);
    ASSERT_EQ(255, output.rightStickX);
    ASSERT_EQ(255, output.rightStickY);
    ASSERT_EQ(255, output.leftTrigger);
    ASSERT_EQ(255, output.rightTrigger);
}

TEST_CASE(Translate_AllAxesMinimum)
{
    InputTranslator translator(0);  // No deadzone
    BigbenInputReport input = createNeutralInput();
    input.leftStickX = 0;
    input.leftStickY = 0;
    input.rightStickX = 0;
    input.rightStickY = 0;

    BigbenHIDReport output;
    ASSERT_TRUE(translator.translate(&input, &output));

    ASSERT_EQ(0, output.leftStickX);
    ASSERT_EQ(0, output.leftStickY);
    ASSERT_EQ(0, output.rightStickX);
    ASSERT_EQ(0, output.rightStickY);
}

// =============================================================================
// MARK: - Static Utility Tests
// =============================================================================

TEST_CASE(InitializeNeutralReport_SetsCorrectValues)
{
    BigbenHIDReport report;
    memset(&report, 0xFF, sizeof(report));  // Fill with garbage

    InputTranslator::initializeNeutralReport(&report);

    ASSERT_EQ(BIGBEN_REPORT_ID_INPUT, report.reportId);
    ASSERT_EQ(0, report.buttons);
    ASSERT_EQ(128, report.leftStickX);
    ASSERT_EQ(128, report.leftStickY);
    ASSERT_EQ(128, report.rightStickX);
    ASSERT_EQ(128, report.rightStickY);
    ASSERT_EQ(0, report.leftTrigger);
    ASSERT_EQ(0, report.rightTrigger);
    ASSERT_EQ(INPUT_TRANSLATOR_HAT_NEUTRAL, report.hatSwitch);
}

TEST_CASE(InitializeNeutralReport_NullPointer_DoesNotCrash)
{
    // Should not crash
    InputTranslator::initializeNeutralReport(nullptr);
}

TEST_CASE(IsButtonPressed_DetectsPressed)
{
    BigbenHIDReport report;
    InputTranslator::initializeNeutralReport(&report);
    report.buttons = (1 << HID_BTN_A) | (1 << HID_BTN_X);

    ASSERT_TRUE(InputTranslator::isButtonPressed(&report, HID_BTN_A));
    ASSERT_TRUE(InputTranslator::isButtonPressed(&report, HID_BTN_X));
}

TEST_CASE(IsButtonPressed_DetectsNotPressed)
{
    BigbenHIDReport report;
    InputTranslator::initializeNeutralReport(&report);
    report.buttons = (1 << HID_BTN_A);

    ASSERT_FALSE(InputTranslator::isButtonPressed(&report, HID_BTN_B));
    ASSERT_FALSE(InputTranslator::isButtonPressed(&report, HID_BTN_Y));
}

TEST_CASE(IsButtonPressed_NullReport_ReturnsFalse)
{
    ASSERT_FALSE(InputTranslator::isButtonPressed(nullptr, HID_BTN_A));
}

// =============================================================================
// MARK: - Main Test Runner
// =============================================================================

int main(int argc, char* argv[])
{
    std::cout << "==================================================" << std::endl;
    std::cout << "InputTranslator Unit Tests" << std::endl;
    std::cout << "==================================================" << std::endl;

    // Run all registered tests
    TestEntry* current = testListHead;
    while (current != nullptr) {
        testsRun++;
        int failedBefore = testsFailed;

        std::cout << "Running: " << current->name << "... ";
        current->func();

        if (testsFailed == failedBefore) {
            std::cout << "PASS" << std::endl;
        }

        current = current->next;
    }

    std::cout << "==================================================" << std::endl;
    std::cout << "Results: " << (testsRun - testsFailed) << "/" << testsRun << " tests passed" << std::endl;

    if (testsFailed > 0) {
        std::cout << "FAILED: " << testsFailed << " test(s) failed" << std::endl;
        return 1;
    }

    std::cout << "SUCCESS: All tests passed!" << std::endl;
    return 0;
}
