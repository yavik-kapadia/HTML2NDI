# Network Permissions Guide

HTML2NDI requires local network access to function properly. This guide explains why these permissions are needed and how to grant them.

## Why Network Permissions Are Required

HTML2NDI uses your local network for:

1. **NDI Video Streaming** - Transmitting high-quality video over your network
2. **NDI Discovery** - Finding and advertising NDI sources using mDNS/Bonjour
3. **HTTP Control API** - Communication between the Manager and worker processes
4. **Web Dashboard** - Serving the web-based control interface

## macOS Permissions

### First Launch

When you first launch HTML2NDI Manager, macOS will prompt you with:

```
"HTML2NDI Manager" would like to find and connect
to devices on your local network.
```

**✅ You must click "Allow"** for the app to work properly.

### If You Accidentally Denied Permission

If you clicked "Don't Allow" or want to change the permission later:

1. Open **System Settings**
2. Go to **Privacy & Security**
3. Scroll down and click **Local Network**
4. Find **HTML2NDI Manager** in the list
5. Toggle the switch to **ON** (green)

Or click the "Open System Settings" button in the permission alert.

### Permission Prompt Shortcut

The app includes a convenient button to take you directly to the correct settings page:
- Click "Fix" in the network status banner
- Or use the alert dialog's "Open System Settings" button

## What Happens Without Permission?

If local network permission is denied:

❌ **NDI streams will not work** - No video output  
❌ **NDI discovery will fail** - Sources won't appear on the network  
❌ **Worker communication fails** - Can't control streams  
❌ **Web dashboard inaccessible** - HTTP server can't bind to port  

## Firewall Settings

### macOS Built-in Firewall

If you have the macOS firewall enabled:

1. Go to **System Settings** → **Network** → **Firewall**
2. Click **Options**
3. Ensure **HTML2NDI Manager** is set to **Allow incoming connections**
4. Same for **html2ndi** (the worker process)

### Third-Party Firewalls

If using third-party firewall software (Little Snitch, Lulu, etc.):

Allow these connections:
- **HTML2NDI Manager** → All local network addresses
- **html2ndi** → All local network addresses
- **UDP Port 5960** → NDI discovery (mDNS)
- **TCP Port 8080** → Web dashboard (or your configured port)
- **TCP Ports 8099+** → Worker HTTP APIs (configurable)

## NDI-Specific Requirements

### Bonjour Services

HTML2NDI advertises these Bonjour services:
- `_ndi._tcp` - NDI source discovery
- `_http._tcp` - HTTP control API

These must be allowed for NDI to function properly.

### Network Ports

| Port | Protocol | Purpose | Required |
|------|----------|---------|----------|
| 5960 | UDP | NDI Discovery | Yes |
| 5961+ | TCP/UDP | NDI Streaming | Yes |
| 8080 | TCP | Web Dashboard | Configurable |
| 8099+ | TCP | Worker HTTP APIs | Configurable |

## Troubleshooting

### "Network Permission Denied" Warning

If you see this warning in the dashboard:

1. Check System Settings → Privacy & Security → Local Network
2. Ensure HTML2NDI Manager is enabled
3. Restart the application
4. If still not working, try the steps below

### NDI Sources Not Appearing

1. **Check Firewall**
   ```bash
   # Test if NDI discovery port is accessible
   nc -uz localhost 5960
   ```

2. **Verify Bonjour**
   ```bash
   # Check for NDI services on your network
   dns-sd -B _ndi._tcp
   ```

3. **Check Network Settings**
   - Ensure you're on the same network/VLAN as other NDI devices
   - Some corporate networks block multicast (required for NDI)

### Workers Can't Start

If workers fail with "Address already in use":

```bash
# Check what's using the port
lsof -i :8099
```

Kill the conflicting process or configure a different port.

### Permission Prompt Not Appearing

If macOS doesn't show the permission prompt:

1. Delete the permission database:
   ```bash
   tccutil reset NetworkExtension
   ```

2. Restart your Mac

3. Launch HTML2NDI Manager again

## Security Considerations

### Local Network Only

HTML2NDI is designed for **local network use only**:
- No internet access required
- No external connections made
- All communication stays within your local network

### Broadcast vs. Targeted Binding

For enhanced security, you can configure workers to bind to a specific network interface:

```bash
# Bind to specific interface only
./html2ndi.app/Contents/MacOS/html2ndi \
  --url "https://example.com" \
  --bind-address "192.168.1.100" \
  --http-port 8099
```

See [SECURITY_AUDIT.md](SECURITY_AUDIT.md) for more details.

## Testing Network Access

### Quick Test

Run this command to verify local network access:

```bash
# Test if you can bind to a local port
nc -l 5960
```

If this fails, you likely don't have local network permission.

### Full Network Test

Use the built-in integration tests:

```bash
cd /path/to/HTML2NDI
./tests/integration/test_ci.sh
```

This will verify:
- Worker can start
- HTTP API is accessible
- Network binding works
- Configuration is valid

## Getting Help

If you continue to have network permission issues:

1. Check the logs:
   ```bash
   tail -f ~/Library/Logs/HTML2NDI/*.log
   ```

2. Run with verbose logging:
   ```bash
   HTML2NDI_LOG_LEVEL=debug ./html2ndi.app/Contents/MacOS/html2ndi ...
   ```

3. Open an issue on GitHub with:
   - macOS version
   - Firewall settings
   - Error messages from logs
   - Output of `ifconfig` (to see your network setup)

## Summary

✅ **Grant local network permission when prompted**  
✅ **Allow HTML2NDI in your firewall**  
✅ **Use the built-in permission checker**  
✅ **Keep security in mind for broadcast networks**  

With proper permissions, HTML2NDI will work seamlessly on your local network!

