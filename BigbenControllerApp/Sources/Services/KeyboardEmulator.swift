//
//  KeyboardEmulator.swift
//  BigbenController
//
//  Maps controller inputs to keyboard/mouse events
//  This allows the controller to work with any game that supports keyboard
//

import Foundation
import CoreGraphics
import AppKit
import CoreVideo

// MARK: - Key Mapping Configuration

struct KeyMapping: Codable, Equatable {
    var buttonA: UInt16 = CGKeyCode.space          // Space (Jump)
    var buttonB: UInt16 = CGKeyCode.keyC           // C (Crouch)
    var buttonX: UInt16 = CGKeyCode.keyR           // R (Reload)
    var buttonY: UInt16 = CGKeyCode.keyF           // F (Use)
    var buttonLB: UInt16 = CGKeyCode.keyQ          // Q (Lean Left)
    var buttonRB: UInt16 = CGKeyCode.keyE          // E (Lean Right)
    var buttonLT: UInt16 = CGKeyCode.mouseLeft     // Left Click (ADS)
    var buttonRT: UInt16 = CGKeyCode.mouseRight    // Right Click (Fire)
    var buttonBack: UInt16 = CGKeyCode.tab         // Tab (Scoreboard)
    var buttonStart: UInt16 = CGKeyCode.escape     // Escape (Menu)
    var buttonL3: UInt16 = CGKeyCode.leftShift     // Shift (Sprint)
    var buttonR3: UInt16 = CGKeyCode.keyV          // V (Melee)
    var buttonHome: UInt16 = 0                     // Disabled

    var dpadUp: UInt16 = CGKeyCode.upArrow         // Up Arrow
    var dpadDown: UInt16 = CGKeyCode.downArrow     // Down Arrow
    var dpadLeft: UInt16 = CGKeyCode.leftArrow     // Left Arrow
    var dpadRight: UInt16 = CGKeyCode.rightArrow   // Right Arrow

    // Movement keys
    var moveForward: UInt16 = CGKeyCode.keyW
    var moveBackward: UInt16 = CGKeyCode.keyS
    var moveLeft: UInt16 = CGKeyCode.keyA
    var moveRight: UInt16 = CGKeyCode.keyD

    // Mouse sensitivity (pixels per full stick deflection per update)
    var mouseSensitivity: Double = 25.0            // Reduced from 35.0 for better control with new response curve
    var mouseDeadzone: Double = 0.15               // Inner deadzone to ignore stick drift (reduced from 0.18)
    var mouseOuterDeadzone: Double = 0.95          // Outer deadzone to prevent edge instability
    var mouseAcceleration: Double = 2.0            // Legacy: kept for compatibility, multi-stage curve used instead

    static let `default` = KeyMapping()

    static let fpsPreset = KeyMapping()

    static let racingPreset: KeyMapping = {
        var m = KeyMapping()
        m.buttonRT = CGKeyCode.keyW      // Accelerate
        m.buttonLT = CGKeyCode.keyS      // Brake
        m.buttonA = CGKeyCode.space      // Handbrake
        m.buttonX = CGKeyCode.keyN       // Nitro
        m.buttonB = CGKeyCode.keyR       // Reset
        return m
    }()
}

// MARK: - CGKeyCode Extension

extension CGKeyCode {
    // Common key codes
    static let keyA: UInt16 = 0x00
    static let keyS: UInt16 = 0x01
    static let keyD: UInt16 = 0x02
    static let keyF: UInt16 = 0x03
    static let keyH: UInt16 = 0x04
    static let keyG: UInt16 = 0x05
    static let keyZ: UInt16 = 0x06
    static let keyX: UInt16 = 0x07
    static let keyC: UInt16 = 0x08
    static let keyV: UInt16 = 0x09
    static let keyN: UInt16 = 0x2D
    static let keyB: UInt16 = 0x0B
    static let keyQ: UInt16 = 0x0C
    static let keyW: UInt16 = 0x0D
    static let keyE: UInt16 = 0x0E
    static let keyR: UInt16 = 0x0F
    static let keyY: UInt16 = 0x10
    static let keyT: UInt16 = 0x11
    static let key1: UInt16 = 0x12
    static let key2: UInt16 = 0x13
    static let key3: UInt16 = 0x14
    static let key4: UInt16 = 0x15
    static let key5: UInt16 = 0x17
    static let key6: UInt16 = 0x16
    static let key7: UInt16 = 0x1A
    static let key8: UInt16 = 0x1C
    static let key9: UInt16 = 0x19
    static let key0: UInt16 = 0x1D
    static let space: UInt16 = 0x31
    static let escape: UInt16 = 0x35
    static let tab: UInt16 = 0x30
    static let leftShift: UInt16 = 0x38
    static let leftControl: UInt16 = 0x3B
    static let leftAlt: UInt16 = 0x3A

