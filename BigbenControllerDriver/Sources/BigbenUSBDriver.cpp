//
//  BigbenUSBDriver.cpp
//  BigbenControllerDriver
//
//  DriverKit USB driver implementation for Bigben Interactive game controllers.
//  Handles USB communication, input report parsing, and HID translation for
//  the Bigben PC Compact Controller (VID: 0x146b, PID: 0x0603).
//

#include <os/log.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/IOService.h>
#include <DriverKit/IOMemoryDescriptor.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <DriverKit/OSData.h>
#include <USBDriverKit/IOUSBHostDevice.h>
#include <USBDriverKit/IOUSBHostInterface.h>
#include <USBDriverKit/IOUSBHostPipe.h>
#include <HIDDriverKit/IOUserUSBHostHIDDevice.h>
#include <HIDDriverKit/IOHIDDeviceKeys.h>

#include "BigbenUSBDriver.h"
#include "../../Shared/BigbenProtocol.h"

// =============================================================================
// MARK: - Constants and Configuration
// =============================================================================

#define kBigbenDriverClassName          "BigbenUSBDriver"
#define kBigbenLogSubsystem             "com.bigben.controller.driver"

// USB endpoint constants
#define kBigbenInputEndpointAddress     0x81    // EP1 IN
#define kBigbenOutputEndpointAddress    0x02    // EP2 OUT
#define kBigbenInputReportInterval      4       // 4ms polling interval
#define kBigbenMaxPendingReads          2       // Number of concurrent reads

// Report sizes
#define kBigbenInputReportSize          64
#define kBigbenOutputReportSize         8

// Logging macros
#define LOG_INFO(fmt, ...)    os_log_info(OS_LOG_DEFAULT, "[BigbenUSB] " fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)   os_log_error(OS_LOG_DEFAULT, "[BigbenUSB] ERROR: " fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...)   os_log_debug(OS_LOG_DEFAULT, "[BigbenUSB] DEBUG: " fmt, ##__VA_ARGS__)

// =============================================================================
// MARK: - HID Report Descriptor
// =============================================================================

