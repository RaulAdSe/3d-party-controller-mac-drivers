//
//  PreferencesView.swift
//  BigbenControllerApp
//
//  Preferences window with:
//  - Controller status section
//  - Button remapping UI (future)
//  - LED control (future)
//  - About section
//

import SwiftUI

struct PreferencesView: View {
    @ObservedObject var controllerMonitor: ControllerMonitor
    @ObservedObject var driverManager: DriverManager

    var body: some View {
        TabView {
            GeneralTab(
                controllerMonitor: controllerMonitor,
                driverManager: driverManager
            )
            .tabItem {
                Label("General", systemImage: "gearshape")
            }

            ControllerTab(controllerMonitor: controllerMonitor)
                .tabItem {
                    Label("Controller", systemImage: "gamecontroller")
                }

            ButtonMappingTab()
                .tabItem {
                    Label("Buttons", systemImage: "rectangle.grid.3x2")
                }

            LEDControlTab()
                .tabItem {
                    Label("LEDs", systemImage: "lightbulb")
                }

            AboutTab()
                .tabItem {
                    Label("About", systemImage: "info.circle")
                }
        }
        .frame(width: 500, height: 400)
    }
}

// MARK: - General Tab

private struct GeneralTab: View {
    @ObservedObject var controllerMonitor: ControllerMonitor
    @ObservedObject var driverManager: DriverManager
    @AppStorage("launchAtLogin") private var launchAtLogin = false
    @AppStorage("showInDock") private var showInDock = false

    var body: some View {
        Form {
            Section {
                driverStatusView
            } header: {
                Text("Driver Status")
            }

            Section {
                Toggle("Launch at Login", isOn: $launchAtLogin)
                Toggle("Show in Dock", isOn: $showInDock)
                    .onChange(of: showInDock) { _, newValue in
                        updateDockVisibility(newValue)
                    }
            } header: {
                Text("Startup")
            }

            Section {
                connectionStatusView
            } header: {
                Text("Connection Status")
            }
        }
        .formStyle(.grouped)
        .padding()
    }

    private var driverStatusView: some View {
        VStack(alignment: .leading, spacing: 12) {
            HStack {
                Image(systemName: driverManager.state.symbolName)
                    .foregroundStyle(driverStatusColor)
                    .font(.title2)

                VStack(alignment: .leading) {
                    Text(driverManager.state.description)
                        .font(.headline)
                    Text("Bigben Controller Driver")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                Spacer()

                driverActionButton
            }

            if driverManager.state == .needsApproval {
                HStack {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundStyle(.orange)
                    Text("The driver requires approval in System Settings.")
                        .font(.caption)
                }
                .padding(8)
                .background(Color.orange.opacity(0.1))
                .cornerRadius(8)
            }

            if driverManager.state == .needsReboot {
                HStack {
                    Image(systemName: "arrow.clockwise.circle.fill")
                        .foregroundStyle(.purple)
                    Text("Please restart your Mac to complete installation.")
                        .font(.caption)
                }
                .padding(8)
                .background(Color.purple.opacity(0.1))
                .cornerRadius(8)
            }
        }
    }

    private var driverStatusColor: Color {
        switch driverManager.state {
        case .installed:
            return .green
        case .needsApproval, .needsReboot:
            return .orange
        case .error:
            return .red
        case .installing:
            return .blue
        default:
            return .secondary
        }
    }

    @ViewBuilder
    private var driverActionButton: some View {
        switch driverManager.state {
        case .notInstalled:
            Button("Install") {
                driverManager.installDriver()
            }
            .disabled(driverManager.isProcessing)

        case .needsApproval:
            Button("Open Settings") {
                driverManager.openApprovalSettings()
            }

        case .installed:
            Button("Reinstall") {
                driverManager.installDriver()
            }
            .disabled(driverManager.isProcessing)

        case .installing:
            ProgressView()
                .scaleEffect(0.7)

        default:
            EmptyView()
        }
    }

    private var connectionStatusView: some View {
        HStack {
            Circle()
                .fill(controllerMonitor.isConnected ? Color.green : Color.red)
                .frame(width: 10, height: 10)

            Text(controllerMonitor.isConnected ? "Controller Connected" : "No Controller Detected")

            Spacer()

            Button("Refresh") {
                controllerMonitor.refresh()
            }
        }
    }

    private func updateDockVisibility(_ show: Bool) {
        NSApp.setActivationPolicy(show ? .regular : .accessory)
    }
}

// MARK: - Controller Tab

private struct ControllerTab: View {
    @ObservedObject var controllerMonitor: ControllerMonitor

    var body: some View {
        Form {
            if let info = controllerMonitor.controllerInfo {
                Section {
                    LabeledContent("Product", value: info.productDescription)
                    LabeledContent("Manufacturer", value: info.manufacturer)
                    LabeledContent("Vendor ID", value: String(format: "0x%04X", info.vendorID))
                    LabeledContent("Product ID", value: String(format: "0x%04X", info.productID))

                    if let serial = info.serialNumber {
                        LabeledContent("Serial Number", value: serial)
                    }

                    LabeledContent("Location ID", value: String(format: "0x%08X", info.locationID))
                } header: {
                    Text("Controller Information")
                }

                Section {
                    controllerDiagram
                } header: {
                    Text("Controller Layout")
                }
            } else {
                ContentUnavailableView(
                    "No Controller Connected",
                    systemImage: "gamecontroller",
                    description: Text("Connect a Bigben controller to see its details.")
                )
            }
        }
        .formStyle(.grouped)
        .padding()
    }