    // Arrow keys
    static let upArrow: UInt16 = 0x7E
    static let downArrow: UInt16 = 0x7D
    static let leftArrow: UInt16 = 0x7B
    static let rightArrow: UInt16 = 0x7C

    // Special codes for mouse buttons (handled separately)
    static let mouseLeft: UInt16 = 0xF0
    static let mouseRight: UInt16 = 0xF1
    static let mouseMiddle: UInt16 = 0xF2
}

// MARK: - Keyboard Emulator

class KeyboardEmulator {

    var mapping = KeyMapping.default
    var isEnabled = true

    private var pressedKeys = Set<UInt16>()
    private var mouseButtonsPressed = Set<UInt16>()
    private var lastState = ControllerState()

    // VSync-aligned mouse movement via CVDisplayLink
    private var displayLink: CVDisplayLink?
    private var pendingDeltaX: Double = 0
    private var pendingDeltaY: Double = 0
    private let mouseLock = NSLock()

    // Smoothing disabled for immediate response (was 0.3, now 0.0)
    private let smoothingFactor: Double = 0.0

    // MARK: - Initialization

    init() {
        setupDisplayLink()
    }

    private func setupDisplayLink() {
        var displayLinkRef: CVDisplayLink?
        let status = CVDisplayLinkCreateWithActiveCGDisplays(&displayLinkRef)

        guard status == kCVReturnSuccess, let link = displayLinkRef else {
            print("Failed to create CVDisplayLink, falling back to immediate updates")
            return
        }

        displayLink = link

        // Set up the callback
        let callback: CVDisplayLinkOutputCallback = { (displayLink, inNow, inOutputTime, flagsIn, flagsOut, context) -> CVReturn in
            guard let context = context else { return kCVReturnSuccess }
            let emulator = Unmanaged<KeyboardEmulator>.fromOpaque(context).takeUnretainedValue()
            emulator.flushMouseMovement()
            return kCVReturnSuccess
        }

        let selfPtr = Unmanaged.passUnretained(self).toOpaque()
        CVDisplayLinkSetOutputCallback(link, callback, selfPtr)
        CVDisplayLinkStart(link)
    }

    // Called at VSync rate to flush accumulated mouse movement
    private func flushMouseMovement() {
        mouseLock.lock()
        let deltaX = pendingDeltaX
        let deltaY = pendingDeltaY
        pendingDeltaX = 0
        pendingDeltaY = 0
        mouseLock.unlock()

        guard abs(deltaX) > 0.1 || abs(deltaY) > 0.1 else { return }

        // Use delta-based mouse movement via CGEventPost for smooth, low-latency movement
        postMouseMovement(deltaX: deltaX, deltaY: deltaY)
    }

    // Post mouse movement using CGEvent with delta values (much smoother than CGWarpMouseCursorPosition)
    private func postMouseMovement(deltaX: Double, deltaY: Double) {
        // Get current mouse position for the event
        let mouseLocation = NSEvent.mouseLocation
        let screenHeight = NSScreen.main?.frame.height ?? 1080
        let point = CGPoint(x: mouseLocation.x, y: screenHeight - mouseLocation.y)

        // Create mouse moved event with delta values
        guard let event = CGEvent(mouseEventSource: nil, mouseType: .mouseMoved, mouseCursorPosition: point, mouseButton: .left) else {
            return
        }

        // Set the delta values - this is what makes movement smooth like a real mouse
        event.setIntegerValueField(.mouseEventDeltaX, value: Int64(deltaX.rounded()))
        event.setIntegerValueField(.mouseEventDeltaY, value: Int64(deltaY.rounded()))

        // Post to HID event tap for lowest latency
        event.post(tap: .cghidEventTap)
    }

    // MARK: - Process Controller State

    func processState(_ state: ControllerState) {
        guard isEnabled else { return }

        // Handle buttons
        processButton(state.buttonA, lastState.buttonA, mapping.buttonA)
        processButton(state.buttonB, lastState.buttonB, mapping.buttonB)
        processButton(state.buttonX, lastState.buttonX, mapping.buttonX)
        processButton(state.buttonY, lastState.buttonY, mapping.buttonY)
        processButton(state.buttonLB, lastState.buttonLB, mapping.buttonLB)
        processButton(state.buttonRB, lastState.buttonRB, mapping.buttonRB)
        processButton(state.buttonLT, lastState.buttonLT, mapping.buttonLT)
        processButton(state.buttonRT, lastState.buttonRT, mapping.buttonRT)
        processButton(state.buttonBack, lastState.buttonBack, mapping.buttonBack)
        processButton(state.buttonStart, lastState.buttonStart, mapping.buttonStart)
        processButton(state.buttonL3, lastState.buttonL3, mapping.buttonL3)
        processButton(state.buttonR3, lastState.buttonR3, mapping.buttonR3)
        processButton(state.buttonHome, lastState.buttonHome, mapping.buttonHome)

        // Handle D-pad
        processButton(state.dpadUp, lastState.dpadUp, mapping.dpadUp)
        processButton(state.dpadDown, lastState.dpadDown, mapping.dpadDown)
        processButton(state.dpadLeft, lastState.dpadLeft, mapping.dpadLeft)
        processButton(state.dpadRight, lastState.dpadRight, mapping.dpadRight)

        // Handle left stick (WASD movement)
        processLeftStick(state)

        // Handle right stick (mouse look)
        processRightStick(state)

        lastState = state
    }

