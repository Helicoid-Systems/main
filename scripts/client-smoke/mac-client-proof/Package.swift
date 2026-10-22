// swift-tools-version: 6.2
import PackageDescription

let package = Package(
    name: "ClientProof",
    platforms: [.macOS(.v15)],
    dependencies: [.package(path: "../../../apps/macos")],
    targets: [
        .executableTarget(
            name: "ClientProof",
            dependencies: [.product(name: "HelicoidCore", package: "macos")]
        ),
    ]
)
