// swift-tools-version:5.7
import PackageDescription

let package = Package(
    name: "BigbenController",
    platforms: [
        .macOS(.v12)
    ],
    products: [
        .executable(
            name: "bigben-mapper",
            targets: ["BigbenMapper"]
        )
    ],
    dependencies: [],
    targets: [
        // C library for USB communication via libusb
        .target(
            name: "CUSBController",
            path: "BigbenControllerApp/Sources/CUSBController",
            sources: ["BigbenUSB.c"],
            publicHeadersPath: "include",
            cSettings: [
                .define("_GNU_SOURCE"),
                .unsafeFlags(["-I/opt/homebrew/include"])
            ],
            linkerSettings: [
                .linkedLibrary("usb-1.0"),
                .unsafeFlags(["-L/opt/homebrew/lib"])
            ]
        ),
        // Main Swift executable
        .executableTarget(
            name: "BigbenMapper",
            dependencies: ["CUSBController"],
            path: "BigbenControllerApp/Sources",
            exclude: [
                "App/BigbenControllerApp.swift",
                "App/AppDelegate.swift",
                "Views",
                "Services/ControllerMonitor.swift",
                "Services/DriverManager.swift",
                "CUSBController"
            ],
            sources: [
                "main.swift",
                "Services/USBController.swift",
                "Services/KeyboardEmulator.swift"
            ],
            linkerSettings: [
                .linkedFramework("IOKit"),
                .linkedFramework("CoreGraphics"),
                .linkedFramework("AppKit")
            ]
        )
    ]
)
