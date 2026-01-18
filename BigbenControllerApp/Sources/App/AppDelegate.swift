//
//  AppDelegate.swift
//  BigbenControllerApp
//
//  App Delegate responsible for:
//  - Requesting SystemExtension installation
//  - Handling driver extension lifecycle
//  - Managing app-level events
//

import AppKit
import SystemExtensions
import os.log

class AppDelegate: NSObject, NSApplicationDelegate {
    private let logger = Logger(subsystem: AppConstants.bundleIdentifier, category: "AppDelegate")
    private var extensionDelegate: SystemExtensionDelegate?

    // MARK: - NSApplicationDelegate

    func applicationDidFinishLaunching(_ notification: Notification) {
        logger.info("Bigben Controller App launched")

        // Configure app to run as accessory (menu bar only)
        NSApp.setActivationPolicy(.accessory)

        // Check and install driver extension if needed
        checkDriverStatus()

        // Register for termination notification
        NotificationCenter.default.addObserver(
            self,
            selector: #selector(applicationWillTerminate(_:)),
            name: NSApplication.willTerminateNotification,
            object: nil
        )
    }

    @objc func applicationWillTerminate(_ notification: Notification) {
        logger.info("Bigben Controller App terminating")
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        // Don't terminate when preferences window is closed
        return false
    }

    // MARK: - Driver Extension Management

    private func checkDriverStatus() {
        logger.info("Checking driver extension status")

        // Request extension activation
        // This will prompt user for approval if not already installed
        activateExtension()
    }

    func activateExtension() {
        logger.info("Requesting driver extension activation")

        let request = OSSystemExtensionRequest.activationRequest(
            forExtensionWithIdentifier: AppConstants.driverBundleIdentifier,
            queue: .main
        )

        extensionDelegate = SystemExtensionDelegate()
        request.delegate = extensionDelegate

        OSSystemExtensionManager.shared.submitRequest(request)
    }

    func deactivateExtension() {
        logger.info("Requesting driver extension deactivation")

        let request = OSSystemExtensionRequest.deactivationRequest(
            forExtensionWithIdentifier: AppConstants.driverBundleIdentifier,
            queue: .main
        )

        extensionDelegate = SystemExtensionDelegate()
        request.delegate = extensionDelegate

        OSSystemExtensionManager.shared.submitRequest(request)
    }
}

// MARK: - System Extension Delegate

class SystemExtensionDelegate: NSObject, OSSystemExtensionRequestDelegate {
    private let logger = Logger(subsystem: AppConstants.bundleIdentifier, category: "SystemExtension")

    func request(
        _ request: OSSystemExtensionRequest,
        actionForReplacingExtension existing: OSSystemExtensionProperties,
        withExtension ext: OSSystemExtensionProperties
    ) -> OSSystemExtensionRequest.ReplacementAction {
        logger.info("Extension replacement requested: \(existing.bundleVersion) -> \(ext.bundleVersion)")

        // Allow replacing older versions
        return .replace
    }

    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        logger.warning("Extension requires user approval in System Preferences")

        // Post notification to update UI
        NotificationCenter.default.post(
            name: .driverNeedsApproval,
            object: nil
        )

        // Show alert to guide user
        DispatchQueue.main.async {
            self.showApprovalAlert()
        }
    }

    func request(
        _ request: OSSystemExtensionRequest,
        didFinishWithResult result: OSSystemExtensionRequest.Result
    ) {
        switch result {
        case .completed:
            logger.info("Extension request completed successfully")
            NotificationCenter.default.post(
                name: .driverInstalled,
                object: nil
            )

        case .willCompleteAfterReboot:
            logger.info("Extension will complete after reboot")
            NotificationCenter.default.post(
                name: .driverNeedsReboot,
                object: nil
            )

        @unknown default:
            logger.warning("Extension request finished with unknown result")
        }
    }

    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        let nsError = error as NSError
        logger.error("Extension request failed: \(error.localizedDescription) (code: \(nsError.code))")

        // Post notification with error
        NotificationCenter.default.post(
            name: .driverInstallFailed,
            object: nil,
            userInfo: ["error": error]
        )

        // Handle specific error codes
        handleExtensionError(nsError)
    }

    // MARK: - Private Methods

    private func showApprovalAlert() {
        let alert = NSAlert()
        alert.messageText = "Driver Approval Required"
        alert.informativeText = """
            The Bigben Controller driver needs approval to run.

            Please go to System Settings > Privacy & Security and click "Allow" \
            next to the blocked extension.
            """
        alert.alertStyle = .informational
        alert.addButton(withTitle: "Open System Settings")
        alert.addButton(withTitle: "Later")

        let response = alert.runModal()
        if response == .alertFirstButtonReturn {
            openSecurityPreferences()
        }
    }

    private func openSecurityPreferences() {
        // Open System Settings to Privacy & Security
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy") {
            NSWorkspace.shared.open(url)
        }
    }

    private func handleExtensionError(_ error: NSError) {
        // OSSystemExtensionError codes
        switch error.code {
        case 1: // unknownExtension
            logger.error("Driver extension not found in app bundle")
        case 2: // missingEntitlement
            logger.error("App missing required entitlement for system extension")
        case 3: // extensionNotFound
            logger.error("Extension bundle not found")
        case 4: // extensionMissingIdentifier
            logger.error("Extension missing bundle identifier")
        case 5: // duplicateExtension
            logger.error("Duplicate extension detected")
        case 6: // unknownExtensionCategory
            logger.error("Unknown extension category")
        case 7: // codeSignatureInvalid
            logger.error("Extension code signature is invalid")
        case 8: // validationFailed
            logger.error("Extension validation failed")
        case 9: // forbiddenBySystemPolicy
            logger.error("Extension forbidden by system policy")
        case 10: // requestCanceled
            logger.info("Extension request was canceled")
        case 11: // requestSuperseded
            logger.info("Extension request was superseded")
        case 12: // authorizationRequired
            logger.warning("User authorization required for extension")
        default:
            logger.error("Unknown extension error: \(error.code)")
        }
    }
}

// MARK: - Notification Names

extension Notification.Name {
    static let driverInstalled = Notification.Name("com.bigben.driverInstalled")
    static let driverNeedsApproval = Notification.Name("com.bigben.driverNeedsApproval")
    static let driverNeedsReboot = Notification.Name("com.bigben.driverNeedsReboot")
    static let driverInstallFailed = Notification.Name("com.bigben.driverInstallFailed")
    static let driverUninstalled = Notification.Name("com.bigben.driverUninstalled")
}