    // MARK: - Button Processing

    private func processButton(_ current: Bool, _ previous: Bool, _ keyCode: UInt16) {
        guard keyCode != 0 else { return }

        if current && !previous {
            pressKey(keyCode)
        } else if !current && previous {
            releaseKey(keyCode)
        }
    }

    private func pressKey(_ keyCode: UInt16) {
        // Handle mouse buttons separately
        if keyCode >= CGKeyCode.mouseLeft {
            pressMouseButton(keyCode)
            return
        }

        guard !pressedKeys.contains(keyCode) else { return }
        pressedKeys.insert(keyCode)

        if let event = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: true) {
            event.post(tap: .cghidEventTap)
        }
    }

    private func releaseKey(_ keyCode: UInt16) {
        // Handle mouse buttons separately
        if keyCode >= CGKeyCode.mouseLeft {
            releaseMouseButton(keyCode)
            return
        }

        guard pressedKeys.contains(keyCode) else { return }
        pressedKeys.remove(keyCode)

        if let event = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: false) {
            event.post(tap: .cghidEventTap)
        }
    }

    // MARK: - Mouse Button Processing

    private func pressMouseButton(_ button: UInt16) {
        guard !mouseButtonsPressed.contains(button) else { return }
        mouseButtonsPressed.insert(button)

        let mouseLocation = NSEvent.mouseLocation
        let screenHeight = NSScreen.main?.frame.height ?? 1080
        let point = CGPoint(x: mouseLocation.x, y: screenHeight - mouseLocation.y)

        let buttonType: CGMouseButton
        let eventType: CGEventType

        switch button {
        case CGKeyCode.mouseLeft:
            buttonType = .left
            eventType = .leftMouseDown
        case CGKeyCode.mouseRight:
            buttonType = .right
            eventType = .rightMouseDown
        case CGKeyCode.mouseMiddle:
            buttonType = .center
            eventType = .otherMouseDown
        default:
            return
        }

        if let event = CGEvent(mouseEventSource: nil, mouseType: eventType, mouseCursorPosition: point, mouseButton: buttonType) {
            event.post(tap: .cghidEventTap)
        }
    }

    private func releaseMouseButton(_ button: UInt16) {
        guard mouseButtonsPressed.contains(button) else { return }
        mouseButtonsPressed.remove(button)

        let mouseLocation = NSEvent.mouseLocation
        let screenHeight = NSScreen.main?.frame.height ?? 1080
        let point = CGPoint(x: mouseLocation.x, y: screenHeight - mouseLocation.y)

        let buttonType: CGMouseButton
        let eventType: CGEventType

        switch button {
        case CGKeyCode.mouseLeft:
            buttonType = .left
            eventType = .leftMouseUp
        case CGKeyCode.mouseRight:
            buttonType = .right
            eventType = .rightMouseUp
        case CGKeyCode.mouseMiddle:
            buttonType = .center
            eventType = .otherMouseUp
        default:
            return
        }

        if let event = CGEvent(mouseEventSource: nil, mouseType: eventType, mouseCursorPosition: point, mouseButton: buttonType) {
            event.post(tap: .cghidEventTap)
        }
    }

    // MARK: - Left Stick (Movement)

    private func processLeftStick(_ state: ControllerState) {
        let x = normalizeAxis(state.leftStickX)
        let y = normalizeAxis(state.leftStickY)

        // Forward/backward
        if y < -mapping.mouseDeadzone {
            pressKey(mapping.moveForward)
        } else {
            releaseKey(mapping.moveForward)
        }

        if y > mapping.mouseDeadzone {
            pressKey(mapping.moveBackward)
        } else {
            releaseKey(mapping.moveBackward)
        }

        // Left/right
        if x < -mapping.mouseDeadzone {
            pressKey(mapping.moveLeft)
        } else {
            releaseKey(mapping.moveLeft)
        }

        if x > mapping.mouseDeadzone {
            pressKey(mapping.moveRight)
        } else {
            releaseKey(mapping.moveRight)
        }
    }

    // MARK: - Right Stick (Mouse Look)

    private func processRightStick(_ state: ControllerState) {
        let x = normalizeAxis(state.rightStickX)
        let y = normalizeAxis(state.rightStickY)

        // Apply dual deadzone (inner and outer)
        let adjustedX = applyDualDeadzone(x, inner: mapping.mouseDeadzone, outer: mapping.mouseOuterDeadzone)
        var adjustedY = applyDualDeadzone(y, inner: mapping.mouseDeadzone, outer: mapping.mouseOuterDeadzone)

        // INVERT Y AXIS (fix for inverted camera)
        adjustedY = -adjustedY

        // Apply multi-stage response curve for better control
        // 0-40%: Precision zone (power 0.7) - fine aiming
        // 40-80%: Linear zone (power 1.0) - consistent movement
        // 80-100%: Acceleration zone (power 1.3) - fast turns
        let curvedX = applyMultiStageAcceleration(adjustedX)
        let curvedY = applyMultiStageAcceleration(adjustedY)

        // Calculate target mouse movement
        let targetX = curvedX * mapping.mouseSensitivity
        let targetY = curvedY * mapping.mouseSensitivity

        // Skip if no movement
        guard abs(targetX) > 0.1 || abs(targetY) > 0.1 else { return }

        // Accumulate delta for VSync-aligned updates
        if displayLink != nil {
            mouseLock.lock()
            pendingDeltaX += targetX
            pendingDeltaY += targetY
            mouseLock.unlock()
        } else {
            // Fallback: immediate update if CVDisplayLink not available
            postMouseMovement(deltaX: targetX, deltaY: targetY)
        }
    }

    // Multi-stage response curve for natural joystick-to-mouse feel
    // Zone 1 (0-40%): Precision - power 0.7 for fine aiming
    // Zone 2 (40-80%): Linear - power 1.0 for consistent tracking
    // Zone 3 (80-100%): Acceleration - power 1.3 for fast turns
    private func applyMultiStageAcceleration(_ value: Double) -> Double {
        let sign = value >= 0 ? 1.0 : -1.0
        let magnitude = abs(value)

        let result: Double
        if magnitude <= 0.4 {
            // Precision zone: power 0.7 for fine control
            // Normalize to 0-1 within this zone, apply curve, scale back
            let normalized = magnitude / 0.4
            result = pow(normalized, 0.7) * 0.4
        } else if magnitude <= 0.8 {
            // Linear zone: power 1.0 (no curve)
            // Start from where precision zone ended (0.4)
            let normalized = (magnitude - 0.4) / 0.4
            result = 0.4 + normalized * 0.4
        } else {
            // Acceleration zone: power 1.3 for fast turns
            // Start from where linear zone ended (0.8)
            let normalized = (magnitude - 0.8) / 0.2
            result = 0.8 + pow(normalized, 1.3) * 0.2
        }

        return sign * result
    }

    // MARK: - Helpers

    private func normalizeAxis(_ value: UInt8) -> Double {
        return (Double(value) - 128.0) / 128.0
    }

    // Dual deadzone: inner removes drift, outer prevents edge instability
    private func applyDualDeadzone(_ value: Double, inner: Double, outer: Double) -> Double {
        let magnitude = abs(value)

        // Inner deadzone: eliminate stick drift
        if magnitude < inner {
            return 0
        }

        // Outer deadzone: clamp to max at threshold to prevent instability at edges
        let clampedMagnitude = min(magnitude, outer)

        // Scale to 0-1 range between inner and outer deadzone
        let sign = value > 0 ? 1.0 : -1.0
        let scaledMagnitude = (clampedMagnitude - inner) / (outer - inner)

        return sign * scaledMagnitude
    }

    // Legacy single deadzone for left stick (movement keys don't need precision)
    private func applyDeadzone(_ value: Double, _ deadzone: Double) -> Double {
        if abs(value) < deadzone {
            return 0
        }
        let sign = value > 0 ? 1.0 : -1.0
        return sign * (abs(value) - deadzone) / (1.0 - deadzone)
    }

    // MARK: - Cleanup

    func releaseAllKeys() {
        for keyCode in pressedKeys {
            if let event = CGEvent(keyboardEventSource: nil, virtualKey: keyCode, keyDown: false) {
                event.post(tap: .cghidEventTap)
            }
        }
        pressedKeys.removeAll()

        for button in mouseButtonsPressed {
            releaseMouseButton(button)
        }
        mouseButtonsPressed.removeAll()
    }

    deinit {
        // Stop and release CVDisplayLink
        if let link = displayLink {
            CVDisplayLinkStop(link)
        }
        displayLink = nil

        releaseAllKeys()
    }
}
