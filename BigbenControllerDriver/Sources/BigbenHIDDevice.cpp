//
//  BigbenHIDDevice.cpp
//  BigbenControllerDriver
//
//  Implementation of the virtual HID gamepad device for Bigben controllers.
//  Translates proprietary input reports to standard HID format and handles
//  output reports (rumble, LEDs) by forwarding to the USB driver.
//
//  Copyright (c) 2024. Licensed under MIT License.
//

#include <os/log.h>

#include <DriverKit/IOLib.h>
#include <DriverKit/IOMemoryDescriptor.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <DriverKit/OSDictionary.h>
#include <DriverKit/OSData.h>
#include <DriverKit/OSNumber.h>
#include <DriverKit/OSString.h>

#include "BigbenHIDDevice.h"
#include "InputTranslator.h"
#include "../../Shared/BigbenProtocol.h"
#include "../../Shared/HIDReportDescriptor.h"

// =============================================================================
// MARK: - Logging
// =============================================================================

#define LOG_SUBSYSTEM "com.bigben.controller.hid"

#define HIDLog(fmt, ...)     os_log(OS_LOG_DEFAULT, LOG_SUBSYSTEM ": " fmt, ##__VA_ARGS__)
#define HIDLogDebug(fmt, ...) os_log_debug(OS_LOG_DEFAULT, LOG_SUBSYSTEM ": " fmt, ##__VA_ARGS__)
#define HIDLogError(fmt, ...) os_log_error(OS_LOG_DEFAULT, LOG_SUBSYSTEM ": " fmt, ##__VA_ARGS__)

// =============================================================================
// MARK: - Class Extension Definition
// =============================================================================

struct BigbenHIDDevice_IVars
{
    BigbenUSBDriver*    usbDriver;           // Reference to USB driver for output
    IOMemoryDescriptor* reportDescriptor;    // Cached report descriptor memory
    uint8_t             currentLEDState;     // Current LED bitmask
    bool                isStarted;           // Service running state

    // Input translation instance
    InputTranslator     translator;

    // Cached last report for GET_REPORT
    BigbenHIDReport     lastReport;
    bool                hasValidReport;
};

// =============================================================================
// MARK: - Lifecycle Methods
// =============================================================================

bool BigbenHIDDevice::init()
{
    HIDLog("BigbenHIDDevice::init()");

    if (!super::init()) {
        HIDLogError("super::init() failed");
        return false;
    }

    // Allocate instance variables
    ivars = IONewZero(BigbenHIDDevice_IVars, 1);
    if (ivars == nullptr) {
        HIDLogError("Failed to allocate ivars");
        return false;
    }

    // Initialize to safe defaults
    ivars->usbDriver = nullptr;
    ivars->reportDescriptor = nullptr;
    ivars->currentLEDState = BIGBEN_LED_1; // Default: first LED on
    ivars->isStarted = false;
    ivars->hasValidReport = false;

    // Initialize last report to neutral position
    InputTranslator::initializeNeutralReport(&ivars->lastReport);

    HIDLog("BigbenHIDDevice initialized successfully");
    return true;
}

void BigbenHIDDevice::free()
{
    HIDLog("BigbenHIDDevice::free()");

    if (ivars != nullptr) {
        // Release cached report descriptor
        if (ivars->reportDescriptor != nullptr) {
            ivars->reportDescriptor->release();
            ivars->reportDescriptor = nullptr;
        }

        // Clear USB driver reference (we don't own it)
        ivars->usbDriver = nullptr;

        IOSafeDeleteNULL(ivars, BigbenHIDDevice_IVars, 1);
    }

    super::free();
}

kern_return_t BigbenHIDDevice::Start(IOService* provider)
{
    kern_return_t ret;

    HIDLog("BigbenHIDDevice::Start()");

    if (ivars == nullptr) {
        HIDLogError("ivars not initialized");
        return kIOReturnInvalid;
    }

    // Call parent Start
    ret = super::Start(provider);
    if (ret != kIOReturnSuccess) {
        HIDLogError("super::Start() failed: 0x%x", ret);
        return ret;
    }

    // Pre-create and cache the report descriptor for efficiency
    OSData* descriptorData = newReportDescriptor();
    if (descriptorData != nullptr) {
        // Create memory descriptor from data
        IOBufferMemoryDescriptor* bufferDesc = nullptr;
        ret = IOBufferMemoryDescriptor::Create(
            kIOMemoryDirectionOut,
            descriptorData->getLength(),
            0, // No alignment requirement
            &bufferDesc
        );

        if (ret == kIOReturnSuccess && bufferDesc != nullptr) {
            // Copy descriptor data to buffer
            uint64_t address = 0;
            uint64_t length = 0;
            bufferDesc->Map(0, 0, 0, 0, &address, &length);

            if (address != 0 && length >= descriptorData->getLength()) {
                memcpy((void*)address, descriptorData->getBytesNoCopy(),
                       descriptorData->getLength());
            }

            ivars->reportDescriptor = bufferDesc;
        }

        descriptorData->release();
    }

    ivars->isStarted = true;

    // Register the device with the system
    ret = RegisterService();
    if (ret != kIOReturnSuccess) {
        HIDLogError("RegisterService() failed: 0x%x", ret);
        // Continue anyway, device may still work
    }

    HIDLog("BigbenHIDDevice started successfully");
    return kIOReturnSuccess;
}