// Standard gamepad HID report descriptor that maps the Bigben controller
// to a generic gamepad interface compatible with macOS game frameworks.
static const uint8_t kBigbenHIDReportDescriptor[] = {
    // Usage Page (Generic Desktop)
    0x05, 0x01,
    // Usage (Gamepad)
    0x09, 0x05,
    // Collection (Application)
    0xA1, 0x01,

        // Report ID (1)
        0x85, 0x01,

        // =====================================================================
        // Axes (Left Stick, Right Stick, Triggers)
        // =====================================================================

        // Usage Page (Generic Desktop)
        0x05, 0x01,

        // Left Stick X/Y
        // Usage (X)
        0x09, 0x30,
        // Usage (Y)
        0x09, 0x31,
        // Right Stick X/Y
        // Usage (Rx)
        0x09, 0x33,
        // Usage (Ry)
        0x09, 0x34,
        // Logical Minimum (0)
        0x15, 0x00,
        // Logical Maximum (255)
        0x26, 0xFF, 0x00,
        // Report Size (8)
        0x75, 0x08,
        // Report Count (4)
        0x95, 0x04,
        // Input (Data, Variable, Absolute)
        0x81, 0x02,

        // Triggers (Z and Rz)
        // Usage (Z) - Left Trigger
        0x09, 0x32,
        // Usage (Rz) - Right Trigger
        0x09, 0x35,
        // Report Count (2)
        0x95, 0x02,
        // Input (Data, Variable, Absolute)
        0x81, 0x02,

        // =====================================================================
        // Hat Switch (D-Pad)
        // =====================================================================

        // Usage (Hat Switch)
        0x09, 0x39,
        // Logical Minimum (0)
        0x15, 0x00,
        // Logical Maximum (7)
        0x25, 0x07,
        // Physical Minimum (0)
        0x35, 0x00,
        // Physical Maximum (315) degrees
        0x46, 0x3B, 0x01,
        // Unit (Degrees)
        0x65, 0x14,
        // Report Size (4)
        0x75, 0x04,
        // Report Count (1)
        0x95, 0x01,
        // Input (Data, Variable, Absolute, Null State)
        0x81, 0x42,

        // Padding (4 bits to align to byte boundary)
        // Report Size (4)
        0x75, 0x04,
        // Report Count (1)
        0x95, 0x01,
        // Input (Constant)
        0x81, 0x01,

        // =====================================================================
        // Buttons (13 buttons)
        // =====================================================================

        // Usage Page (Button)
        0x05, 0x09,
        // Usage Minimum (Button 1)
        0x19, 0x01,
        // Usage Maximum (Button 13)
        0x29, 0x0D,
        // Logical Minimum (0)
        0x15, 0x00,
        // Logical Maximum (1)
        0x25, 0x01,
        // Report Size (1)
        0x75, 0x01,
        // Report Count (13)
        0x95, 0x0D,
        // Input (Data, Variable, Absolute)
        0x81, 0x02,

        // Padding (3 bits to complete the button byte pair)
        // Report Size (1)
        0x75, 0x01,
        // Report Count (3)
        0x95, 0x03,
        // Input (Constant)
        0x81, 0x01,

        // =====================================================================
        // Output Report (LED and Rumble)
        // =====================================================================

        // Report ID (2) for output
        0x85, 0x02,

        // Usage Page (LEDs)
        0x05, 0x08,
        // Usage Minimum (LED 1)
        0x19, 0x01,
        // Usage Maximum (LED 4)
        0x29, 0x04,
        // Report Count (4)
        0x95, 0x04,
        // Report Size (1)
        0x75, 0x01,
        // Output (Data, Variable, Absolute)
        0x91, 0x02,

        // Padding (4 bits)
        0x95, 0x04,
        0x91, 0x01,

        // Usage Page (Physical Interface Device)
        0x05, 0x0F,
        // Usage (Set Effect Report)
        0x09, 0x21,
        // Collection (Logical)
        0xA1, 0x02,
            // Usage (DC Enable Actuators)
            0x09, 0x97,
            // Logical Minimum (0)
            0x15, 0x00,
            // Logical Maximum (1)
            0x25, 0x01,
            // Report Size (1)
            0x75, 0x01,
            // Report Count (1)
            0x95, 0x01,
            // Output (Data, Variable, Absolute)
            0x91, 0x02,

            // Padding (7 bits)
            0x95, 0x07,
            0x91, 0x01,

            // Usage (Magnitude) - strong motor
            0x09, 0x70,
            // Logical Maximum (255)
            0x26, 0xFF, 0x00,
            // Report Size (8)
            0x75, 0x08,
            // Report Count (1)
            0x95, 0x01,
            // Output (Data, Variable, Absolute)
            0x91, 0x02,

            // Usage (Magnitude) - weak motor
            0x09, 0x70,
            // Output (Data, Variable, Absolute)
            0x91, 0x02,
        // End Collection
        0xC0,

    // End Collection
    0xC0
};

static const size_t kBigbenHIDReportDescriptorSize = sizeof(kBigbenHIDReportDescriptor);

// =============================================================================
// MARK: - Internal Data Structures
// =============================================================================

struct BigbenUSBDriver_IVars {
    // USB Objects
    IOUSBHostInterface      *interface;
    IOUSBHostPipe           *inputPipe;
    IOUSBHostPipe           *outputPipe;

    // Memory Descriptors for I/O
    IOBufferMemoryDescriptor *inputBuffer;
    IOBufferMemoryDescriptor *outputBuffer;
    IOMemoryDescriptor       *hidDescriptor;

    // Async I/O Actions
    OSAction                *readAction;
    OSAction                *writeAction;

    // State tracking
    bool                     isStarted;
    bool                     isPolling;
    bool                     deviceConnected;

    // Last known controller state for change detection
    BigbenInputReport        lastReport;
    bool                     hasLastReport;

