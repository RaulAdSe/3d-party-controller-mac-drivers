//
//  ControllerMonitor.swift
//  BigbenControllerApp
//
//  Service class responsible for:
//  - Using IOKit to monitor USB device connection
//  - Publishing controller state (connected/disconnected)
//  - Providing controller info (serial, firmware, etc.)
//

import Foundation
import IOKit
import IOKit.usb
import Combine
import os.log

// MARK: - Controller Info

struct ControllerInfo: Equatable {
    let vendorID: UInt16
    let productID: UInt16
    let productName: String
    let manufacturer: String
    let serialNumber: String?
    let locationID: UInt32

    var displayName: String {
        if !productName.isEmpty {
            return productName
        }
        return "Bigben Controller"
    }

    var productDescription: String {
        switch productID {
        case AppConstants.ProductIDs.pcCompact:
            return "PC Compact Controller"
        case AppConstants.ProductIDs.ps4Compact:
            return "PS4 Compact Controller"
        case AppConstants.ProductIDs.ps3Minipad:
            return "PS3 Mini Pad"
        default:
            return "Unknown Controller"
        }
    }
}

// MARK: - Controller Monitor

@MainActor
class ControllerMonitor: ObservableObject {
    // MARK: - Published Properties

    @Published private(set) var isConnected = false
    @Published private(set) var controllerInfo: ControllerInfo?
    @Published private(set) var connectionHistory: [Date] = []

    // MARK: - Private Properties

    private let logger = Logger(subsystem: AppConstants.bundleIdentifier, category: "ControllerMonitor")
    private var notificationPort: IONotificationPortRef?
    private var addedIterator: io_iterator_t = 0
    private var removedIterator: io_iterator_t = 0
    private var runLoopSource: CFRunLoopSource?

    // MARK: - Initialization

    init() {
        startMonitoring()
    }

    deinit {
        stopMonitoring()
    }

    // MARK: - Public Methods

    /// Refresh controller status
    func refresh() {
        logger.info("Refreshing controller status")
        checkCurrentlyConnectedDevices()
    }

    // MARK: - Private Methods

    private func startMonitoring() {
        logger.info("Starting USB device monitoring")

        // Create matching dictionary for Bigben vendor
        guard let matchingDict = createMatchingDictionary() else {
            logger.error("Failed to create matching dictionary")
            return
        }

        // Create notification port
        notificationPort = IONotificationPortCreate(kIOMainPortDefault)
        guard let notificationPort = notificationPort else {
            logger.error("Failed to create notification port")
            return
        }

        // Get run loop source and add to main run loop
        runLoopSource = IONotificationPortGetRunLoopSource(notificationPort).takeUnretainedValue()
        if let runLoopSource = runLoopSource {
            CFRunLoopAddSource(CFRunLoopGetMain(), runLoopSource, .defaultMode)
        }

        // Set up device added notification
        setupDeviceAddedNotification(matchingDict: matchingDict, port: notificationPort)

        // Set up device removed notification
        setupDeviceRemovedNotification(port: notificationPort)

        // Check for already connected devices
        checkCurrentlyConnectedDevices()
    }

    private func stopMonitoring() {
        logger.info("Stopping USB device monitoring")

        if addedIterator != 0 {
            IOObjectRelease(addedIterator)
            addedIterator = 0
        }

        if removedIterator != 0 {
            IOObjectRelease(removedIterator)
            removedIterator = 0
        }

        if let runLoopSource = runLoopSource {
            CFRunLoopRemoveSource(CFRunLoopGetMain(), runLoopSource, .defaultMode)
        }

        if let notificationPort = notificationPort {
            IONotificationPortDestroy(notificationPort)
        }
    }

    private func createMatchingDictionary() -> CFDictionary? {
        // Create matching dictionary for USB devices
        guard let matchingDict = IOServiceMatching(kIOUSBDeviceClassName) as? NSMutableDictionary else {
            return nil
        }

        // Match Bigben vendor ID
        matchingDict[kUSBVendorID] = AppConstants.vendorID

        return matchingDict as CFDictionary
    }

    private func setupDeviceAddedNotification(matchingDict: CFDictionary, port: IONotificationPortRef) {
        // Create a copy of the matching dictionary (it gets consumed)
        guard let matchingCopy = (matchingDict as NSDictionary).mutableCopy() as? CFDictionary else {
            logger.error("Failed to copy matching dictionary")
            return
        }

        let selfPtr = Unmanaged.passUnretained(self).toOpaque()

        let result = IOServiceAddMatchingNotification(
            port,
            kIOFirstMatchNotification,
            matchingCopy,
            deviceAddedCallback,
            selfPtr,
            &addedIterator
        )

        if result != KERN_SUCCESS {
            logger.error("Failed to add device added notification: \(result)")
            return
        }

        // Process any existing devices in the iterator
        processAddedDevices()
    }

    private func setupDeviceRemovedNotification(port: IONotificationPortRef) {
        guard let matchingDict = createMatchingDictionary() else {
            return
        }

        let selfPtr = Unmanaged.passUnretained(self).toOpaque()

        let result = IOServiceAddMatchingNotification(
            port,
            kIOTerminatedNotification,
            matchingDict,
            deviceRemovedCallback,
            selfPtr,
            &removedIterator
        )

        if result != KERN_SUCCESS {
            logger.error("Failed to add device removed notification: \(result)")
            return
        }

        // Drain the iterator
        while case let device = IOIteratorNext(removedIterator), device != 0 {
            IOObjectRelease(device)
        }
    }

    private func checkCurrentlyConnectedDevices() {
        guard let matchingDict = createMatchingDictionary() else {
            return
        }

        var iterator: io_iterator_t = 0
        let result = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &iterator)

