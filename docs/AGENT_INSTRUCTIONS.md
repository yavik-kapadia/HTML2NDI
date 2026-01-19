# Agent Instructions for HTML2NDI Development

This document provides specific guidelines and procedures for AI agents working on the HTML2NDI codebase.

## Table of Contents
- [Code Quality Standards](#code-quality-standards)
- [Version Management](#version-management)
- [Build & Run Procedures](#build--run-procedures)
- [Testing Requirements](#testing-requirements)
- [CI/CD Guidelines](#cicd-guidelines)

---

## Code Quality Standards

### General Principles
- **Always write tests** for new features and bug fixes
- **All documentation** should be stored in `/docs` except `README.md` and `AGENTS.md`
- **Expert-level implementation**: Take on the harder route; only choose the easiest path as a last resort
- **Concise documentation**: Be clear and brief; avoid unnecessary verbosity
- **Test before merge**: The commit should pass all tests before merging
- **Lint before merge**: Always run the linter before merging
- **No hard-coded colors**: Use theme-aware or configurable color systems
- **Avoid emojis** in code, documentation, and commit messages

### Language-Specific Guidelines

#### C++ (Worker)
- Use **C++17/20** features
- Follow **Google C++ Style Guide**
- Use modern constructs:
  - `std::thread`, `std::mutex` for threading
  - Smart pointers (`std::unique_ptr`, `std::shared_ptr`)
  - `std::chrono` for time operations
- **RAII** for all resource management
- Keep external dependencies minimal

#### Swift (Manager)
- Use **Swift 5.9+**
- Follow SwiftUI best practices
- Use `@Published`, `@ObservedObject`, `@StateObject` appropriately
- Leverage Combine framework for reactive programming
- Use `async/await` for asynchronous operations

---

## Version Management

### CRITICAL: Pre-Release Checklist

**BEFORE creating any version tag or release, follow these steps:**

### 1. Check Existing Tags

```bash
git tag -l | sort -V
```

Verify the version you intend to create doesn't already exist and follows proper progression.

### 2. Verify Version Files Match

Ensure consistency across these three locations:

| File | Location | Format |
|------|----------|--------|
| `CMakeLists.txt` | Line ~3 | `project(HTML2NDI VERSION X.Y.Z ...)` |
| `CMakeLists.txt` | Line ~350 | `MACOSX_BUNDLE_BUNDLE_VERSION "X.Y.Z"` |
| `src/app/config.cpp` | Line ~10 | `const char* VERSION = "X.Y.Z";` |

```bash
# Quick check command
grep -r "VERSION.*1\." CMakeLists.txt src/app/config.cpp
```

### 3. Follow Semantic Versioning

| Version Type | Format | Use Case |
|-------------|--------|----------|
| **Major** | `X.0.0` | Breaking changes, incompatible API changes |
| **Minor** | `x.Y.0` | New features, backward compatible |
| **Patch** | `x.y.Z` | Bug fixes, backward compatible |
| **Alpha** | `x.y.z-alpha` | Pre-release testing |
| **Beta** | `x.y.z-beta` | Feature-complete pre-release |

### 4. Version Progression Rules

✅ **Correct Progression:**
```
v1.4.0 → v1.4.1 → v1.5.0-alpha → v1.5.0 → v1.5.1
```

❌ **Incorrect Progression:**
```
v1.5.0 → v1.5.0-alpha  (alpha must come BEFORE stable)
v1.4.0 → v1.4.0-alpha  (if v1.4.0 exists, use v1.5.0-alpha)
```

**Rules:**
- Alpha versions **must come BEFORE** their stable release
- Never create `vX.Y.Z-alpha` if `vX.Y.Z` already exists
- If `vX.Y.Z` exists, next alpha is `vX.Y+1.0-alpha` or `vX+1.0.0-alpha`

### 5. Pre-Commit Version Checklist

Run these commands **before** committing version changes:

```bash
# 1. List all existing tags to avoid conflicts
git tag -l | sort -V

# 2. Verify intended version doesn't conflict
# Example: If creating v1.5.0-alpha, ensure v1.5.0 doesn't exist yet

# 3. Grep all version references
grep -r "VERSION.*1\." CMakeLists.txt src/app/config.cpp

# 4. Ensure all three locations match the intended version
```

### 6. Version Update Procedure

Follow these steps **in order**:

```bash
# 1. Update version in three locations
#    - CMakeLists.txt (2 places)
#    - src/app/config.cpp (1 place)

# 2. Commit version bump separately
git add CMakeLists.txt src/app/config.cpp
git commit -m "chore: Bump version to X.Y.Z"

# 3. Create annotated tag
git tag -a vX.Y.Z -m "Release vX.Y.Z: Brief description"

# 4. Push commit first
git push origin main

# 5. Push tag (triggers release workflow)
git push origin vX.Y.Z
```

### 7. If Version Conflict Detected

If you discover a version conflict:

1. **STOP immediately** - Do not proceed with release
2. **Ask user** which version to use
3. **Common fix**: Increment to next available version
4. **Example**: If `v1.4.0` exists, use `v1.5.0-alpha` (not `v1.4.0-alpha`)

### 8. Automated Pre-Release Verification

**CRITICAL: Run before EVERY release!**

Use the automated pre-release check script:

```bash
./scripts/pre-release-check.sh
```

This script verifies:
- Version consistency across all files
- Clean build from scratch (both C++ and Swift)
- All unit tests pass
- All executables run without errors
- No uncommitted changes
- Required documentation exists
- No common code issues (debug prints, TODOs in critical paths)

**If the script fails, DO NOT proceed with the release until all issues are fixed.**

### 9. Testing Requirements Before Release

Before any release, ensure:

1. **Unit Tests Coverage**:
   - All new features have unit tests
   - All bug fixes have regression tests
   - Audio/Video code paths are tested
   - Run: `./build/html2ndi_tests`

2. **Integration Tests**:
   - Performance test passes: `./tests/integration/test_performance.sh`
   - Manual smoke test of common workflows
   - Test with actual NDI receivers

3. **Compilation Tests**:
   - Clean build succeeds on macOS (arm64)
   - All code paths compile (including unused features like audio)
   - No compiler warnings in release mode

4. **Manual Verification**:
   - Launch HTML2NDI Manager
   - Create a test stream
   - Verify NDI output in a receiver (OBS, vMix, etc.)
   - Check web dashboard works
   - Verify stream controls work
   - Test performance monitoring

---

## Build & Run Procedures

### Prerequisites

```bash
# Install build tools
brew install cmake ninja librsvg

# Install NDI SDK
# Download from: https://ndi.video/download-ndi-sdk/
# Extract to: /Library/NDI SDK for macOS/
```

### Build Worker (C++)

```bash
# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build
ninja

# The binary will be at: build/html2ndi
```

### Build Manager (Swift)

```bash
# Navigate to manager directory
cd manager

# Build with Swift Package Manager
swift build -c release

# Create app bundle
./build.sh

# The app will be at: manager/build/HTML2NDI Manager.app
```

### Run the Application

```bash
# Option 1: Launch from command line
./manager/build/"HTML2NDI Manager.app"/Contents/MacOS/"HTML2NDI Manager"

# Option 2: Open with macOS
open manager/build/"HTML2NDI Manager.app"

# Option 3: Copy to Applications and launch
cp -r manager/build/"HTML2NDI Manager.app" /Applications/
open /Applications/"HTML2NDI Manager.app"
```

Access the web dashboard at: `http://localhost:8080`

---

## Testing Requirements

### Test Coverage

Every feature must have corresponding tests:

| Test Type | Location | Purpose |
|-----------|----------|---------|
| **C++ Unit Tests** | `tests/cpp/` | Core logic, config parsing, APIs, NDI sender |
| **Swift Unit Tests** | `manager/Tests/` | UI components, stream management |
| **Web UI Tests** | `tests/web/` | Dashboard controls, user interactions |
| **Integration Tests** | `tests/integration/` | End-to-end NDI output validation, performance tests |
| **Mock Infrastructure** | `tests/mocks/` | NDI SDK mocks for local testing |

### Mock NDI SDK for Local Testing

For local development without the full NDI SDK:

- **Mock Headers**: `tests/mocks/Processing.NDI.Lib.h`
- **Mock Implementation**: `tests/mocks/ndi_mock.cpp`
- **Purpose**: Provides stub NDI functions for compilation and unit testing
- **Important**: Mock API signatures must match production SDK to catch type errors

The mock SDK is especially useful for:
- Running unit tests without NDI runtime installed
- CI environments where NDI SDK may not be available
- Testing compilation of all code paths (including unused features like audio)

### Running Tests

#### C++ Unit Tests

```bash
cd build
ninja html2ndi_tests
./html2ndi_tests
```

**Test Files**:
- `tests/cpp/test_main.cpp` - Test runner entry point
- `tests/cpp/test_logger.cpp` - Logging functionality
- `tests/cpp/test_config.cpp` - Configuration parsing
- `tests/cpp/test_ndi_sender.cpp` - NDI audio/video sending (NEW)

**Coverage Requirements**:
- All NDI sender public methods must be tested
- Audio and video code paths must have unit tests
- Type compatibility with NDI SDK must be verified

#### Swift Unit Tests

```bash
cd manager
swift test
```

#### Web UI Tests

```bash
# Open in browser
open tests/web/genlock-dashboard-tests.html
open tests/web/resolution-preset-tests.html

# Or use a local server
cd tests/web
python3 -m http.server 8888
# Visit: http://localhost:8888/genlock-dashboard-tests.html
```

#### Integration Tests

```bash
# CI mode (non-interactive)
cd tests/integration
./test_ci.sh

# Full validation (requires NDI SDK)
./test_ndi_output.sh

# Python-based validation
python3 test_ndi_output.py
```

### Test Requirements Before Merge

✅ **Pre-Merge Checklist:**
- [ ] All C++ unit tests pass
- [ ] All Swift unit tests pass
- [ ] Web UI tests validated manually
- [ ] Integration tests pass in CI environment
- [ ] No new linter warnings
- [ ] Code coverage maintained or improved

---

## CI/CD Guidelines

### GitHub Actions Workflows

| Workflow | Trigger | Purpose |
|----------|---------|---------|
| `build.yml` | Push to `main` | Build and test on every commit |
| `release.yml` | Tag push (`v*`) | Build, sign, notarize, and release |

### CI/CD Best Practices

1. **Fail Fast**: Build should fail immediately on first error
2. **Clear Error Messages**: Compiler errors should be visible in logs
3. **Test Before Release**: Never skip test execution
4. **Version Verification**: CI should verify version consistency
5. **Compilation Coverage**: Ensure all code paths compile, not just used ones

### Common CI Failures and Fixes

| Error | Cause | Fix |
|-------|-------|-----|
| Incompatible pointer types | Type mismatch with NDI SDK | Verify mock headers match production SDK |
| Missing symbols | Unused code not compiled locally | Run full build before pushing |
| Test failures | Environment differences | Use mock SDK for consistent testing |
| Version mismatch | Forgot to update all locations | Use pre-release-check.sh script |

### Release Process

**Automated Release (Recommended):**

```bash
# 1. Ensure all tests pass
git push origin main

# 2. Wait for build.yml to complete successfully

# 3. Create and push version tag
git tag -a v1.X.Y -m "Release v1.X.Y"
git push origin v1.X.Y

# 4. GitHub Actions will automatically:
#    - Build the app
#    - Run all tests
#    - Code sign with Apple Developer ID
#    - Notarize with Apple
#    - Create DMG and ZIP
#    - Publish GitHub Release
```

**Manual Release (For Testing):**

```bash
# Build locally
cd manager
./build.sh

# Create DMG manually
hdiutil create -volname "HTML2NDI Manager" \
  -srcfolder build/"HTML2NDI Manager.app" \
  -ov -format UDZO \
  HTML2NDI-Manager-manual.dmg
```

### Release Artifacts

Each release produces:

| Artifact | Size | Use Case |
|----------|------|----------|
| `HTML2NDI-Manager-{version}-arm64.dmg` | ~150MB | User-friendly installer |
| `HTML2NDI-Manager-{version}-arm64.zip` | ~657MB | CI/CD, automation |

Both are **code signed** and **notarized** by Apple.

---

## Common Development Tasks

### Adding a New Feature

1. **Create feature branch** (optional for CI testing)
   ```bash
   git checkout -b feature/new-feature
   ```

2. **Implement the feature**
   - Follow code quality standards
   - Update relevant documentation in `/docs`

3. **Write tests**
   - Add C++ unit tests for worker logic
   - Add Swift tests for UI components
   - Add integration tests if needed

4. **Test locally**
   ```bash
   cd build && ninja test_runner && ./test_runner
   cd ../manager && swift test
   ```

5. **Run linter**
   ```bash
   # C++
   clang-format -i src/**/*.cpp include/**/*.h
   
   # Swift
   swift-format lint -r manager/
   ```

6. **Commit and push**
   ```bash
   git add .
   git commit -m "feat: Add new feature"
   git push origin main
   ```

### Fixing a Bug

1. **Identify the issue**
   - Check GitHub Issues
   - Review logs and error messages

2. **Write a failing test** that reproduces the bug

3. **Fix the bug** in the relevant file

4. **Verify the test now passes**

5. **Commit with descriptive message**
   ```bash
   git commit -m "fix: Resolve issue with NDI frame timing"
   ```

### Updating Documentation

1. **Add/update files in `/docs`** directory
2. **Keep `README.md` high-level** - link to detailed docs
3. **Update AGENTS.md** only if project architecture changes
4. **Follow markdown best practices**
   - Use headings hierarchically
   - Include code examples in fenced blocks
   - Add tables for structured data

---

## Troubleshooting

### Build Issues

**CEF not found:**
```bash
# Re-download CEF
cd build
rm -rf cef
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja
```

**NDI SDK not found:**
```bash
# Verify NDI SDK location
ls -la "/Library/NDI SDK for macOS/"

# If missing, download from:
# https://ndi.video/download-ndi-sdk/
```

### Test Failures

**Integration tests fail in CI:**
- Check `test_ci.sh` for environment-specific issues
- Verify network permissions are properly mocked
- Ensure no hanging processes from previous runs

**Swift tests fail:**
- Clean build: `swift package clean`
- Rebuild: `swift build`
- Run tests: `swift test`

### Release Issues

**Notarization fails:**
- Verify Apple Developer credentials in GitHub Secrets
- Check code signing certificate is valid
- Review notarization logs in GitHub Actions

**Version conflict:**
- Follow [Version Management](#version-management) checklist
- Delete conflicting tag: `git tag -d vX.Y.Z && git push origin :refs/tags/vX.Y.Z`
- Create corrected version

---

## Additional Resources

- [Security Audit](SECURITY_AUDIT.md) - Security considerations and hardening
- [Network Permissions](NETWORK_PERMISSIONS.md) - macOS network permissions guide
- [Genlock Implementation](genlock-implementation-summary.md) - Multi-stream synchronization
- [Network Interface Guide](NETWORK_INTERFACE_GUIDE.md) - Binding to specific interfaces

---

## Questions?

If you encounter issues not covered in this guide:
1. Check existing documentation in `/docs`
2. Review GitHub Issues and closed PRs
3. Consult the main project specification in `.cursorrules`
4. Ask the project maintainer

---

**Last Updated:** December 2025  
**Version:** 1.5.3-alpha