    // Statistics
    uint64_t                 reportsReceived;
    uint64_t                 reportErrors;
    uint64_t                 outputReportsSent;
};

// =============================================================================
// MARK: - IOService Lifecycle Implementation
// =============================================================================

bool BigbenUSBDriver::init()
{
    LOG_INFO("Initializing BigbenUSBDriver");

    if (!super::init()) {
        LOG_ERROR("super::init() failed");
        return false;
    }

    // Allocate instance variables
    ivars = IONewZero(BigbenUSBDriver_IVars, 1);
    if (ivars == nullptr) {
        LOG_ERROR("Failed to allocate ivars");
        return false;
    }

    // Initialize state
    ivars->interface = nullptr;
    ivars->inputPipe = nullptr;
    ivars->outputPipe = nullptr;
    ivars->inputBuffer = nullptr;
    ivars->outputBuffer = nullptr;
    ivars->hidDescriptor = nullptr;
    ivars->readAction = nullptr;
    ivars->writeAction = nullptr;
    ivars->isStarted = false;
    ivars->isPolling = false;
    ivars->deviceConnected = false;
    ivars->hasLastReport = false;
    ivars->reportsReceived = 0;
    ivars->reportErrors = 0;
    ivars->outputReportsSent = 0;

    LOG_INFO("BigbenUSBDriver initialized successfully");
    return true;
}

void BigbenUSBDriver::free()
{
    LOG_INFO("Freeing BigbenUSBDriver");

    CleanupResources();

    if (ivars != nullptr) {
        IOSafeDeleteNULL(ivars, BigbenUSBDriver_IVars, 1);
    }

    super::free();
}

kern_return_t BigbenUSBDriver::Start(IOService *provider)
{
    kern_return_t ret;

    LOG_INFO("Starting BigbenUSBDriver");

    if (ivars == nullptr) {
        LOG_ERROR("ivars is null in Start()");
        return kIOReturnInvalid;
    }

    // Call parent Start
    ret = super::Start(provider);
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("super::Start() failed with error 0x%x", ret);
        return ret;
    }

    // Get the USB interface from the provider
    ivars->interface = OSDynamicCast(IOUSBHostInterface, provider);
    if (ivars->interface == nullptr) {
        LOG_ERROR("Provider is not an IOUSBHostInterface");
        super::Stop(provider);
        return kIOReturnNotFound;
    }

    // Retain the interface
    ivars->interface->retain();

    // Configure the device
    ret = ConfigureDevice();
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("ConfigureDevice() failed with error 0x%x", ret);
        super::Stop(provider);
        return ret;
    }

    // Open the interface
    ret = OpenInterface();
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("OpenInterface() failed with error 0x%x", ret);
        super::Stop(provider);
        return ret;
    }

    // Set up endpoints
    ret = SetupInterruptInEndpoint();
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("SetupInterruptInEndpoint() failed with error 0x%x", ret);
        super::Stop(provider);
        return ret;
    }

    ret = SetupInterruptOutEndpoint();
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("SetupInterruptOutEndpoint() failed with error 0x%x", ret);
        super::Stop(provider);
        return ret;
    }

    // Create HID report descriptor
    ret = CreateHIDReportDescriptor();
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("CreateHIDReportDescriptor() failed with error 0x%x", ret);
        super::Stop(provider);
        return ret;
    }

    // Start input polling
    ret = StartInputPolling();
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("StartInputPolling() failed with error 0x%x", ret);
        super::Stop(provider);
        return ret;
    }

    ivars->isStarted = true;
    ivars->deviceConnected = true;

    LOG_INFO("BigbenUSBDriver started successfully");
    LOG_INFO("Controller: Bigben PC Compact Controller (VID: 0x%04x, PID: 0x%04x)",
             BIGBEN_VENDOR_ID, BIGBEN_PRODUCT_PC_COMPACT);

    // Register service for other drivers/applications
    RegisterService();

    return kIOReturnSuccess;
}