kern_return_t BigbenHIDDevice::Stop(IOService* provider)
{
    HIDLog("BigbenHIDDevice::Stop()");

    if (ivars != nullptr) {
        ivars->isStarted = false;
    }

    return super::Stop(provider);
}

// =============================================================================
// MARK: - HID Device Description
// =============================================================================

OSDictionary* BigbenHIDDevice::newDeviceDescription()
{
    HIDLog("BigbenHIDDevice::newDeviceDescription()");

    OSDictionary* dict = OSDictionary::withCapacity(8);
    if (dict == nullptr) {
        HIDLogError("Failed to create device description dictionary");
        return nullptr;
    }

    // Vendor ID - Bigben Interactive
    OSNumber* vendorID = OSNumber::withNumber(BIGBEN_VENDOR_ID, 32);
    if (vendorID != nullptr) {
        dict->setObject(kIOHIDVendorIDKey, vendorID);
        vendorID->release();
    }

    // Product ID - Default to PS4 Compact Controller
    OSNumber* productID = OSNumber::withNumber(BIGBEN_PRODUCT_PS4_COMPACT, 32);
    if (productID != nullptr) {
        dict->setObject(kIOHIDProductIDKey, productID);
        productID->release();
    }

    // Version number
    OSNumber* versionNum = OSNumber::withNumber(0x0100, 32); // v1.0
    if (versionNum != nullptr) {
        dict->setObject(kIOHIDVersionNumberKey, versionNum);
        versionNum->release();
    }

    // Product name
    OSString* productName = OSString::withCString("Bigben Interactive Controller");
    if (productName != nullptr) {
        dict->setObject(kIOHIDProductKey, productName);
        productName->release();
    }

    // Manufacturer
    OSString* manufacturer = OSString::withCString("Bigben Interactive");
    if (manufacturer != nullptr) {
        dict->setObject(kIOHIDManufacturerKey, manufacturer);
        manufacturer->release();
    }

    // Transport type
    OSString* transport = OSString::withCString("USB");
    if (transport != nullptr) {
        dict->setObject(kIOHIDTransportKey, transport);
        transport->release();
    }

    // Serial number (generated)
    OSString* serial = OSString::withCString("BIGBEN-001");
    if (serial != nullptr) {
        dict->setObject(kIOHIDSerialNumberKey, serial);
        serial->release();
    }

    // Country code (not localized)
    OSNumber* countryCode = OSNumber::withNumber(0, 32);
    if (countryCode != nullptr) {
        dict->setObject(kIOHIDCountryCodeKey, countryCode);
        countryCode->release();
    }

    // Location ID (placeholder)
    OSNumber* locationID = OSNumber::withNumber(0, 32);
    if (locationID != nullptr) {
        dict->setObject(kIOHIDLocationIDKey, locationID);
        locationID->release();
    }

    HIDLogDebug("Device description created with VID=0x%04x PID=0x%04x",
                BIGBEN_VENDOR_ID, BIGBEN_PRODUCT_PS4_COMPACT);

    return dict;
}

OSData* BigbenHIDDevice::newReportDescriptor()
{
    HIDLog("BigbenHIDDevice::newReportDescriptor()");

    // Create OSData from the static report descriptor
    OSData* data = OSData::withBytes(kBigbenHIDReportDescriptor,
                                      kBigbenHIDReportDescriptorSize);

    if (data == nullptr) {
        HIDLogError("Failed to create report descriptor data");
        return nullptr;
    }

    HIDLogDebug("Report descriptor created, size=%zu bytes", kBigbenHIDReportDescriptorSize);
    return data;
}

// =============================================================================
// MARK: - Report Handling
// =============================================================================