        if result != KERN_SUCCESS {
            logger.error("Failed to get matching services: \(result)")
            return
        }

        defer { IOObjectRelease(iterator) }

        var foundDevice = false
        while case let device = IOIteratorNext(iterator), device != 0 {
            if let info = getDeviceInfo(device) {
                foundDevice = true
                Task { @MainActor in
                    self.handleDeviceConnected(info: info)
                }
            }
            IOObjectRelease(device)
        }

        if !foundDevice {
            Task { @MainActor in
                self.handleDeviceDisconnected()
            }
        }
    }

    private func processAddedDevices() {
        while case let device = IOIteratorNext(addedIterator), device != 0 {
            if let info = getDeviceInfo(device) {
                Task { @MainActor in
                    self.handleDeviceConnected(info: info)
                }
            }
            IOObjectRelease(device)
        }
    }

    private func processRemovedDevices() {
        while case let device = IOIteratorNext(removedIterator), device != 0 {
            Task { @MainActor in
                self.handleDeviceDisconnected()
            }
            IOObjectRelease(device)
        }
    }

    private func getDeviceInfo(_ device: io_service_t) -> ControllerInfo? {
        // Get vendor ID
        guard let vendorID = getDeviceProperty(device, key: kUSBVendorID) as? UInt16 else {
            return nil
        }

        // Verify it's a Bigben device
        guard vendorID == AppConstants.vendorID else {
            return nil
        }

        // Get product ID
        guard let productID = getDeviceProperty(device, key: kUSBProductID) as? UInt16 else {
            return nil
        }

        // Check if it's a supported product
        let supportedProducts: [UInt16] = [
            AppConstants.ProductIDs.pcCompact,
            AppConstants.ProductIDs.ps4Compact,
            AppConstants.ProductIDs.ps3Minipad
        ]

        guard supportedProducts.contains(productID) else {
            logger.info("Unsupported Bigben product: 0x\(String(productID, radix: 16))")
            return nil
        }

        // Get product name
        let productName = getDeviceProperty(device, key: kUSBProductString) as? String ?? ""

        // Get manufacturer
        let manufacturer = getDeviceProperty(device, key: kUSBVendorString) as? String ?? "Bigben Interactive"

        // Get serial number (optional)
        let serialNumber = getDeviceProperty(device, key: kUSBSerialNumberString) as? String

        // Get location ID
        let locationID = getDeviceProperty(device, key: kUSBDevicePropertyLocationID) as? UInt32 ?? 0

        return ControllerInfo(
            vendorID: vendorID,
            productID: productID,
            productName: productName,
            manufacturer: manufacturer,
            serialNumber: serialNumber,
            locationID: locationID
        )
    }

    private func getDeviceProperty(_ device: io_service_t, key: String) -> Any? {
        let cfKey = key as CFString
        guard let value = IORegistryEntryCreateCFProperty(device, cfKey, kCFAllocatorDefault, 0) else {
            return nil
        }
        return value.takeRetainedValue()
    }

    private func handleDeviceConnected(info: ControllerInfo) {
        logger.info("Controller connected: \(info.displayName)")

        isConnected = true
        controllerInfo = info
        connectionHistory.append(Date())

        // Keep only last 10 connection events
        if connectionHistory.count > 10 {
            connectionHistory.removeFirst()
        }

        // Post notification
        NotificationCenter.default.post(
            name: .controllerConnected,
            object: nil,
            userInfo: ["info": info]
        )
    }

    private func handleDeviceDisconnected() {
        logger.info("Controller disconnected")

        isConnected = false
        controllerInfo = nil

        // Post notification
        NotificationCenter.default.post(
            name: .controllerDisconnected,
            object: nil
        )
    }
}

// MARK: - IOKit Callbacks

private func deviceAddedCallback(
    refCon: UnsafeMutableRawPointer?,
    iterator: io_iterator_t
) {
    guard let refCon = refCon else { return }
    let monitor = Unmanaged<ControllerMonitor>.fromOpaque(refCon).takeUnretainedValue()

    Task { @MainActor in
        monitor.processAddedDevices()
    }
}

private func deviceRemovedCallback(
    refCon: UnsafeMutableRawPointer?,
    iterator: io_iterator_t
) {
    guard let refCon = refCon else { return }
    let monitor = Unmanaged<ControllerMonitor>.fromOpaque(refCon).takeUnretainedValue()

    Task { @MainActor in
        monitor.processRemovedDevices()
    }
}

// MARK: - IOKit Constants

private let kUSBVendorID = "idVendor"
private let kUSBProductID = "idProduct"
private let kUSBProductString = "USB Product Name"
private let kUSBVendorString = "USB Vendor Name"
private let kUSBSerialNumberString = "USB Serial Number"
private let kUSBDevicePropertyLocationID = "locationID"

// MARK: - Notification Names

extension Notification.Name {
    static let controllerConnected = Notification.Name("com.bigben.controllerConnected")
    static let controllerDisconnected = Notification.Name("com.bigben.controllerDisconnected")
}

// MARK: - ControllerMonitor Extensions

extension ControllerMonitor {
    /// Make processAddedDevices accessible for callbacks
    func processAddedDevices() {
        while case let device = IOIteratorNext(addedIterator), device != 0 {
            if let info = getDeviceInfo(device) {
                handleDeviceConnected(info: info)
            }
            IOObjectRelease(device)
        }
    }

    /// Make processRemovedDevices accessible for callbacks
    func processRemovedDevices() {
        while case let device = IOIteratorNext(removedIterator), device != 0 {
            handleDeviceDisconnected()
            IOObjectRelease(device)
        }
    }
}