kern_return_t BigbenUSBDriver::Stop(IOService *provider)
{
    LOG_INFO("Stopping BigbenUSBDriver");

    if (ivars != nullptr) {
        ivars->isStarted = false;
        ivars->deviceConnected = false;

        // Stop input polling
        StopInputPolling();

        // Log statistics
        LOG_INFO("Statistics: Reports received: %llu, Errors: %llu, Output reports sent: %llu",
                 ivars->reportsReceived, ivars->reportErrors, ivars->outputReportsSent);

        // Clean up resources
        CleanupResources();
    }

    LOG_INFO("BigbenUSBDriver stopped");
    return super::Stop(provider);
}

// =============================================================================
// MARK: - USB Configuration
// =============================================================================

kern_return_t BigbenUSBDriver::ConfigureDevice()
{
    LOG_DEBUG("Configuring device");

    if (ivars->interface == nullptr) {
        LOG_ERROR("Interface is null in ConfigureDevice()");
        return kIOReturnNotAttached;
    }

    // The interface should already be configured by the time we match
    // Just verify we have the expected configuration
    LOG_INFO("Device configuration complete");
    return kIOReturnSuccess;
}

kern_return_t BigbenUSBDriver::OpenInterface()
{
    kern_return_t ret;

    LOG_DEBUG("Opening USB interface");

    if (ivars->interface == nullptr) {
        LOG_ERROR("Interface is null in OpenInterface()");
        return kIOReturnNotAttached;
    }

    // Open the interface
    ret = ivars->interface->Open(this, 0, nullptr);
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("Failed to open interface: 0x%x", ret);
        return ret;
    }

    LOG_INFO("USB interface opened successfully");
    return kIOReturnSuccess;
}

kern_return_t BigbenUSBDriver::SetupInterruptInEndpoint()
{
    kern_return_t ret;

    LOG_DEBUG("Setting up interrupt IN endpoint");

    if (ivars->interface == nullptr) {
        LOG_ERROR("Interface is null in SetupInterruptInEndpoint()");
        return kIOReturnNotAttached;
    }

    // Copy the pipe for the interrupt IN endpoint
    ret = ivars->interface->CopyPipe(kBigbenInputEndpointAddress, &ivars->inputPipe);
    if (ret != kIOReturnSuccess || ivars->inputPipe == nullptr) {
        LOG_ERROR("Failed to get input pipe: 0x%x", ret);
        return ret != kIOReturnSuccess ? ret : kIOReturnNotFound;
    }

    // Allocate input buffer
    ret = IOBufferMemoryDescriptor::Create(
        kIOMemoryDirectionIn,
        kBigbenInputReportSize,
        0,
        &ivars->inputBuffer
    );
    if (ret != kIOReturnSuccess || ivars->inputBuffer == nullptr) {
        LOG_ERROR("Failed to create input buffer: 0x%x", ret);
        return ret != kIOReturnSuccess ? ret : kIOReturnNoMemory;
    }

    // Create the async read completion action
    ret = CreateActionReadComplete(
        sizeof(void*),
        &ivars->readAction
    );
    if (ret != kIOReturnSuccess || ivars->readAction == nullptr) {
        LOG_ERROR("Failed to create read action: 0x%x", ret);
        return ret != kIOReturnSuccess ? ret : kIOReturnNoMemory;
    }

    LOG_INFO("Interrupt IN endpoint (0x%02x) configured", kBigbenInputEndpointAddress);
    return kIOReturnSuccess;
}