kern_return_t BigbenHIDDevice::getReport(IOMemoryDescriptor*      report,
                                          IOHIDReportType          reportType,
                                          IOOptionBits             options,
                                          uint32_t                 completionTimeout,
                                          OSAction*                action)
{
    HIDLogDebug("BigbenHIDDevice::getReport() type=%d", reportType);

    if (report == nullptr) {
        HIDLogError("getReport: null report descriptor");
        return kIOReturnBadArgument;
    }

    // Only support input reports
    if (reportType != kIOHIDReportTypeInput) {
        HIDLogError("getReport: unsupported report type %d", reportType);
        return kIOReturnUnsupported;
    }

    // Get the report ID from options
    uint8_t reportID = (options & 0xFF);

    // Only support report ID 1
    if (reportID != 0 && reportID != BIGBEN_REPORT_ID_INPUT) {
        HIDLogError("getReport: unsupported report ID %d", reportID);
        return kIOReturnUnsupported;
    }

    // Prepare report to return
    BigbenHIDReport reportToSend;

    if (ivars != nullptr && ivars->hasValidReport) {
        // Return cached last report
        reportToSend = ivars->lastReport;
    } else {
        // Return neutral state
        InputTranslator::initializeNeutralReport(&reportToSend);
    }

    // Map the output buffer and copy data
    uint64_t address = 0;
    uint64_t length = 0;

    kern_return_t ret = report->Map(0, 0, 0, 0, &address, &length);
    if (ret != kIOReturnSuccess) {
        HIDLogError("getReport: failed to map memory: 0x%x", ret);
        return ret;
    }

    if (length < sizeof(BigbenHIDReport)) {
        HIDLogError("getReport: buffer too small (%llu < %zu)", length, sizeof(BigbenHIDReport));
        return kIOReturnNoSpace;
    }

    // Copy report data
    memcpy((void*)address, &reportToSend, sizeof(BigbenHIDReport));

    // Complete with action if provided
    if (action != nullptr) {
        CompleteReport(action, kIOReturnSuccess, sizeof(BigbenHIDReport));
    }

    return kIOReturnSuccess;
}

kern_return_t BigbenHIDDevice::setReport(IOMemoryDescriptor*      report,
                                          IOHIDReportType          reportType,
                                          IOOptionBits             options,
                                          uint32_t                 completionTimeout,
                                          OSAction*                action)
{
    HIDLogDebug("BigbenHIDDevice::setReport() type=%d", reportType);

    if (report == nullptr) {
        HIDLogError("setReport: null report descriptor");
        return kIOReturnBadArgument;
    }

    // Only support output reports (rumble, LEDs)
    if (reportType != kIOHIDReportTypeOutput) {
        HIDLogError("setReport: unsupported report type %d", reportType);
        return kIOReturnUnsupported;
    }

    // Map the input buffer
    uint64_t address = 0;
    uint64_t length = 0;

    kern_return_t ret = report->Map(0, 0, 0, 0, &address, &length);
    if (ret != kIOReturnSuccess) {
        HIDLogError("setReport: failed to map memory: 0x%x", ret);
        return ret;
    }

    if (length < 1) {
        HIDLogError("setReport: empty report");
        return kIOReturnUnderrun;
    }

    // Parse the report based on report ID
    const uint8_t* data = (const uint8_t*)address;
    uint8_t reportID = data[0];

    switch (reportID) {
        case BIGBEN_REPORT_ID_LED: {
            // LED report - for now, just log it
            if (length >= sizeof(BigbenLEDReport)) {
                const BigbenLEDReport* ledReport = (const BigbenLEDReport*)data;
                HIDLogDebug("LED report: state=0x%02x", ledReport->ledState);

                ret = sendLEDToUSB(ledReport->ledState);
            } else {
                HIDLogError("setReport: LED report too small");
                ret = kIOReturnUnderrun;
            }
            break;
        }

        case BIGBEN_REPORT_ID_RUMBLE: {
            // Rumble report
            if (length >= sizeof(BigbenRumbleReport)) {
                const BigbenRumbleReport* rumbleReport = (const BigbenRumbleReport*)data;
                HIDLogDebug("Rumble report: left=%d right=%d",
                           rumbleReport->leftMotorForce, rumbleReport->rightMotorOn);

                ret = sendRumbleToUSB(rumbleReport->leftMotorForce,
                                      rumbleReport->rightMotorOn);
            } else {
                HIDLogError("setReport: Rumble report too small");
                ret = kIOReturnUnderrun;
            }
            break;
        }

        default:
            HIDLogError("setReport: unknown report ID %d", reportID);
            ret = kIOReturnUnsupported;
            break;
    }

    // Complete with action if provided
    if (action != nullptr) {
        CompleteReport(action, ret, (uint32_t)length);
    }

    return ret;
}

// =============================================================================
// MARK: - Input Processing
// =============================================================================

