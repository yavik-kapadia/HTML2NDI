// swift-tools-version: 5.9
import PackageDescription

let package = Package(
    name: "HTML2NDI Manager",
    platforms: [
        .macOS(.v13)
    ],
    products: [
        .executable(name: "HTML2NDI Manager", targets: ["HTML2NDI Manager"])
    ],
    targets: [
        .executableTarget(
            name: "HTML2NDI Manager",
            path: "HTML2NDI Manager"
        )
    ]
)

