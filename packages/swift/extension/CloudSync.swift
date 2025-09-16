// CloudSync.swift
// This file serves as a placeholder for the CloudSync target.
// The actual SQLite extension is built using the Makefile through the build plugin.

import Foundation

/// Placeholder structure for CloudSync
public struct CloudSync {
    /// Returns the path to the built CloudSync dylib inside the XCFramework
    public static var path: String {
        #if os(macOS)
        return "CloudSync.xcframework/macos-arm64_x86_64/CloudSync.framework/CloudSync"
        #elseif targetEnvironment(simulator)
        return "CloudSync.xcframework/ios-arm64_x86_64-simulator/CloudSync.framework/CloudSync"
        #else
        return "CloudSync.xcframework/ios-arm64/CloudSync.framework/CloudSync"
        #endif
    }
}