kern_return_t BigbenUSBDriver::SetupInterruptOutEndpoint()
{
    kern_return_t ret;

    LOG_DEBUG("Setting up interrupt OUT endpoint");

    if (ivars->interface == nullptr) {
        LOG_ERROR("Interface is null in SetupInterruptOutEndpoint()");
        return kIOReturnNotAttached;
    }

    // Copy the pipe for the interrupt OUT endpoint
    ret = ivars->interface->CopyPipe(kBigbenOutputEndpointAddress, &ivars->outputPipe);
    if (ret != kIOReturnSuccess || ivars->outputPipe == nullptr) {
        // Output endpoint is optional - some controllers might not have it
        LOG_INFO("Output pipe not available (may be normal for some controller variants)");
        return kIOReturnSuccess;
    }

    // Allocate output buffer
    ret = IOBufferMemoryDescriptor::Create(
        kIOMemoryDirectionOut,
        kBigbenOutputReportSize,
        0,
        &ivars->outputBuffer
    );
    if (ret != kIOReturnSuccess || ivars->outputBuffer == nullptr) {
        LOG_ERROR("Failed to create output buffer: 0x%x", ret);
        return ret != kIOReturnSuccess ? ret : kIOReturnNoMemory;
    }

    // Create the async write completion action
    ret = CreateActionWriteComplete(
        sizeof(void*),
        &ivars->writeAction
    );
    if (ret != kIOReturnSuccess || ivars->writeAction == nullptr) {
        LOG_ERROR("Failed to create write action: 0x%x", ret);
        return ret != kIOReturnSuccess ? ret : kIOReturnNoMemory;
    }

    LOG_INFO("Interrupt OUT endpoint (0x%02x) configured", kBigbenOutputEndpointAddress);
    return kIOReturnSuccess;
}

kern_return_t BigbenUSBDriver::CreateHIDReportDescriptor()
{
    kern_return_t ret;

    LOG_DEBUG("Creating HID report descriptor");

    // Create memory descriptor for the HID report descriptor
    ret = IOBufferMemoryDescriptor::Create(
        kIOMemoryDirectionOut,
        kBigbenHIDReportDescriptorSize,
        0,
        (IOBufferMemoryDescriptor**)&ivars->hidDescriptor
    );
    if (ret != kIOReturnSuccess || ivars->hidDescriptor == nullptr) {
        LOG_ERROR("Failed to create HID descriptor buffer: 0x%x", ret);
        return ret != kIOReturnSuccess ? ret : kIOReturnNoMemory;
    }

    // Get the address of the buffer and copy the descriptor
    IOBufferMemoryDescriptor *bufferDesc = OSDynamicCast(IOBufferMemoryDescriptor, ivars->hidDescriptor);
    if (bufferDesc == nullptr) {
        LOG_ERROR("Failed to cast HID descriptor to buffer");
        return kIOReturnInternalError;
    }

    uint64_t address = 0;
    uint64_t length = 0;
    ret = bufferDesc->Map(0, 0, 0, 0, &address, &length);
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("Failed to map HID descriptor buffer: 0x%x", ret);
        return ret;
    }

    // Copy the descriptor data
    memcpy((void*)address, kBigbenHIDReportDescriptor, kBigbenHIDReportDescriptorSize);

    LOG_INFO("HID report descriptor created (%zu bytes)", kBigbenHIDReportDescriptorSize);
    return kIOReturnSuccess;
}

// =============================================================================
// MARK: - Input Report Handling
// =============================================================================

kern_return_t BigbenUSBDriver::StartInputPolling()
{
    kern_return_t ret;

    LOG_DEBUG("Starting input polling");

    if (ivars->inputPipe == nullptr || ivars->inputBuffer == nullptr || ivars->readAction == nullptr) {
        LOG_ERROR("Input pipe/buffer/action not configured");
        return kIOReturnNotReady;
    }

    if (ivars->isPolling) {
        LOG_DEBUG("Already polling");
        return kIOReturnSuccess;
    }

    // Queue the first async read
    ret = ivars->inputPipe->AsyncIO(
        ivars->inputBuffer,
        kBigbenInputReportSize,
        ivars->readAction,
        0
    );
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("Failed to start async read: 0x%x", ret);
        return ret;
    }

    ivars->isPolling = true;
    LOG_INFO("Input polling started");
    return kIOReturnSuccess;
}

