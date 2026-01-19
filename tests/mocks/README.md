# NDI SDK Mock Infrastructure

This directory contains mock implementations of the NDI SDK for local testing and development.

## Purpose

The mock NDI SDK serves several critical purposes:

1. **Local Development**: Develop and test without installing the full NDI SDK
2. **Unit Testing**: Run unit tests in environments where NDI SDK is unavailable
3. **Type Safety**: Catch API mismatches between our code and the real NDI SDK
4. **CI/CD**: Faster builds in CI environments (when using mocks)

## Files

- `Processing.NDI.Lib.h` - Mock NDI SDK header matching production API
- `ndi_mock.cpp` - Stub implementations of NDI functions
- `README.md` - This file

## Important: API Compatibility

**CRITICAL**: The mock headers MUST match the production NDI SDK API exactly.

### Example of Type Mismatch Bug

In v1.6.0, we had this bug:

```cpp
// Our code
audio_frame_.p_data = const_cast<float*>(data);

// Production NDI SDK expects
audio_frame_.p_data = reinterpret_cast<uint8_t*>(const_cast<float*>(data));
```

The mock header had `float* p_data` but the real SDK has `uint8_t* p_data`. This compiled locally but failed in CI, causing a release to fail.

### Prevention

To prevent similar issues:

1. **Keep mocks synchronized** with production NDI SDK
2. **Run pre-release-check.sh** before every release
3. **Test compilation** of all code paths, not just used ones
4. **Review NDI SDK updates** and update mocks accordingly

## Usage

### For Local Development

```bash
# Build with mock NDI SDK
cd build
cmake .. -DUSE_MOCK_NDI=ON
ninja
```

### For Testing

```bash
# Run tests with mock
cd build
./html2ndi_tests
```

### Production Builds

Production builds always use the real NDI SDK from `/Library/NDI SDK for Apple/`.

## Updating Mocks

When the NDI SDK is updated:

1. Check release notes for API changes
2. Update `Processing.NDI.Lib.h` to match new API signatures
3. Update `ndi_mock.cpp` stub implementations if needed
4. Run all tests to ensure compatibility
5. Update this README with any breaking changes

## Known Limitations

The mock SDK:
- Does NOT send actual NDI frames
- Does NOT discover NDI sources on the network
- Does NOT provide actual tally information
- Returns stub/dummy data for all queries

For full NDI functionality testing, use the production SDK.

## Version History

- **v1.6.1**: Added mock infrastructure, fixed audio frame type mismatch
- **v1.6.0**: No mocks, build failure due to type mismatch

## References

- Production NDI SDK: https://ndi.video/download-ndi-sdk/
- NDI SDK Documentation: https://docs.ndi.video/
