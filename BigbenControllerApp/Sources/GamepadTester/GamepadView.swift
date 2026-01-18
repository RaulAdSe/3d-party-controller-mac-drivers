//
//  GamepadView.swift
//  GamepadTester
//
//  Visual representation of gamepad state
//

import SwiftUI

struct GamepadView: View {
    @StateObject private var controller = USBControllerReader()

    var body: some View {
        ZStack {
            // Background gradient
            LinearGradient(
                colors: [Color(white: 0.12), Color(white: 0.08)],
                startPoint: .top,
                endPoint: .bottom
            )
            .ignoresSafeArea()

            VStack(spacing: 20) {
                // Header
                HStack {
                    Circle()
                        .fill(controller.isConnected ? Color.green : Color.red)
                        .frame(width: 12, height: 12)
                    Text(controller.statusMessage)
                        .font(.system(size: 14, weight: .medium))
                        .foregroundColor(.white.opacity(0.8))
                    Spacer()
                    Text("Gamepad Tester")
                        .font(.system(size: 16, weight: .bold))
                        .foregroundColor(.white)
                }
                .padding(.horizontal, 30)
                .padding(.top, 20)

                Spacer()

                // Main controller layout
                HStack(spacing: 60) {
                    // Left side
                    VStack(spacing: 30) {
                        // Left bumper
                        BumperView(label: "LB", isPressed: controller.state.buttonLB)

                        // Left trigger
                        TriggerView(label: "LT", value: controller.state.leftTriggerNormalized)

                        // Left stick
                        StickView(
                            label: "L",
                            x: controller.state.leftStickXNormalized,
                            y: controller.state.leftStickYNormalized,
                            isPressed: controller.state.buttonL3
                        )
                    }

                    // Center section
                    VStack(spacing: 25) {
                        // Top row: Back, Home, Start
                        HStack(spacing: 20) {
                            SmallButtonView(label: "BACK", isPressed: controller.state.buttonBack)
                            SmallButtonView(label: "HOME", isPressed: controller.state.buttonHome, color: .blue)
                            SmallButtonView(label: "START", isPressed: controller.state.buttonStart)
                        }

                        // Middle: D-pad and Face buttons
                        HStack(spacing: 80) {
                            DPadView(
                                up: controller.state.dpadUp,
                                down: controller.state.dpadDown,
                                left: controller.state.dpadLeft,
                                right: controller.state.dpadRight
                            )

                            FaceButtonsView(
                                a: controller.state.buttonA,
                                b: controller.state.buttonB,
                                x: controller.state.buttonX,
                                y: controller.state.buttonY
                            )
                        }

                        // Raw values display
                        RawValuesView(state: controller.state)
                    }

                    // Right side
                    VStack(spacing: 30) {
                        // Right bumper
                        BumperView(label: "RB", isPressed: controller.state.buttonRB)

                        // Right trigger
                        TriggerView(label: "RT", value: controller.state.rightTriggerNormalized)

                        // Right stick
                        StickView(
                            label: "R",
                            x: controller.state.rightStickXNormalized,
                            y: controller.state.rightStickYNormalized,
                            isPressed: controller.state.buttonR3
                        )
                    }
                }

                Spacer()
            }
        }
        .onAppear {
            controller.start()
        }
        .onDisappear {
            controller.stop()
        }
    }
}

// MARK: - Stick View

struct StickView: View {
    let label: String
    let x: Double
    let y: Double
    let isPressed: Bool

    private let size: CGFloat = 120
    private let dotSize: CGFloat = 30

    var body: some View {
        VStack(spacing: 8) {
            ZStack {
                // Outer ring
                Circle()
                    .stroke(Color.white.opacity(0.3), lineWidth: 2)
                    .frame(width: size, height: size)

                // Inner background
                Circle()
                    .fill(Color.white.opacity(0.05))
                    .frame(width: size - 4, height: size - 4)

                // Crosshair
                Path { path in
                    path.move(to: CGPoint(x: size/2, y: 10))
                    path.addLine(to: CGPoint(x: size/2, y: size - 10))
                    path.move(to: CGPoint(x: 10, y: size/2))
                    path.addLine(to: CGPoint(x: size - 10, y: size/2))
                }
                .stroke(Color.white.opacity(0.15), lineWidth: 1)
                .frame(width: size, height: size)

                // Stick position dot (Y inverted: up on stick = up on screen)
                Circle()
                    .fill(isPressed ? Color.green : Color.blue)
                    .shadow(color: (isPressed ? Color.green : Color.blue).opacity(0.5), radius: 8)
                    .frame(width: dotSize, height: dotSize)
                    .offset(
                        x: CGFloat(x) * (size - dotSize) / 2,
                        y: CGFloat(-y) * (size - dotSize) / 2
                    )
            }

            // Label and values
            VStack(spacing: 2) {
                Text("\(label)3")
                    .font(.system(size: 12, weight: .bold))
                    .foregroundColor(isPressed ? .green : .white.opacity(0.6))

                Text(String(format: "%.2f, %.2f", x, y))
                    .font(.system(size: 10, design: .monospaced))
                    .foregroundColor(.white.opacity(0.5))
            }
        }
    }
}

// MARK: - Trigger View

struct TriggerView: View {
    let label: String
    let value: Double

    var body: some View {
        VStack(spacing: 6) {
            Text(label)
                .font(.system(size: 12, weight: .bold))
                .foregroundColor(value > 0.1 ? .orange : .white.opacity(0.6))

            ZStack(alignment: .bottom) {
                // Background
                RoundedRectangle(cornerRadius: 4)
                    .fill(Color.white.opacity(0.1))
                    .frame(width: 50, height: 80)

                // Fill
                RoundedRectangle(cornerRadius: 4)
                    .fill(
                        LinearGradient(
                            colors: [Color.orange, Color.red],
                            startPoint: .bottom,
                            endPoint: .top
                        )
                    )
                    .frame(width: 50, height: CGFloat(value) * 80)
            }

            Text(String(format: "%.0f%%", value * 100))
                .font(.system(size: 10, design: .monospaced))
                .foregroundColor(.white.opacity(0.5))
        }
    }
}

