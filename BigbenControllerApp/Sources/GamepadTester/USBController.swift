//
//  USBController.swift
//  GamepadTester
//
//  USB communication with controller using libusb via C wrapper
//

import Foundation
import CUSBController

// MARK: - Controller State

struct ControllerState: Equatable {
    var leftStickX: UInt8 = 128
    var leftStickY: UInt8 = 128
    var rightStickX: UInt8 = 128
    var rightStickY: UInt8 = 128
    var leftTrigger: UInt8 = 0
    var rightTrigger: UInt8 = 0
    var dpad: UInt8 = 8
    var buttons: UInt16 = 0

    // Button accessors
    var buttonA: Bool { (buttons & UInt16(BTN_A)) != 0 }
    var buttonB: Bool { (buttons & UInt16(BTN_B)) != 0 }
    var buttonX: Bool { (buttons & UInt16(BTN_X)) != 0 }
    var buttonY: Bool { (buttons & UInt16(BTN_Y)) != 0 }
    var buttonLB: Bool { (buttons & UInt16(BTN_LEFT_BUMPER)) != 0 }
    var buttonRB: Bool { (buttons & UInt16(BTN_RIGHT_BUMPER)) != 0 }
    var buttonLT: Bool { leftTrigger > 128 }
    var buttonRT: Bool { rightTrigger > 128 }
    var buttonBack: Bool { (buttons & UInt16(BTN_BACK)) != 0 }
    var buttonStart: Bool { (buttons & UInt16(BTN_START)) != 0 }
    var buttonL3: Bool { (buttons & UInt16(BTN_LEFT_THUMB)) != 0 }
    var buttonR3: Bool { (buttons & UInt16(BTN_RIGHT_THUMB)) != 0 }
    var buttonHome: Bool { (buttons & UInt16(BTN_GUIDE)) != 0 }

    var dpadUp: Bool { (buttons & UInt16(BTN_DPAD_UP)) != 0 }
    var dpadDown: Bool { (buttons & UInt16(BTN_DPAD_DOWN)) != 0 }
    var dpadLeft: Bool { (buttons & UInt16(BTN_DPAD_LEFT)) != 0 }
    var dpadRight: Bool { (buttons & UInt16(BTN_DPAD_RIGHT)) != 0 }

    // Normalized values (-1 to 1)
    var leftStickXNormalized: Double { (Double(leftStickX) - 128.0) / 128.0 }
    var leftStickYNormalized: Double { (Double(leftStickY) - 128.0) / 128.0 }
    var rightStickXNormalized: Double { (Double(rightStickX) - 128.0) / 128.0 }
    var rightStickYNormalized: Double { (Double(rightStickY) - 128.0) / 128.0 }
    var leftTriggerNormalized: Double { Double(leftTrigger) / 255.0 }
    var rightTriggerNormalized: Double { Double(rightTrigger) / 255.0 }

    static func from(report: BigbenInputReport) -> ControllerState {
        var state = ControllerState()
        state.leftStickX = UInt8(clamping: Int(report.left_stick_x / 256) + 128)
        state.leftStickY = UInt8(clamping: Int(report.left_stick_y / 256) + 128)
        state.rightStickX = UInt8(clamping: Int(report.right_stick_x / 256) + 128)
        state.rightStickY = UInt8(clamping: Int(report.right_stick_y / 256) + 128)
        state.leftTrigger = report.left_trigger
        state.rightTrigger = report.right_trigger
        state.buttons = report.buttons
        return state
    }
}

// MARK: - USB Controller Reader

class USBControllerReader: ObservableObject {
    @Published var state = ControllerState()
    @Published var isConnected = false
    @Published var statusMessage = "Searching for controller..."

    private var controller: OpaquePointer?
    private var readTimer: DispatchSourceTimer?
    private var isRunning = false
    private let pollQueue = DispatchQueue(label: "com.gamepad.poll", qos: .userInteractive)

    init() {
        let result = bigben_init()
        if result != 0 {
            statusMessage = "Failed to initialize USB"
        }
    }

    deinit {
        stop()
        bigben_cleanup()
    }

    func start() {
        guard !isRunning else { return }
        isRunning = true
        controller = bigben_create()
        guard controller != nil else {
            statusMessage = "Failed to create controller"
            return
        }
        tryConnect()
    }

    func stop() {
        isRunning = false
        readTimer?.cancel()
        readTimer = nil
        if let ctrl = controller {
            bigben_stop_reading(ctrl)
            bigben_close(ctrl)
            bigben_destroy(ctrl)
            controller = nil
        }
    }

    private func tryConnect() {
        guard let ctrl = controller else { return }

        let result = bigben_open(ctrl)
        if result == 0 {
            DispatchQueue.main.async { [weak self] in
                self?.isConnected = true
                self?.statusMessage = "Controller connected"
            }
            startReading()
        } else {
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
                guard let self = self, self.isRunning else { return }
                self.tryConnect()
            }
        }
    }

    private func startReading() {
        readTimer?.cancel()
        let timer = DispatchSource.makeTimerSource(queue: pollQueue)
        timer.schedule(deadline: .now(), repeating: .milliseconds(8), leeway: .milliseconds(1))
        timer.setEventHandler { [weak self] in
            self?.pollController()
        }
        timer.resume()
        readTimer = timer
    }

    private func pollController() {
        guard let ctrl = controller else { return }

        var report = BigbenInputReport()
        let result = bigben_poll(ctrl, &report, 10)

        if result == 0 {
            let newState = ControllerState.from(report: report)
            DispatchQueue.main.async { [weak self] in
                self?.state = newState
            }
        } else if result == -4 {
            readTimer?.cancel()
            readTimer = nil

            DispatchQueue.main.async { [weak self] in
                self?.isConnected = false
                self?.statusMessage = "Controller disconnected"
                self?.state = ControllerState()
            }

            if let ctrl = controller {
                bigben_close(ctrl)
            }

            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
                guard let self = self, self.isRunning else { return }
                self.tryConnect()
            }
        }
    }
}
