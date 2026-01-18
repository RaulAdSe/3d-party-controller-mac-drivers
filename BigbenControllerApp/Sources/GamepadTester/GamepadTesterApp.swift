//
//  GamepadTesterApp.swift
//  GamepadTester
//
//  Visual gamepad testing panel - shows controller state in real-time
//

import SwiftUI
import AppKit
import CUSBController

@main
struct GamepadTesterApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    var body: some Scene {
        WindowGroup {
            GamepadView()
                .frame(minWidth: 700, minHeight: 500)
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Center the window
        if let window = NSApplication.shared.windows.first {
            window.center()
            window.title = "Gamepad Tester"
            window.titlebarAppearsTransparent = true
            window.styleMask.insert(.fullSizeContentView)
        }
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return true
    }
}
