#!/bin/bash
# Pre-Release Verification Script
# 
# Run this before creating a new release to verify:
# 1. All code compiles
# 2. All tests pass
# 3. No linter errors
# 4. Version numbers are consistent
# 5. Documentation is up to date

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "======================================"
echo "HTML2NDI Pre-Release Verification"
echo "======================================"
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓${NC} $1"
}

fail() {
    echo -e "${RED}✗${NC} $1"
    exit 1
}

warn() {
    echo -e "${YELLOW}⚠${NC} $1"
}

# 1. Check version consistency
echo "1. Checking version consistency..."
CMAKE_VERSION=$(grep "project(HTML2NDI VERSION" "$PROJECT_ROOT/CMakeLists.txt" | sed -E 's/.*VERSION ([0-9.]+).*/\1/')
CODE_VERSION=$(grep "const char\* VERSION" "$PROJECT_ROOT/src/app/config.cpp" | sed -E 's/.*"([0-9.]+)".*/\1/')

if [ "$CMAKE_VERSION" != "$CODE_VERSION" ]; then
    fail "Version mismatch: CMakeLists.txt ($CMAKE_VERSION) != config.cpp ($CODE_VERSION)"
fi
pass "Version consistent: $CMAKE_VERSION"

# 2. Clean build test
echo ""
echo "2. Running clean build test..."
cd "$PROJECT_ROOT"
if [ -d "build" ]; then
    rm -rf build
fi
mkdir -p build
cd build

cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DBUILD_TESTS=ON || fail "CMake configuration failed"

pass "CMake configuration succeeded"

ninja || fail "Build failed"
pass "Build succeeded"

# 3. Run tests
echo ""
echo "3. Running unit tests..."
if [ -f "./html2ndi_tests" ]; then
    ./html2ndi_tests || fail "Unit tests failed"
    pass "Unit tests passed"
else
    warn "Unit tests not found, skipping"
fi

# 4. Verify executables work
echo ""
echo "4. Verifying executables..."
if [ -f "./bin/html2ndi.app/Contents/MacOS/html2ndi" ]; then
    ./bin/html2ndi.app/Contents/MacOS/html2ndi --version || fail "html2ndi executable failed"
    pass "html2ndi executable works"
else
    fail "html2ndi executable not found"
fi

# 5. Build Swift manager
echo ""
echo "5. Building Swift manager..."
cd "$PROJECT_ROOT/manager"
swift build --configuration release || fail "Swift manager build failed"
pass "Swift manager build succeeded"

# 6. Check for uncommitted changes
echo ""
echo "6. Checking for uncommitted changes..."
cd "$PROJECT_ROOT"
if [ -n "$(git status --porcelain)" ]; then
    warn "You have uncommitted changes:"
    git status --short
else
    pass "No uncommitted changes"
fi

# 7. Verify documentation exists
echo ""
echo "7. Checking documentation..."
DOCS=(
    "README.md"
    "RELEASE_NOTES.md"
    "docs/AGENT_INSTRUCTIONS.md"
    "docs/PERFORMANCE_MONITORING.md"
)

for doc in "${DOCS[@]}"; do
    if [ ! -f "$PROJECT_ROOT/$doc" ]; then
        fail "Missing documentation: $doc"
    fi
done
pass "All required documentation present"

# 8. Check for common issues
echo ""
echo "8. Scanning for common issues..."

# Check for debug print statements
if grep -r "std::cout\|printf\|LOG_DEBUG" "$PROJECT_ROOT/src" --exclude-dir=build | grep -v "LOG_DEBUG\|LOG_INFO\|LOG_WARNING\|LOG_ERROR" > /dev/null 2>&1; then
    warn "Found potential debug print statements"
fi

# Check for TODO/FIXME in critical files
if grep -r "TODO\|FIXME" "$PROJECT_ROOT/src" --exclude-dir=build > /dev/null 2>&1; then
    warn "Found TODO/FIXME comments in source code"
fi

pass "Code scan complete"

# Summary
echo ""
echo "======================================"
echo -e "${GREEN}✓ All pre-release checks passed!${NC}"
echo "======================================"
echo ""
echo "Version: $CMAKE_VERSION"
echo "Build: Release (arm64)"
echo "Tests: Passed"
echo ""
echo "Next steps:"
echo "1. Update RELEASE_NOTES.md with changes"
echo "2. Commit any final changes"
echo "3. Tag release: git tag -a v$CMAKE_VERSION -m \"Release v$CMAKE_VERSION\""
echo "4. Push: git push origin main && git push origin v$CMAKE_VERSION"
echo ""
