//
//  DriverManager.swift
//  BigbenControllerApp
//
//  Service class responsible for:
//  - Using SystemExtensions framework to install/uninstall driver
//  - Checking driver status
//  - Handling approval flow
//

import Foundation
import SystemExtensions
import Combine
import os.log

// MARK: - Driver State

enum DriverState: Equatable {
    case unknown
    case notInstalled
    case installing
    case needsApproval
    case needsReboot
    case installed
    case error(String)

    var description: String {
        switch self {
        case .unknown:
            return "Checking..."
        case .notInstalled:
            return "Not Installed"
        case .installing:
            return "Installing..."
        case .needsApproval:
            return "Needs Approval"
        case .needsReboot:
            return "Restart Required"
        case .installed:
            return "Installed"
        case .error(let message):
            return "Error: \(message)"
        }
    }

    var symbolName: String {
        switch self {
        case .unknown:
            return "questionmark.circle"
        case .notInstalled:
            return "xmark.circle"
        case .installing:
            return "arrow.down.circle"
        case .needsApproval:
            return "exclamationmark.triangle"
        case .needsReboot:
            return "arrow.clockwise.circle"
        case .installed:
            return "checkmark.circle.fill"
        case .error:
            return "xmark.octagon"
        }
    }

    var color: String {
        switch self {
        case .unknown:
            return "gray"
        case .notInstalled:
            return "orange"
        case .installing:
            return "blue"
        case .needsApproval:
            return "yellow"
        case .needsReboot:
            return "purple"
        case .installed:
            return "green"
        case .error:
            return "red"
        }
    }
}

// MARK: - Driver Manager

@MainActor
class DriverManager: NSObject, ObservableObject {
    // MARK: - Published Properties

    @Published private(set) var state: DriverState = .unknown
    @Published private(set) var isProcessing = false

    // MARK: - Private Properties

    private let logger = Logger(subsystem: AppConstants.bundleIdentifier, category: "DriverManager")
    private var cancellables = Set<AnyCancellable>()
    private var extensionDelegate: DriverExtensionDelegate?

    // MARK: - Initialization

    override init() {
        super.init()
        setupNotificationObservers()
        checkDriverStatus()
    }

    // MARK: - Public Methods

    /// Install the driver extension
    func installDriver() {
        guard !isProcessing else {
            logger.warning("Driver installation already in progress")
            return
        }

        logger.info("Starting driver installation")
        isProcessing = true
        state = .installing

        let request = OSSystemExtensionRequest.activationRequest(
            forExtensionWithIdentifier: AppConstants.driverBundleIdentifier,
            queue: .main
        )

        extensionDelegate = DriverExtensionDelegate { [weak self] result in
            Task { @MainActor in
                self?.handleInstallationResult(result)
            }
        }
        request.delegate = extensionDelegate

        OSSystemExtensionManager.shared.submitRequest(request)
    }

    /// Uninstall the driver extension
    func uninstallDriver() {
        guard !isProcessing else {
            logger.warning("Driver operation already in progress")
            return
        }

        logger.info("Starting driver uninstallation")
        isProcessing = true

        let request = OSSystemExtensionRequest.deactivationRequest(
            forExtensionWithIdentifier: AppConstants.driverBundleIdentifier,
            queue: .main
        )

        extensionDelegate = DriverExtensionDelegate { [weak self] result in
            Task { @MainActor in
                self?.handleUninstallationResult(result)
            }
        }
        request.delegate = extensionDelegate

        OSSystemExtensionManager.shared.submitRequest(request)
    }

    /// Check current driver status
    func checkDriverStatus() {
        logger.info("Checking driver status")

        // Check if the driver extension bundle exists in the app
        let driverBundlePath = Bundle.main.builtInPlugInsPath
        let driverExists = driverBundlePath != nil

        if !driverExists {
            logger.warning("Driver extension not found in app bundle")
            state = .notInstalled
            return
        }

        // Try to query system extension status
        // Note: There's no direct API to query status, so we rely on notifications
        // and attempt an activation which will report current state
        state = .unknown
    }

