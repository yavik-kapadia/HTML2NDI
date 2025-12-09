# Web Dashboard Tests

Automated tests for the HTML2NDI dashboard UI, genlock controls, and resolution/framerate presets.

## Running Tests

### Genlock Dashboard Tests

```bash
# Open in browser
open tests/web/genlock-dashboard-tests.html
```

### Resolution & Framerate Preset Tests

```bash
# Open in browser
open tests/web/resolution-preset-tests.html
```

The test suites will automatically run and display results.

### Test Categories

#### Genlock Tests

1. **API Integration Tests**
   - Mode switching (disabled/master/slave)
   - Master address configuration
   - Status retrieval
   - Error handling

2. **UI State Tests**
   - Mode selector updates
   - Master address field visibility
   - Color-coded indicators
   - Real-time status display

3. **Validation Tests**
   - Invalid mode rejection
   - Master address format validation
   - Mode transition sequences

4. **Metrics Tests**
   - Offset calculations
   - Jitter thresholds
   - Sync status tracking
   - Packet statistics

#### Resolution & Framerate Tests

1. **Standard Presets**
   - 7 standard resolutions (4K, QHD, 1080p, 720p, XGA, FWVGA, SD)
   - 5 standard framerates (24, 25, 30, 50, 60 fps)
   - Progressive/interlaced scan modes

2. **Format Validation**
   - Resolution parsing (widthxheight)
   - Aspect ratio calculations (16:9, 4:3)
   - Format identifier generation (1080p60, 720i50, etc.)

3. **API Integration**
   - Progressive boolean field
   - Stream data format validation
   - Request format verification

4. **UI Components**
   - Dropdown value parsing
   - Format preview generation
   - Scan mode conversion (p/i)

## Test Framework

Uses a custom lightweight test framework with:
- Mock API responses
- Assertion helpers
- Visual test results
- Pass/fail statistics

## Expected Results

All tests should pass:
- ✅ Genlock tests: 15+ tests
- ✅ Resolution/framerate tests: 14+ tests
- ✅ 100% pass rate
- ✅ < 1 second execution time per suite

## Manual Testing

1. **Master Mode:**
   - Select "Master" from dropdown
   - Click "Apply"
   - Verify status shows "Mode: Master"

2. **Slave Mode:**
   - Select "Slave" from dropdown
   - Enter master address (e.g., 192.168.100.10:5960)
   - Click "Apply"
   - Verify offset and jitter displays

3. **Disabled Mode:**
   - Select "Disabled"
   - Click "Apply"
   - Verify all metrics show "N/A"

## Integration with Worker

These tests mock the worker API responses. For end-to-end testing:

```bash
# Start worker
./html2ndi --url http://test.com --http-port 8080

# Open control panel
open http://localhost:8080

# Test genlock controls manually
```

## CI/CD

Tests can be run headlessly using Playwright or Puppeteer:

```bash
# Example with Playwright
npm install playwright
npx playwright test tests/web/genlock-dashboard-tests.html
```