    private var controllerDiagram: some View {
        VStack(spacing: 20) {
            HStack(spacing: 60) {
                // Left side
                VStack {
                    Text("LB")
                        .font(.caption)
                        .padding(4)
                        .background(Color.secondary.opacity(0.2))
                        .cornerRadius(4)

                    Text("LT")
                        .font(.caption)
                        .padding(4)
                        .background(Color.secondary.opacity(0.2))
                        .cornerRadius(4)
                }

                // Center
                Image(systemName: "gamecontroller.fill")
                    .font(.system(size: 60))
                    .foregroundStyle(.secondary)

                // Right side
                VStack {
                    Text("RB")
                        .font(.caption)
                        .padding(4)
                        .background(Color.secondary.opacity(0.2))
                        .cornerRadius(4)

                    Text("RT")
                        .font(.caption)
                        .padding(4)
                        .background(Color.secondary.opacity(0.2))
                        .cornerRadius(4)
                }
            }

            HStack(spacing: 40) {
                // D-Pad
                VStack {
                    Text("D-Pad")
                        .font(.caption2)
                        .foregroundStyle(.secondary)
                }

                // Face buttons
                VStack {
                    Text("Y")
                    HStack {
                        Text("X")
                        Spacer().frame(width: 20)
                        Text("B")
                    }
                    Text("A")
                }
                .font(.caption)
            }
        }
        .frame(maxWidth: .infinity)
        .padding()
    }
}

// MARK: - Button Mapping Tab

private struct ButtonMappingTab: View {
    @State private var selectedProfile = "Default"
    let profiles = ["Default", "Racing", "FPS", "Custom"]

    var body: some View {
        Form {
            Section {
                Picker("Profile", selection: $selectedProfile) {
                    ForEach(profiles, id: \.self) { profile in
                        Text(profile).tag(profile)
                    }
                }

                HStack {
                    Button("New Profile") {}
                    Button("Delete") {}
                        .disabled(selectedProfile == "Default")
                }
            } header: {
                Text("Button Mapping Profiles")
            }

            Section {
                Text("Button remapping will be available in a future update.")
                    .font(.callout)
                    .foregroundStyle(.secondary)

                // Placeholder for button mapping grid
                buttonMappingPlaceholder
            } header: {
                Text("Button Assignments")
            }
        }
        .formStyle(.grouped)
        .padding()
    }

    private var buttonMappingPlaceholder: some View {
        VStack(alignment: .leading, spacing: 8) {
            ForEach(["A", "B", "X", "Y", "LB", "RB", "LT", "RT"], id: \.self) { button in
                HStack {
                    Text(button)
                        .frame(width: 30)
                    Image(systemName: "arrow.right")
                        .foregroundStyle(.secondary)
                    Text(button)
                        .foregroundStyle(.secondary)
                    Spacer()
                    Image(systemName: "chevron.right")
                        .foregroundStyle(.tertiary)
                }
                .padding(.vertical, 4)
            }
        }
        .opacity(0.5)
    }
}

// MARK: - LED Control Tab

private struct LEDControlTab: View {
    @State private var led1Enabled = true
    @State private var led2Enabled = false
    @State private var led3Enabled = false
    @State private var led4Enabled = false
    @State private var brightness: Double = 1.0

    var body: some View {
        Form {
            Section {
                Toggle("LED 1", isOn: $led1Enabled)
                Toggle("LED 2", isOn: $led2Enabled)
                Toggle("LED 3", isOn: $led3Enabled)
                Toggle("LED 4", isOn: $led4Enabled)
            } header: {
                Text("LED Control")
            }

            Section {
                Slider(value: $brightness, in: 0...1) {
                    Text("Brightness")
                }
                Text("\(Int(brightness * 100))%")
                    .foregroundStyle(.secondary)
            } header: {
                Text("Brightness")
            }

            Section {
                Text("LED control will be available when a controller is connected.")
                    .font(.callout)
                    .foregroundStyle(.secondary)
            }
        }
        .formStyle(.grouped)
        .padding()
        .disabled(true) // Disabled until controller connected and feature implemented
    }
}

// MARK: - About Tab

private struct AboutTab: View {
    var body: some View {
        VStack(spacing: 20) {
            // App Icon
            Image(systemName: "gamecontroller.fill")
                .font(.system(size: 64))
                .foregroundStyle(.blue)

            // App Name and Version
            VStack(spacing: 4) {
                Text(AppConstants.appName)
                    .font(.title)
                    .fontWeight(.semibold)

                Text("Version \(appVersion) (\(buildNumber))")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }

            // Description
            Text("A DriverKit-based macOS driver for Bigben Interactive and NACON game controllers.")
                .font(.body)
                .multilineTextAlignment(.center)
                .foregroundStyle(.secondary)
                .padding(.horizontal)

            Divider()
                .padding(.horizontal, 40)

            // Links
            VStack(spacing: 12) {
                Link(destination: URL(string: "https://github.com/RaulAdSe/3d-party-controller-mac-drivers")!) {
                    HStack {
                        Image(systemName: "link")
                        Text("GitHub Repository")
                    }
                }

                Link(destination: URL(string: "https://github.com/RaulAdSe/3d-party-controller-mac-drivers/issues")!) {
                    HStack {
                        Image(systemName: "exclamationmark.bubble")
                        Text("Report an Issue")
                    }
                }
            }

            Spacer()

            // Copyright
            Text("MIT License")
                .font(.caption2)
                .foregroundStyle(.tertiary)
        }
        .padding()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
    }

    private var appVersion: String {
        Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0"
    }

    private var buildNumber: String {
        Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1"
    }
}

// MARK: - Preview

#if DEBUG
struct PreferencesView_Previews: PreviewProvider {
    static var previews: some View {
        PreferencesView(
            controllerMonitor: ControllerMonitor(),
            driverManager: DriverManager()
        )
    }
}
#endif