void BigbenUSBDriver::StopInputPolling()
{
    LOG_DEBUG("Stopping input polling");

    if (!ivars->isPolling) {
        return;
    }

    ivars->isPolling = false;

    // Abort any pending I/O on the input pipe
    if (ivars->inputPipe != nullptr) {
        ivars->inputPipe->Abort(kIOUSBAbortAsyncOption, kIOReturnAborted);
    }

    LOG_INFO("Input polling stopped");
}

void BigbenUSBDriver::ReadComplete(OSAction *action, IOReturn status, uint32_t actualByteCount)
{
    // Check for abort/disconnect
    if (status == kIOReturnAborted || status == kIOReturnNotResponding) {
        LOG_INFO("Read aborted or device not responding (status: 0x%x)", status);
        return;
    }

    if (status != kIOReturnSuccess) {
        LOG_ERROR("Read completed with error: 0x%x", status);
        ivars->reportErrors++;

        // Try to restart polling if we're still connected
        if (ivars->isPolling && ivars->deviceConnected) {
            kern_return_t ret = ivars->inputPipe->AsyncIO(
                ivars->inputBuffer,
                kBigbenInputReportSize,
                ivars->readAction,
                0
            );
            if (ret != kIOReturnSuccess) {
                LOG_ERROR("Failed to restart async read: 0x%x", ret);
                ivars->isPolling = false;
            }
        }
        return;
    }

    // Validate report size
    if (actualByteCount < sizeof(BigbenInputReport)) {
        LOG_DEBUG("Short read: %u bytes (expected at least %zu)", actualByteCount, sizeof(BigbenInputReport));
        ivars->reportErrors++;
    } else {
        // Get the buffer address
        uint64_t address = 0;
        uint64_t length = 0;
        kern_return_t ret = ivars->inputBuffer->Map(0, 0, 0, 0, &address, &length);
        if (ret == kIOReturnSuccess && address != 0) {
            const uint8_t *data = (const uint8_t*)address;

            // Parse the input report
            ParseInputReport(data, actualByteCount);

            ivars->reportsReceived++;

            // Forward the report to the HID layer
            uint64_t timestamp = mach_absolute_time();
            handleReport(timestamp, ivars->inputBuffer, actualByteCount,
                        kIOHIDReportTypeInput, 0);
        }
    }

    // Queue the next read if still polling
    if (ivars->isPolling && ivars->deviceConnected) {
        kern_return_t ret = ivars->inputPipe->AsyncIO(
            ivars->inputBuffer,
            kBigbenInputReportSize,
            ivars->readAction,
            0
        );
        if (ret != kIOReturnSuccess) {
            LOG_ERROR("Failed to queue next async read: 0x%x", ret);
            ivars->isPolling = false;
        }
    }
}

void BigbenUSBDriver::ParseInputReport(const uint8_t *data, size_t length)
{
    if (length < sizeof(BigbenInputReport)) {
        return;
    }

    const BigbenInputReport *report = (const BigbenInputReport*)data;

    // Verify report ID
    if (report->reportId != BIGBEN_REPORT_ID_INPUT) {
        LOG_DEBUG("Unexpected report ID: 0x%02x", report->reportId);
        return;
    }

    // Log state changes for debugging (only if changed)
    if (ivars->hasLastReport) {
        bool changed = memcmp(report, &ivars->lastReport, sizeof(BigbenInputReport)) != 0;
        if (changed) {
            LOG_DEBUG("Input: LX=%3u LY=%3u RX=%3u RY=%3u DPad=%u Btn=0x%04x LT=%3u RT=%3u",
                     report->leftStickX, report->leftStickY,
                     report->rightStickX, report->rightStickY,
                     report->dpad, report->buttons,
                     report->leftTrigger, report->rightTrigger);
        }
    }

    // Store the report for change detection
    memcpy(&ivars->lastReport, report, sizeof(BigbenInputReport));
    ivars->hasLastReport = true;
}

