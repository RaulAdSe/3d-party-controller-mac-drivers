//
//  main.swift
//  BigbenController
//
//  Command-line controller mapper that works immediately
//  No driver installation required - uses direct USB access
//

import Foundation
import AppKit

// Helper to flush output immediately
func log(_ message: String) {
    print(message)
    fflush(stdout)
}

log("""
╔═══════════════════════════════════════════════════════════════╗
║         Bigben Controller → Keyboard/Mouse Mapper             ║
║                    for macOS                                  ║
╠═══════════════════════════════════════════════════════════════╣
║  This tool maps your Bigben/NACON controller to keyboard      ║
║  and mouse inputs so you can play any game!                   ║
╚═══════════════════════════════════════════════════════════════╝
""")

// Check for Accessibility permissions
func checkAccessibilityPermissions() -> Bool {
    let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true]
    return AXIsProcessTrustedWithOptions(options as CFDictionary)
}

log("\n🔐 Checking Accessibility permissions...")

if !checkAccessibilityPermissions() {
    log("""

    ⚠️  Accessibility permissions required!

    This app needs permission to control your keyboard and mouse.

    Please:
    1. Open System Settings → Privacy & Security → Accessibility
    2. Find this app and enable the toggle
    3. Run this app again

    """)

    // Open System Preferences
    if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility") {
        NSWorkspace.shared.open(url)
    }

    log("Press Enter after granting permission...")
    _ = readLine()

    if !AXIsProcessTrusted() {
        log("❌ Permission still not granted. Exiting.")
        exit(1)
    }
}

log("✅ Accessibility permissions granted!\n")

// Initialize components
let usbReader = USBControllerReader()
let keyboardEmulator = KeyboardEmulator()

var isRunning = true

// Debug mode - set to true to see input values
let debugMode = CommandLine.arguments.contains("--debug")
var lastDebugTime = Date.distantPast

// Controller state callback
usbReader.onStateChanged = { state in
    keyboardEmulator.processState(state)

    // Debug output
    if debugMode && Date().timeIntervalSince(lastDebugTime) > 0.1 {
        lastDebugTime = Date()
        var buttons: [String] = []
        if state.buttonA { buttons.append("A") }
        if state.buttonB { buttons.append("B") }
        if state.buttonX { buttons.append("X") }
        if state.buttonY { buttons.append("Y") }
        if state.buttonLB { buttons.append("LB") }
        if state.buttonRB { buttons.append("RB") }
        if state.buttonLT { buttons.append("LT") }
        if state.buttonRT { buttons.append("RT") }
        if state.buttonBack { buttons.append("Back") }
        if state.buttonStart { buttons.append("Start") }
        if state.buttonL3 { buttons.append("L3") }
        if state.buttonR3 { buttons.append("R3") }
        if state.buttonHome { buttons.append("Home") }
        if state.dpadUp { buttons.append("DUp") }
        if state.dpadDown { buttons.append("DDown") }
        if state.dpadLeft { buttons.append("DLeft") }
        if state.dpadRight { buttons.append("DRight") }

        let lx = Int(state.leftStickX) - 128
        let ly = Int(state.leftStickY) - 128
        let rx = Int(state.rightStickX) - 128
        let ry = Int(state.rightStickY) - 128
        let lt = state.leftTrigger
        let rt = state.rightTrigger

        let line = String(format: "L:(%+4d,%+4d) R:(%+4d,%+4d) LT:%3d RT:%3d %@",
                          lx, ly, rx, ry, lt, rt, buttons.joined(separator: " "))
        log("\r\(line)           ")
    }
}

usbReader.onConnected = {
    log("""

    🎮 Controller Connected!

    Default mapping (FPS preset):
    ┌─────────────────────────────────────────────────────────┐
    │ Left Stick  → W/A/S/D (Movement)                        │
    │ Right Stick → Mouse (Look around)                       │
    │ A (Cross)   → Space (Jump)                              │
    │ B (Circle)  → C (Crouch)                                │
    │ X (Square)  → R (Reload)                                │
    │ Y (Triangle)→ F (Use/Interact)                          │
    │ LB (L1)     → Q                                         │
    │ RB (R1)     → E                                         │
    │ LT (L2)     → Right Click (ADS)                         │
    │ RT (R2)     → Left Click (Fire)                         │
    │ L3          → Shift (Sprint)                            │
    │ R3          → V (Melee)                                 │
    │ Back/Share  → Tab (Scoreboard)                          │
    │ Start       → Escape (Menu)                             │
    │ D-Pad       → 1/2/3/4 (Weapon slots)                    │
    └─────────────────────────────────────────────────────────┘

    Press Ctrl+C to exit.

    """)
}

usbReader.onDisconnected = {
    log("\n⚠️  Controller disconnected! Waiting for reconnection...")
    keyboardEmulator.releaseAllKeys()
}

usbReader.onError = { error in
    log("❌ Error: \(error.localizedDescription)")
}

// Handle Ctrl+C
signal(SIGINT) { _ in
    log("\n\n👋 Exiting...")
    isRunning = false
}

// Start the controller reader
log("🔍 Looking for Bigben controller (VID: 0x146b)...")
log("   Supported: PC Compact (0x0603), PS4 Compact (0x0d05)\n")

usbReader.start()

// Keep running
RunLoop.main.run()