    /// Open System Settings to approve the driver
    func openApprovalSettings() {
        logger.info("Opening System Settings for driver approval")

        // macOS 13+ uses System Settings
        if let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy") {
            NSWorkspace.shared.open(url)
        }
    }

    // MARK: - Private Methods

    private func setupNotificationObservers() {
        NotificationCenter.default.publisher(for: .driverInstalled)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.state = .installed
                self?.isProcessing = false
            }
            .store(in: &cancellables)

        NotificationCenter.default.publisher(for: .driverNeedsApproval)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.state = .needsApproval
                self?.isProcessing = false
            }
            .store(in: &cancellables)

        NotificationCenter.default.publisher(for: .driverNeedsReboot)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.state = .needsReboot
                self?.isProcessing = false
            }
            .store(in: &cancellables)

        NotificationCenter.default.publisher(for: .driverInstallFailed)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] notification in
                if let error = notification.userInfo?["error"] as? Error {
                    self?.state = .error(error.localizedDescription)
                } else {
                    self?.state = .error("Unknown error")
                }
                self?.isProcessing = false
            }
            .store(in: &cancellables)

        NotificationCenter.default.publisher(for: .driverUninstalled)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] _ in
                self?.state = .notInstalled
                self?.isProcessing = false
            }
            .store(in: &cancellables)
    }

    private func handleInstallationResult(_ result: Result<OSSystemExtensionRequest.Result, Error>) {
        isProcessing = false

        switch result {
        case .success(let extensionResult):
            switch extensionResult {
            case .completed:
                logger.info("Driver installation completed")
                state = .installed

            case .willCompleteAfterReboot:
                logger.info("Driver installation will complete after reboot")
                state = .needsReboot

            @unknown default:
                logger.warning("Unknown installation result")
                state = .installed
            }

        case .failure(let error):
            logger.error("Driver installation failed: \(error.localizedDescription)")
            state = .error(error.localizedDescription)
        }
    }

    private func handleUninstallationResult(_ result: Result<OSSystemExtensionRequest.Result, Error>) {
        isProcessing = false

        switch result {
        case .success:
            logger.info("Driver uninstallation completed")
            state = .notInstalled
            NotificationCenter.default.post(name: .driverUninstalled, object: nil)

        case .failure(let error):
            logger.error("Driver uninstallation failed: \(error.localizedDescription)")
            state = .error(error.localizedDescription)
        }
    }
}

// MARK: - Driver Extension Delegate

private class DriverExtensionDelegate: NSObject, OSSystemExtensionRequestDelegate {
    typealias CompletionHandler = (Result<OSSystemExtensionRequest.Result, Error>) -> Void

    private let logger = Logger(subsystem: AppConstants.bundleIdentifier, category: "DriverExtensionDelegate")
    private let completion: CompletionHandler
    private var didReportApprovalNeeded = false

    init(completion: @escaping CompletionHandler) {
        self.completion = completion
        super.init()
    }

    func request(
        _ request: OSSystemExtensionRequest,
        actionForReplacingExtension existing: OSSystemExtensionProperties,
        withExtension ext: OSSystemExtensionProperties
    ) -> OSSystemExtensionRequest.ReplacementAction {
        logger.info("Replacing extension \(existing.bundleVersion) with \(ext.bundleVersion)")
        return .replace
    }

    func requestNeedsUserApproval(_ request: OSSystemExtensionRequest) {
        logger.warning("User approval needed for extension")
        didReportApprovalNeeded = true

        NotificationCenter.default.post(
            name: .driverNeedsApproval,
            object: nil
        )
    }

    func request(
        _ request: OSSystemExtensionRequest,
        didFinishWithResult result: OSSystemExtensionRequest.Result
    ) {
        logger.info("Extension request finished with result: \(String(describing: result))")
        completion(.success(result))
    }

    func request(_ request: OSSystemExtensionRequest, didFailWithError error: Error) {
        logger.error("Extension request failed: \(error.localizedDescription)")
        completion(.failure(error))
    }
}