void BigbenUSBDriver::LogControllerState()
{
    if (!ivars->hasLastReport) {
        LOG_INFO("No controller state available");
        return;
    }

    const BigbenInputReport *r = &ivars->lastReport;
    LOG_INFO("Controller State:");
    LOG_INFO("  Left Stick:  X=%d Y=%d", BIGBEN_ANALOG_TO_SIGNED(r->leftStickX),
             BIGBEN_ANALOG_TO_SIGNED(r->leftStickY));
    LOG_INFO("  Right Stick: X=%d Y=%d", BIGBEN_ANALOG_TO_SIGNED(r->rightStickX),
             BIGBEN_ANALOG_TO_SIGNED(r->rightStickY));
    LOG_INFO("  Triggers:    L=%u R=%u", r->leftTrigger, r->rightTrigger);
    LOG_INFO("  D-Pad:       %u", r->dpad);
    LOG_INFO("  Buttons:     0x%04x", r->buttons);
}

// =============================================================================
// MARK: - Output Report Handling
// =============================================================================

void BigbenUSBDriver::WriteComplete(OSAction *action, IOReturn status, uint32_t actualByteCount)
{
    if (status != kIOReturnSuccess) {
        LOG_ERROR("Write completed with error: 0x%x", status);
        return;
    }

    LOG_DEBUG("Output report sent successfully (%u bytes)", actualByteCount);
    ivars->outputReportsSent++;
}

kern_return_t BigbenUSBDriver::SendOutputReport(const uint8_t *data, size_t length)
{
    if (ivars->outputPipe == nullptr || ivars->outputBuffer == nullptr) {
        LOG_DEBUG("Output endpoint not available");
        return kIOReturnNotFound;
    }

    if (length > kBigbenOutputReportSize) {
        LOG_ERROR("Output report too large: %zu bytes", length);
        return kIOReturnBadArgument;
    }

    // Map the output buffer and copy data
    uint64_t address = 0;
    uint64_t bufferLength = 0;
    kern_return_t ret = ivars->outputBuffer->Map(0, 0, 0, 0, &address, &bufferLength);
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("Failed to map output buffer: 0x%x", ret);
        return ret;
    }

    memcpy((void*)address, data, length);

    // Pad remaining bytes with zeros
    if (length < kBigbenOutputReportSize) {
        memset((uint8_t*)address + length, 0, kBigbenOutputReportSize - length);
    }

    // Send the report
    ret = ivars->outputPipe->AsyncIO(
        ivars->outputBuffer,
        kBigbenOutputReportSize,
        ivars->writeAction,
        0
    );
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("Failed to send output report: 0x%x", ret);
        return ret;
    }

    return kIOReturnSuccess;
}

// =============================================================================
// MARK: - IOUserUSBHostHIDDevice Overrides
// =============================================================================

kern_return_t BigbenUSBDriver::getHIDDescriptor(IOMemoryDescriptor **descriptor)
{
    LOG_DEBUG("getHIDDescriptor called");

    if (descriptor == nullptr) {
        return kIOReturnBadArgument;
    }

    if (ivars->hidDescriptor == nullptr) {
        LOG_ERROR("HID descriptor not available");
        return kIOReturnNotReady;
    }

    *descriptor = ivars->hidDescriptor;
    ivars->hidDescriptor->retain();

    return kIOReturnSuccess;
}

kern_return_t BigbenUSBDriver::handleReport(
    uint64_t timestamp,
    IOMemoryDescriptor *report,
    uint32_t reportLength,
    IOHIDReportType type,
    IOOptionBits options)
{
    // Forward to parent implementation for HID processing
    return super::handleReport(timestamp, report, reportLength, type, options);
}