// MARK: - Bumper View

struct BumperView: View {
    let label: String
    let isPressed: Bool

    var body: some View {
        Text(label)
            .font(.system(size: 14, weight: .bold))
            .foregroundColor(isPressed ? .black : .white.opacity(0.8))
            .frame(width: 60, height: 30)
            .background(
                RoundedRectangle(cornerRadius: 6)
                    .fill(isPressed ? Color.cyan : Color.white.opacity(0.15))
            )
            .overlay(
                RoundedRectangle(cornerRadius: 6)
                    .stroke(Color.white.opacity(0.3), lineWidth: 1)
            )
    }
}

// MARK: - D-Pad View

struct DPadView: View {
    let up: Bool
    let down: Bool
    let left: Bool
    let right: Bool

    private let buttonSize: CGFloat = 35

    var body: some View {
        VStack(spacing: 0) {
            // Up
            DPadButton(direction: "chevron.up", isPressed: up)

            HStack(spacing: 0) {
                // Left
                DPadButton(direction: "chevron.left", isPressed: left)

                // Center
                Rectangle()
                    .fill(Color.white.opacity(0.1))
                    .frame(width: buttonSize, height: buttonSize)

                // Right
                DPadButton(direction: "chevron.right", isPressed: right)
            }

            // Down
            DPadButton(direction: "chevron.down", isPressed: down)
        }
        .background(
            RoundedRectangle(cornerRadius: 8)
                .fill(Color.black.opacity(0.3))
        )
    }
}

struct DPadButton: View {
    let direction: String
    let isPressed: Bool

    var body: some View {
        Image(systemName: direction)
            .font(.system(size: 14, weight: .bold))
            .foregroundColor(isPressed ? .black : .white.opacity(0.7))
            .frame(width: 35, height: 35)
            .background(isPressed ? Color.white : Color.white.opacity(0.1))
    }
}

// MARK: - Face Buttons View

struct FaceButtonsView: View {
    let a: Bool
    let b: Bool
    let x: Bool
    let y: Bool

    var body: some View {
        VStack(spacing: 5) {
            // Y (top)
            FaceButton(label: "Y", isPressed: y, color: .yellow)

            HStack(spacing: 5) {
                // X (left)
                FaceButton(label: "X", isPressed: x, color: .blue)

                // Spacer
                Color.clear.frame(width: 40, height: 40)

                // B (right)
                FaceButton(label: "B", isPressed: b, color: .red)
            }

            // A (bottom)
            FaceButton(label: "A", isPressed: a, color: .green)
        }
    }
}

struct FaceButton: View {
    let label: String
    let isPressed: Bool
    let color: Color

    var body: some View {
        Text(label)
            .font(.system(size: 16, weight: .bold))
            .foregroundColor(isPressed ? .white : color)
            .frame(width: 40, height: 40)
            .background(
                Circle()
                    .fill(isPressed ? color : color.opacity(0.2))
                    .shadow(color: isPressed ? color.opacity(0.6) : .clear, radius: 8)
            )
            .overlay(
                Circle()
                    .stroke(color, lineWidth: 2)
            )
    }
}

// MARK: - Small Button View

struct SmallButtonView: View {
    let label: String
    let isPressed: Bool
    var color: Color = .white

    var body: some View {
        Text(label)
            .font(.system(size: 9, weight: .semibold))
            .foregroundColor(isPressed ? .black : color.opacity(0.7))
            .frame(width: 50, height: 24)
            .background(
                RoundedRectangle(cornerRadius: 12)
                    .fill(isPressed ? color : color.opacity(0.15))
            )
    }
}

// MARK: - Raw Values View

struct RawValuesView: View {
    let state: ControllerState

    var body: some View {
        VStack(spacing: 8) {
            Text("RAW VALUES")
                .font(.system(size: 10, weight: .bold))
                .foregroundColor(.white.opacity(0.4))

            HStack(spacing: 20) {
                VStack(alignment: .leading, spacing: 2) {
                    Text("Left Stick")
                        .font(.system(size: 9))
                        .foregroundColor(.white.opacity(0.5))
                    Text(String(format: "X: %3d  Y: %3d", state.leftStickX, state.leftStickY))
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.white.opacity(0.7))
                }

                VStack(alignment: .leading, spacing: 2) {
                    Text("Right Stick")
                        .font(.system(size: 9))
                        .foregroundColor(.white.opacity(0.5))
                    Text(String(format: "X: %3d  Y: %3d", state.rightStickX, state.rightStickY))
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.white.opacity(0.7))
                }

                VStack(alignment: .leading, spacing: 2) {
                    Text("Triggers")
                        .font(.system(size: 9))
                        .foregroundColor(.white.opacity(0.5))
                    Text(String(format: "L: %3d  R: %3d", state.leftTrigger, state.rightTrigger))
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.white.opacity(0.7))
                }

                VStack(alignment: .leading, spacing: 2) {
                    Text("Buttons")
                        .font(.system(size: 9))
                        .foregroundColor(.white.opacity(0.5))
                    Text(String(format: "0x%04X", state.buttons))
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(.white.opacity(0.7))
                }
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 10)
            .background(
                RoundedRectangle(cornerRadius: 8)
                    .fill(Color.white.opacity(0.05))
            )
        }
    }
}

#Preview {
    GamepadView()
        .frame(width: 700, height: 500)
}
