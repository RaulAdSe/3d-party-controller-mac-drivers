//
//  USBController.swift
//  BigbenController
//
//  USB communication with Bigben controller using libusb via C wrapper
//

import Foundation
import CUSBController

// MARK: - Controller State (matching KeyboardEmulator expectations)

struct ControllerState: Equatable {
    var leftStickX: UInt8 = 128
    var leftStickY: UInt8 = 128
    var rightStickX: UInt8 = 128
    var rightStickY: UInt8 = 128
    var leftTrigger: UInt8 = 0
    var rightTrigger: UInt8 = 0
    var dpad: UInt8 = 8  // Neutral (no direction)
    var buttons: UInt16 = 0

    // Button accessors (matching XInput mapping)
    var buttonA: Bool { (buttons & UInt16(BTN_A)) != 0 }
    var buttonB: Bool { (buttons & UInt16(BTN_B)) != 0 }
    var buttonX: Bool { (buttons & UInt16(BTN_X)) != 0 }
    var buttonY: Bool { (buttons & UInt16(BTN_Y)) != 0 }
    var buttonLB: Bool { (buttons & UInt16(BTN_LEFT_BUMPER)) != 0 }
    var buttonRB: Bool { (buttons & UInt16(BTN_RIGHT_BUMPER)) != 0 }
    var buttonLT: Bool { leftTrigger > 128 }  // Analog trigger
    var buttonRT: Bool { rightTrigger > 128 }  // Analog trigger
    var buttonBack: Bool { (buttons & UInt16(BTN_BACK)) != 0 }
    var buttonStart: Bool { (buttons & UInt16(BTN_START)) != 0 }
    var buttonL3: Bool { (buttons & UInt16(BTN_LEFT_THUMB)) != 0 }
    var buttonR3: Bool { (buttons & UInt16(BTN_RIGHT_THUMB)) != 0 }
    var buttonHome: Bool { (buttons & UInt16(BTN_GUIDE)) != 0 }

    // D-pad directions (XInput uses discrete bits)
    var dpadUp: Bool { (buttons & UInt16(BTN_DPAD_UP)) != 0 }
    var dpadDown: Bool { (buttons & UInt16(BTN_DPAD_DOWN)) != 0 }
    var dpadLeft: Bool { (buttons & UInt16(BTN_DPAD_LEFT)) != 0 }
    var dpadRight: Bool { (buttons & UInt16(BTN_DPAD_RIGHT)) != 0 }

    // Convert from XInput report
    static func from(report: BigbenInputReport) -> ControllerState {
        var state = ControllerState()

        // XInput sticks are signed 16-bit, convert to unsigned 8-bit (0-255)
        state.leftStickX = UInt8(clamping: Int(report.left_stick_x / 256) + 128)
        state.leftStickY = UInt8(clamping: Int(report.left_stick_y / 256) + 128)
        state.rightStickX = UInt8(clamping: Int(report.right_stick_x / 256) + 128)
        state.rightStickY = UInt8(clamping: Int(report.right_stick_y / 256) + 128)

        state.leftTrigger = report.left_trigger
        state.rightTrigger = report.right_trigger
        state.buttons = report.buttons

        // Convert button d-pad to numeric (for backward compat, though we use button flags now)
        var dpadValue: UInt8 = 8  // Neutral
        let up = (report.buttons & UInt16(BTN_DPAD_UP)) != 0
        let down = (report.buttons & UInt16(BTN_DPAD_DOWN)) != 0
        let left = (report.buttons & UInt16(BTN_DPAD_LEFT)) != 0
        let right = (report.buttons & UInt16(BTN_DPAD_RIGHT)) != 0

        if up && !down && !left && !right { dpadValue = 0 }      // N
        else if up && right { dpadValue = 1 }                      // NE
        else if right && !up && !down { dpadValue = 2 }           // E
        else if down && right { dpadValue = 3 }                    // SE
        else if down && !left && !right { dpadValue = 4 }         // S
        else if down && left { dpadValue = 5 }                     // SW
        else if left && !up && !down { dpadValue = 6 }            // W
        else if up && left { dpadValue = 7 }                       // NW

        state.dpad = dpadValue

        return state
    }
}

// MARK: - USB Controller Reader

class USBControllerReader {

    var onStateChanged: ((ControllerState) -> Void)?
    var onError: ((Error) -> Void)?
    var onConnected: (() -> Void)?
    var onDisconnected: (() -> Void)?

    private var controller: OpaquePointer?
    private var readTimer: DispatchSourceTimer?
    private var currentState = ControllerState()
    private var isRunning = false
    private let pollQueue = DispatchQueue(label: "com.bigben.poll", qos: .userInteractive)

    init() {
        // Initialize libusb
        let result = bigben_init()
        if result != 0 {
            print("Warning: Failed to initialize libusb")
        }
    }

    deinit {
        stop()
        bigben_cleanup()
    }

    // MARK: - Public Methods

    func start() {
        guard !isRunning else { return }
        isRunning = true

        // Create controller instance
        controller = bigben_create()
        guard controller != nil else {
            print("Failed to create controller instance")
            return
        }

        // Try to open connection
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

    func sendRumble(weakMotor: UInt8, strongMotor: UInt8) {
        guard let ctrl = controller, bigben_is_connected(ctrl) else { return }
        bigben_set_rumble(ctrl, weakMotor, strongMotor)
    }

    // MARK: - Private Methods

    private func tryConnect() {
        guard let ctrl = controller else { return }

        // Try to open the controller
        let result = bigben_open(ctrl)
        if result == 0 {
            print("Controller opened successfully")
            DispatchQueue.main.async { [weak self] in
                self?.onConnected?()
            }
            startReading()
        } else {
            // Schedule retry
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
                guard let self = self, self.isRunning else { return }
                self.tryConnect()
            }
        }
    }

    private func startReading() {
        // Poll for input at high frequency for smooth mouse movement
        // 500Hz = 2ms intervals for very responsive camera control
        readTimer?.cancel()

        let timer = DispatchSource.makeTimerSource(queue: pollQueue)
        timer.schedule(deadline: .now(), repeating: .milliseconds(2), leeway: .microseconds(100))
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

            if newState != currentState {
                currentState = newState
                onStateChanged?(newState)
            }
        } else if result == -4 { // LIBUSB_ERROR_NO_DEVICE
            // Device disconnected
            readTimer?.cancel()
            readTimer = nil

            DispatchQueue.main.async { [weak self] in
                self?.onDisconnected?()
            }

            // Try to reconnect
            if let ctrl = controller {
                bigben_close(ctrl)
            }

            DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in
                guard let self = self, self.isRunning else { return }
                self.tryConnect()
            }
        }
        // Ignore timeout errors (result == -7), they're normal
    }
}