kern_return_t BigbenUSBDriver::setReport(
    IOMemoryDescriptor *report,
    IOHIDReportType reportType,
    IOOptionBits options)
{
    if (report == nullptr) {
        return kIOReturnBadArgument;
    }

    if (reportType != kIOHIDReportTypeOutput) {
        LOG_DEBUG("setReport called with non-output report type: %d", reportType);
        return kIOReturnUnsupported;
    }

    // Get report ID from options
    uint8_t reportID = (uint8_t)(options & 0xFF);

    LOG_DEBUG("setReport: type=%d, reportID=%u", reportType, reportID);

    // Map the report buffer
    uint64_t address = 0;
    uint64_t length = 0;
    kern_return_t ret = report->Map(0, 0, 0, 0, &address, &length);
    if (ret != kIOReturnSuccess) {
        LOG_ERROR("Failed to map output report: 0x%x", ret);
        return ret;
    }

    const uint8_t *data = (const uint8_t*)address;

    // Handle LED report
    if (reportID == BIGBEN_REPORT_ID_LED || (length > 0 && data[0] == BIGBEN_REPORT_ID_LED)) {
        LOG_DEBUG("Processing LED report");
        return SendOutputReport(data, length);
    }

    // Handle Rumble report
    if (reportID == BIGBEN_REPORT_ID_RUMBLE || (length > 0 && data[0] == BIGBEN_REPORT_ID_RUMBLE)) {
        LOG_DEBUG("Processing Rumble report");
        return SendOutputReport(data, length);
    }

    LOG_DEBUG("Unknown output report ID: %u", reportID);
    return kIOReturnUnsupported;
}

kern_return_t BigbenUSBDriver::getReport(
    IOMemoryDescriptor *report,
    IOHIDReportType reportType,
    IOOptionBits options)
{
    if (report == nullptr) {
        return kIOReturnBadArgument;
    }

    // Get report ID from options
    uint8_t reportID = (uint8_t)(options & 0xFF);

    LOG_DEBUG("getReport: type=%d, reportID=%u", reportType, reportID);

    // For input reports, return the last cached report if available
    if (reportType == kIOHIDReportTypeInput && reportID == BIGBEN_REPORT_ID_INPUT) {
        if (!ivars->hasLastReport) {
            return kIOReturnNotReady;
        }

        uint64_t address = 0;
        uint64_t length = 0;
        kern_return_t ret = report->Map(0, 0, 0, 0, &address, &length);
        if (ret != kIOReturnSuccess) {
            return ret;
        }

        size_t copyLength = length < sizeof(BigbenInputReport) ? length : sizeof(BigbenInputReport);
        memcpy((void*)address, &ivars->lastReport, copyLength);

        return kIOReturnSuccess;
    }

    return kIOReturnUnsupported;
}

// =============================================================================
// MARK: - Resource Cleanup
// =============================================================================

void BigbenUSBDriver::CleanupResources()
{
    LOG_DEBUG("Cleaning up resources");

    // Release actions
    if (ivars->readAction != nullptr) {
        ivars->readAction->release();
        ivars->readAction = nullptr;
    }

    if (ivars->writeAction != nullptr) {
        ivars->writeAction->release();
        ivars->writeAction = nullptr;
    }

    // Release buffers
    if (ivars->inputBuffer != nullptr) {
        ivars->inputBuffer->release();
        ivars->inputBuffer = nullptr;
    }

    if (ivars->outputBuffer != nullptr) {
        ivars->outputBuffer->release();
        ivars->outputBuffer = nullptr;
    }

    if (ivars->hidDescriptor != nullptr) {
        ivars->hidDescriptor->release();
        ivars->hidDescriptor = nullptr;
    }

    // Release pipes
    if (ivars->inputPipe != nullptr) {
        ivars->inputPipe->release();
        ivars->inputPipe = nullptr;
    }

    if (ivars->outputPipe != nullptr) {
        ivars->outputPipe->release();
        ivars->outputPipe = nullptr;
    }

    // Close and release interface
    if (ivars->interface != nullptr) {
        ivars->interface->Close(this, 0);
        ivars->interface->release();
        ivars->interface = nullptr;
    }

    LOG_DEBUG("Resources cleaned up");
}

// =============================================================================
// MARK: - Driver Metadata
// =============================================================================

IMPL_SIMPLE_DRIVER_START(BigbenUSBDriver)