kern_return_t BigbenHIDDevice::handleInputReport(const void* inputData, uint32_t length)
{
    if (ivars == nullptr || !ivars->isStarted) {
        HIDLogError("handleInputReport: device not started");
        return kIOReturnNotReady;
    }

    if (inputData == nullptr) {
        HIDLogError("handleInputReport: null input data");
        return kIOReturnBadArgument;
    }

    if (length < sizeof(BigbenInputReport)) {
        HIDLogError("handleInputReport: input too small (%u < %zu)",
                   length, sizeof(BigbenInputReport));
        return kIOReturnUnderrun;
    }

    // Cast input data to proprietary report structure
    const BigbenInputReport* proprietaryReport = (const BigbenInputReport*)inputData;

    // Verify report ID
    if (proprietaryReport->reportId != BIGBEN_REPORT_ID_INPUT) {
        HIDLogError("handleInputReport: wrong report ID %d", proprietaryReport->reportId);
        return kIOReturnBadArgument;
    }

    // Translate to standard HID report format
    BigbenHIDReport hidReport;
    ivars->translator.translate(proprietaryReport, &hidReport);

    // Cache for GET_REPORT
    ivars->lastReport = hidReport;
    ivars->hasValidReport = true;

    // Create memory descriptor for the HID report
    IOBufferMemoryDescriptor* reportBuffer = nullptr;
    kern_return_t ret = IOBufferMemoryDescriptor::Create(
        kIOMemoryDirectionOut,
        sizeof(BigbenHIDReport),
        0,
        &reportBuffer
    );

    if (ret != kIOReturnSuccess || reportBuffer == nullptr) {
        HIDLogError("handleInputReport: failed to create buffer: 0x%x", ret);
        return ret;
    }

    // Map and copy report data
    uint64_t address = 0;
    uint64_t mappedLength = 0;
    ret = reportBuffer->Map(0, 0, 0, 0, &address, &mappedLength);

    if (ret == kIOReturnSuccess && address != 0) {
        memcpy((void*)address, &hidReport, sizeof(BigbenHIDReport));

        // Dispatch the report to macOS
        // The timestamp uses mach_absolute_time() for precision
        uint64_t timestamp = mach_absolute_time();

        ret = handleReport(timestamp,
                          reportBuffer,
                          kIOHIDReportTypeInput,
                          kIOHIDOptionsTypeNone);

        if (ret != kIOReturnSuccess) {
            HIDLogError("handleInputReport: handleReport failed: 0x%x", ret);
        }
    } else {
        HIDLogError("handleInputReport: failed to map buffer: 0x%x", ret);
    }

    // Release the buffer
    reportBuffer->release();

    return ret;
}

// =============================================================================
// MARK: - USB Driver Communication
// =============================================================================

void BigbenHIDDevice::setUSBDriver(BigbenUSBDriver* driver)
{
    HIDLog("BigbenHIDDevice::setUSBDriver(%p)", driver);

    if (ivars != nullptr) {
        ivars->usbDriver = driver;
    }
}

kern_return_t BigbenHIDDevice::sendRumbleToUSB(uint8_t leftMotor, uint8_t rightMotor)
{
    HIDLogDebug("sendRumbleToUSB: left=%d right=%d", leftMotor, rightMotor);

    if (ivars == nullptr || ivars->usbDriver == nullptr) {
        HIDLogError("sendRumbleToUSB: no USB driver");
        return kIOReturnNotReady;
    }

    // Create the rumble report for USB transmission
    BigbenRumbleReport rumbleReport = {
        .reportId = BIGBEN_REPORT_ID_RUMBLE,
        .reserved1 = 0x08,
        .rightMotorOn = (rightMotor > 0) ? 1 : 0,
        .leftMotorForce = leftMotor,
        .duration = 0xFF, // Continuous until changed
        .padding = {0, 0, 0}
    };

    // Forward to USB driver
    // Note: The actual USB driver would implement sendOutputReport()
    // For now, we just log it - Agent 1 will implement the USB side
    HIDLogDebug("Would send rumble to USB: left=%d right=%d", leftMotor, rightMotor);

    // TODO: Call usbDriver->sendOutputReport(&rumbleReport, sizeof(rumbleReport))
    // when USB driver interface is available

    return kIOReturnSuccess;
}

kern_return_t BigbenHIDDevice::sendLEDToUSB(uint8_t ledMask)
{
    HIDLogDebug("sendLEDToUSB: mask=0x%02x", ledMask);

    if (ivars == nullptr) {
        return kIOReturnNotReady;
    }

    // Update cached state
    ivars->currentLEDState = ledMask;

    if (ivars->usbDriver == nullptr) {
        HIDLogError("sendLEDToUSB: no USB driver");
        return kIOReturnNotReady;
    }

    // Create the LED report for USB transmission
    BigbenLEDReport ledReport = {
        .reportId = BIGBEN_REPORT_ID_LED,
        .reserved1 = 0x08,
        .ledState = ledMask,
        .padding = {0, 0, 0, 0, 0}
    };

    // Forward to USB driver
    HIDLogDebug("Would send LED to USB: mask=0x%02x", ledMask);

    // TODO: Call usbDriver->sendOutputReport(&ledReport, sizeof(ledReport))
    // when USB driver interface is available

    return kIOReturnSuccess;
}
