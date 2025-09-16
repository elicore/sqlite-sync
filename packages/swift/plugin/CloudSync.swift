import PackagePlugin
import Foundation

@main
struct CloudSync: BuildToolPlugin {
    /// Entry point for creating build commands for targets in Swift packages.
    func createBuildCommands(context: PluginContext, target: Target) async throws -> [Command] {
        let packageDirectory = context.package.directoryURL
        let outputDirectory = context.pluginWorkDirectoryURL
        return createCloudSyncBuildCommands(packageDirectory: packageDirectory, outputDirectory: outputDirectory)
    }
}

#if canImport(XcodeProjectPlugin)
import XcodeProjectPlugin

extension CloudSync: XcodeBuildToolPlugin {
    // Entry point for creating build commands for targets in Xcode projects.
    func createBuildCommands(context: XcodePluginContext, target: XcodeTarget) throws -> [Command] {
        let outputDirectory = context.pluginWorkDirectoryURL
        return createCloudSyncBuildCommands(packageDirectory: nil, outputDirectory: outputDirectory)
    }
}

#endif

/// Shared function to create CloudSync build commands
func createCloudSyncBuildCommands(packageDirectory: URL?, outputDirectory: URL) -> [Command] {

    // For Xcode projects, use current directory; for Swift packages, use provided packageDirectory
    let workingDirectory = packageDirectory?.path ?? "$(pwd)"
    let packageDirInfo = packageDirectory != nil ? "Package directory: \(packageDirectory!.path)" : "Working directory: $(pwd)"

    return [
        .prebuildCommand(
            displayName: "Building CloudSync XCFramework",
            executable: URL(fileURLWithPath: "/bin/bash"),
            arguments: [
                "-c",
                """
                set -e
                echo "Starting CloudSync XCFramework prebuild..."
                echo "\(packageDirInfo)"
                
                # Clean and create output directory
                rm -rf "\(outputDirectory.path)"
                mkdir -p "\(outputDirectory.path)"
                
                # Build directly from source directory with custom output paths
                cd "\(workingDirectory)" && \
                echo "Building XCFramework with native network..." && \
                make xcframework NATIVE_NETWORK=ON DIST_DIR="\(outputDirectory.path)" BUILD_RELEASE="\(outputDirectory.path)/build/release" BUILD_TEST="\(outputDirectory.path)/build/test" && \
                rm -rf "\(outputDirectory.path)/build" && \
                echo "XCFramework build completed successfully!"
                """
            ],
            outputFilesDirectory: outputDirectory
        )
    ]
}