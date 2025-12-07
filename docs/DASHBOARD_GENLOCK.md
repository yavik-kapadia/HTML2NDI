# Dashboard Genlock Feature - Summary

## What Was Added

Added comprehensive genlock monitoring to the HTML2NDI web control panel.

## Changes Made

### 1. Dashboard UI Enhancement

**File**: `src/http/http_server.cpp`

Added new **Genlock card** to the control panel grid with:

```html
<div class="card">
  <div class="card-h">
    <div class="card-i">G</div>
    <div class="card-t">Genlock</div>
  </div>
  <!-- 4 stat boxes -->
  <div class="stats">
    - Mode (Master/Slave/Disabled)
    - Status (Synced/Not Synced)
    - Offset (microseconds)
    - Jitter (microseconds)
  </div>
  <!-- Informational note -->
</div>
```

### 2. Real-Time Data Updates

**JavaScript Function**: `updGenlock(gl, locked)`

Features:
- Parses genlock data from `/status` endpoint
- Updates all four metrics
- Applies color coding based on thresholds
- Handles all modes (disabled/master/slave)
- Updates every 2 seconds automatically

### 3. Color-Coded Quality Indicators

**Offset Colors**:
- Green: `< 1000μs` (excellent)
- Yellow: `1000-5000μs` (acceptable)  
- Red: `> 5000μs` (poor)

**Jitter Colors**:
- Green: `< 500μs` (stable)
- Yellow: `500-1000μs` (acceptable)
- Red: `> 1000μs` (unstable)

**Mode Colors**:
- Master: Accent (purple/blue)
- Slave: Green
- Disabled: Gray

**Status Colors**:
- Synced: Green
- Not Synced: Red

### 4. Documentation

Created three comprehensive guides:

1. **genlock.md** (563 lines)
   - Complete user guide
   - Configuration, monitoring, troubleshooting
   - API reference and examples

2. **genlock-dashboard-guide.md** (NEW)
   - Dashboard-specific guide
   - Visual examples of all states
   - Interpretation guidelines
   - Monitoring best practices

3. **genlock-implementation-summary.md**
   - Technical implementation details
   - Architecture overview
   - Files modified/created

## User Experience

### Before
- No genlock visibility in dashboard
- Had to use curl/API calls to check sync
- No visual feedback on sync quality

### After
- Real-time genlock monitoring in main dashboard
- Color-coded quality indicators at a glance
- Clear status messages (Synced/Not Synced)
- Automatic refresh every 2 seconds
- Works alongside other stream controls

## Visual Layout

The dashboard now includes 5 cards:

```
┌─────────────┬─────────────┬─────────────┐
│   Status    │ Source URL  │ Color Space │
├─────────────┼─────────────┼─────────────┤
│   Genlock   │   System    │             │
└─────────────┴─────────────┴─────────────┘
```

## Example Display States

### Master Mode
```
Mode:   Master (purple)
Status: N/A
Offset: N/A
Jitter: N/A
```

### Healthy Slave
```
Mode:   Slave (green)
Status: Synced (green)
Offset: -125μs (green)
Jitter: 85.3μs (green)
```

### Problematic Slave
```
Mode:   Slave (gray)
Status: Not Synced (red)
Offset: N/A
Jitter: N/A
```

## API Integration

Dashboard consumes data from:

1. **GET /status** - Main status endpoint
   ```json
   {
     "genlock": {
       "mode": "slave",
       "synchronized": true,
       "offset_us": -125,
       "stats": {
         "jitter_us": 85.3
       }
     },
     "genlocked": true
   }
   ```

2. Auto-refresh every 2 seconds
3. Updates all metrics simultaneously
4. No user interaction required

## Technical Details

### JavaScript Implementation

```javascript
function updGenlock(gl, locked) {
  // Handle disabled/missing genlock
  if (!gl) {
    // Set all to N/A
  }
  
  // Update mode with color coding
  const mode = gl.mode || 'disabled';
  // Master = accent, Slave = green, Disabled = gray
  
  // Update sync status
  const synced = gl.synchronized || false;
  // Synced = green, Not Synced = red
  
  // Update offset (slave only)
  if (mode === 'slave' && gl.offset_us !== undefined) {
    // Color: green < 1000, yellow 1000-5000, red > 5000
  }
  
  // Update jitter (slave only)
  if (gl.stats?.jitter_us !== undefined) {
    // Color: green < 500, yellow 500-1000, red > 1000
  }
}
```

### CSS Styling

Uses existing design system:
- `--ok`: Green (#22c55e)
- `--err`: Red (#ef4444)
- `--accent`: Purple/blue (#6366f1)
- `--dim`: Gray (#71717a)
- Warning: Amber (#f59e0b)

### Performance

- No performance impact (uses existing refresh cycle)
- Lightweight JavaScript (< 50 lines)
- No additional HTTP requests
- Data already included in `/status` response

## Browser Compatibility

Works on:
- Chrome/Edge (modern)
- Firefox (modern)
- Safari (macOS/iOS)
- Mobile browsers

No special requirements:
- Modern JavaScript (ES6+)
- Standard DOM API
- CSS3 for colors

## Accessibility

- Text labels for all metrics
- Color supplemented with text ("Synced"/"Not Synced")
- Semantic HTML structure
- Works without JavaScript (shows static content)
- Mobile responsive

## Testing Checklist

- [x] Disabled mode displays correctly
- [x] Master mode shows purple color
- [x] Slave mode shows sync status
- [x] Offset color codes correctly
- [x] Jitter color codes correctly
- [x] Auto-refresh works
- [x] Mobile layout responsive
- [x] Documentation complete

## Future Enhancements

Potential additions:
- Graphical offset/jitter trends
- Historical sync quality chart
- Alert notifications for sync loss
- Configurable thresholds
- Export sync quality metrics

## Files Modified

1. `src/http/http_server.cpp`
   - Added Genlock card HTML
   - Added `updGenlock()` JavaScript function
   - Updated `stat()` to call `updGenlock()`

2. `docs/genlock.md`
   - Added dashboard monitoring section
   - Added visual examples
   - Added quick workflow

3. `docs/genlock-dashboard-guide.md` (NEW)
   - Comprehensive dashboard guide
   - Visual examples of all states
   - Troubleshooting using dashboard

4. `docs/genlock-implementation-summary.md`
   - Updated to document dashboard
   - Added dashboard display section

## User Benefits

1. **Immediate Visibility**: See genlock status at a glance
2. **No CLI Required**: Monitor without terminal/curl
3. **Visual Feedback**: Colors indicate quality instantly
4. **Production Ready**: Real-time monitoring during live use
5. **Multi-Stream**: Open multiple dashboards to monitor all streams

## Conclusion

The genlock dashboard integration provides professional-grade monitoring with:
- Real-time status updates
- Color-coded quality indicators
- Clear visual feedback
- Zero configuration required
- Seamless integration with existing UI

Users can now monitor genlock synchronization quality directly in the web interface without any additional tools or commands.

