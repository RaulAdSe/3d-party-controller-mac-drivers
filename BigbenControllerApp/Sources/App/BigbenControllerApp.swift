//
//  BigbenControllerApp.swift
//  BigbenControllerApp
//
//  SwiftUI App entry point for the Bigben Controller Manager
//  Provides menu bar presence and preferences window
//

import SwiftUI

@main
struct BigbenControllerApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate
    @StateObject private var controllerMonitor = ControllerMonitor()
    @StateObject private var driverManager = DriverManager()

    var body: some Scene {
        // Menu bar extra for quick access
        MenuBarExtra {
            StatusBarView(
                controllerMonitor: controllerMonitor,
                driverManager: driverManager
            )
        } label: {
            MenuBarIcon(isConnected: controllerMonitor.isConnected)
        }
        .menuBarExtraStyle(.menu)

        // Preferences window
        Settings {
            PreferencesView(
                controllerMonitor: controllerMonitor,
                driverManager: driverManager
            )
        }
    }
}

// MARK: - Menu Bar Icon

struct MenuBarIcon: View {
    let isConnected: Bool

    var body: some View {
        Image(systemName: isConnected ? "gamecontroller.fill" : "gamecontroller")
            .symbolRenderingMode(.hierarchical)
            .foregroundStyle(isConnected ? .green : .secondary)
    }
}

// MARK: - App Constants

enum AppConstants {
    static let bundleIdentifier = "com.bigben.controllerapp"
    static let driverBundleIdentifier = "com.bigben.controllerdriver"
    static let appName = "Bigben Controller"
    static let vendorID: UInt16 = 0x146b

    enum ProductIDs {
        static let pcCompact: UInt16 = 0x0603
        static let ps4Compact: UInt16 = 0x0d05
        static let ps3Minipad: UInt16 = 0x0902
    }
}
