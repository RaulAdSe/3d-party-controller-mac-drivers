//
//  StatusBarView.swift
//  BigbenControllerApp
//
//  Menu bar view showing:
//  - Connection status icon (green/red)
//  - Controller name when connected
//  - "Open Preferences" menu item
//  - "Quit" menu item
//

import SwiftUI

struct StatusBarView: View {
    @ObservedObject var controllerMonitor: ControllerMonitor
    @ObservedObject var driverManager: DriverManager
    @Environment(\.openSettings) private var openSettings

    var body: some View {
        VStack(alignment: .leading, spacing: 0) {
            // Controller Status Section
            controllerStatusSection

            Divider()
                .padding(.vertical, 4)

            // Driver Status Section
            driverStatusSection

            Divider()
                .padding(.vertical, 4)

            // Actions Section
            actionsSection
        }
        .frame(minWidth: 220)
    }

    // MARK: - Controller Status Section

    private var controllerStatusSection: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack(spacing: 8) {
                Image(systemName: controllerMonitor.isConnected ? "gamecontroller.fill" : "gamecontroller")
                    .foregroundStyle(controllerMonitor.isConnected ? .green : .secondary)
                    .font(.title2)

                VStack(alignment: .leading, spacing: 2) {
                    Text(controllerMonitor.isConnected ? "Connected" : "Disconnected")
                        .font(.headline)
                        .foregroundStyle(controllerMonitor.isConnected ? .primary : .secondary)

                    if let info = controllerMonitor.controllerInfo {
                        Text(info.displayName)
                            .font(.caption)
                            .foregroundStyle(.secondary)
                    }
                }

                Spacer()
            }
            .padding(.horizontal, 12)
            .padding(.vertical, 8)

            // Controller details when connected
            if let info = controllerMonitor.controllerInfo {
                controllerDetailsView(info: info)
            }
        }
    }

    private func controllerDetailsView(info: ControllerInfo) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            DetailRow(label: "Product", value: info.productDescription)
            DetailRow(label: "Manufacturer", value: info.manufacturer)

            if let serial = info.serialNumber, !serial.isEmpty {
                DetailRow(label: "Serial", value: serial)
            }
        }
        .padding(.horizontal, 12)
        .padding(.bottom, 8)
    }

    // MARK: - Driver Status Section

    private var driverStatusSection: some View {
        HStack(spacing: 8) {
            Image(systemName: driverManager.state.symbolName)
                .foregroundStyle(driverStatusColor)
                .font(.body)

            Text("Driver: \(driverManager.state.description)")
                .font(.subheadline)

            Spacer()

            if driverManager.state == .needsApproval {
                Button("Approve") {
                    driverManager.openApprovalSettings()
                }
                .buttonStyle(.borderless)
                .font(.caption)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 4)
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

    // MARK: - Actions Section

    private var actionsSection: some View {
        VStack(spacing: 0) {
            // Preferences
            Button {
                openSettings()
            } label: {
                HStack {
                    Image(systemName: "gearshape")
                    Text("Preferences...")
                    Spacer()
                    Text("\u{2318},")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .buttonStyle(MenuButtonStyle())

            // Refresh
            Button {
                controllerMonitor.refresh()
            } label: {
                HStack {
                    Image(systemName: "arrow.clockwise")
                    Text("Refresh")
                    Spacer()
                }
            }
            .buttonStyle(MenuButtonStyle())

            Divider()
                .padding(.vertical, 4)

            // About
            Button {
                showAboutWindow()
            } label: {
                HStack {
                    Image(systemName: "info.circle")
                    Text("About \(AppConstants.appName)")
                    Spacer()
                }
            }
            .buttonStyle(MenuButtonStyle())

            // Quit
            Button {
                NSApplication.shared.terminate(nil)
            } label: {
                HStack {
                    Image(systemName: "power")
                    Text("Quit")
                    Spacer()
                    Text("\u{2318}Q")
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }
            }
            .buttonStyle(MenuButtonStyle())
        }
    }

    // MARK: - Helper Methods

    private func showAboutWindow() {
        NSApplication.shared.orderFrontStandardAboutPanel(
            options: [
                .applicationName: AppConstants.appName,
                .applicationVersion: Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "1.0",
                .version: Bundle.main.infoDictionary?["CFBundleVersion"] as? String ?? "1",
                .credits: NSAttributedString(
                    string: "A DriverKit-based macOS driver for Bigben Interactive controllers.",
                    attributes: [.font: NSFont.systemFont(ofSize: 11)]
                )
            ]
        )
    }
}

// MARK: - Detail Row

private struct DetailRow: View {
    let label: String
    let value: String

    var body: some View {
        HStack {
            Text(label)
                .font(.caption)
                .foregroundStyle(.secondary)
                .frame(width: 80, alignment: .trailing)

            Text(value)
                .font(.caption)
                .foregroundStyle(.primary)

            Spacer()
        }
    }
}

// MARK: - Menu Button Style

private struct MenuButtonStyle: ButtonStyle {
    @State private var isHovered = false

    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .padding(.horizontal, 12)
            .padding(.vertical, 6)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(cornerRadius: 4)
                    .fill(isHovered ? Color.accentColor.opacity(0.2) : Color.clear)
            )
            .contentShape(Rectangle())
            .onHover { hovering in
                isHovered = hovering
            }
    }
}

// MARK: - Preview

#if DEBUG
struct StatusBarView_Previews: PreviewProvider {
    static var previews: some View {
        StatusBarView(
            controllerMonitor: ControllerMonitor(),
            driverManager: DriverManager()
        )
        .frame(width: 250)
    }
}
#endif
