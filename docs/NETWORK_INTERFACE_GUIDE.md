# Network Interface Binding Guide

Quick reference for binding HTML2NDI to specific network interfaces in broadcast environments.

## Understanding Interface Binding

### Default Behavior
```bash
# Current default (binds to all interfaces - will be changed to 127.0.0.1)
./html2ndi --url http://example.com
# Accessible from: ALL network interfaces
```

### Secure Defaults (Recommended for v1.5.0+)
```bash
# Future default (localhost only)
./html2ndi --url http://example.com
# Accessible from: 127.0.0.1 (localhost) only
```

---

## Common Deployment Scenarios

### Scenario 1: Single Workstation (Default)

**Use Case:** Running HTML2NDI on local machine, no network access needed

```bash
./html2ndi \
  --url "https://graphics.local/overlay.html" \
  --ndi-name "Graphics 1"
# Uses default: --http-host 127.0.0.1
```

**Access:** Dashboard at http://localhost:8080  
**Security:** ✅ Maximum (localhost only)

---

### Scenario 2: Dedicated Broadcast Interface

**Use Case:** MacBook Pro with Thunderbolt Ethernet adapter on broadcast VLAN

#### Step 1: Identify Your Interface
```bash
ifconfig | grep -A 3 "^en"

# Example output:
# en0: flags=... (Wi-Fi)
#     inet 192.168.1.100 netmask 0xffffff00
# en7: flags=... (Thunderbolt Ethernet)
#     inet 192.168.100.50 netmask 0xffffff00  ← Your broadcast interface
```

#### Step 2: Bind to Specific IP
```bash
./html2ndi \
  --url "https://graphics.local/overlay.html" \
  --ndi-name "Graphics 1" \
  --http-host 192.168.100.50 \
  --http-port 8080 \
  --api-key "your-api-key-here"
```

#### Step 3: Verify
```bash
lsof -i -P | grep html2ndi
# Should show:
# html2ndi ... TCP 192.168.100.50:8080 (LISTEN)
```

**Access:** Dashboard at http://192.168.100.50:8080  
**Security:** ⚠️ Network accessible - requires API key

---

### Scenario 3: Multiple Interfaces (Split Control)

**Use Case:** Control panel on localhost, NDI on broadcast network

```bash
# Worker 1: Broadcast interface with auth
./html2ndi \
  --url "https://graphics1.local" \
  --ndi-name "Camera 1" \
  --http-host 192.168.100.50 \
  --http-port 8081 \
  --api-key "$API_KEY_1"

# Worker 2: Localhost for local control
./html2ndi \
  --url "https://graphics2.local" \
  --ndi-name "Camera 2" \
  --http-host 127.0.0.1 \
  --http-port 8082
```

**Access:**
- Worker 1: http://192.168.100.50:8081 (network)
- Worker 2: http://localhost:8082 (local only)

---

### Scenario 4: All Interfaces (Not Recommended)

**Use Case:** Testing or development only

```bash
./html2ndi \
  --url "https://example.com" \
  --http-host 0.0.0.0 \
  --api-key "required-for-all-interfaces"
```

**Security:** 🔴 Exposed on ALL interfaces  
**Warning:** Use only for testing, never in production without strong auth

---

## Network Interface Cheat Sheet

### macOS Interface Naming

| Interface | Typical Use | Example IP |
|-----------|-------------|------------|
| `lo0` | Loopback (localhost) | 127.0.0.1 |
| `en0` | Built-in Ethernet or Wi-Fi | 192.168.1.x |
| `en1` | Secondary adapter | 10.0.0.x |
| `en7-en9` | Thunderbolt/USB adapters | 192.168.100.x |
| `bridge0` | Virtual bridge | - |

### Finding Your Broadcast Interface

```bash
# List all interfaces with IPs
ifconfig | grep -E "^[a-z]|inet " | grep -B1 "192.168.100"

# Get IP of specific interface
ifconfig en7 | grep "inet " | awk '{print $2}'

# Show routing table (which interface goes where)
netstat -rn | grep default
```

---

## API Key Setup

### Generate Secure Key
```bash
# Generate 256-bit key
openssl rand -hex 32

# Or use UUID
uuidgen | tr -d '-' | tr '[:upper:]' '[:lower:]'
```

### Store in macOS Keychain
```bash
# Store
security add-generic-password \
  -a html2ndi \
  -s html2ndi-api-key \
  -w "your-generated-key-here" \
  -T /path/to/html2ndi

# Retrieve
security find-generic-password \
  -a html2ndi \
  -s html2ndi-api-key \
  -w
```

### Use in Script
```bash
#!/bin/bash

# Retrieve API key from Keychain
API_KEY=$(security find-generic-password -a html2ndi -s html2ndi-api-key -w)

# Get broadcast interface IP
BROADCAST_IP=$(ifconfig en7 | grep "inet " | awk '{print $2}')

# Start HTML2NDI
./html2ndi \
  --url "https://graphics.local/overlay.html" \
  --ndi-name "Production Graphics" \
  --http-host "$BROADCAST_IP" \
  --api-key "$API_KEY"
```

---

## Firewall Configuration

### macOS Application Firewall

```bash
# Enable Application Firewall
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --setglobalstate on

# Add html2ndi (allows incoming connections)
sudo /usr/libexec/ApplicationFirewall/socketfilterfw \
  --add /path/to/html2ndi.app/Contents/MacOS/html2ndi

# Block by default, then manually allow in System Settings
sudo /usr/libexec/ApplicationFirewall/socketfilterfw \
  --blockapp /path/to/html2ndi.app/Contents/MacOS/html2ndi
```

Then: System Settings > Network > Firewall > Options > Allow specific IPs

### pf (Packet Filter)

Create `/etc/pf.anchors/html2ndi`:

```bash
# Allow HTTP API only from broadcast VLAN
pass in quick on en7 proto tcp from 192.168.100.0/24 to any port 8080
block in quick on en7 proto tcp to any port 8080

# Block from all other interfaces
block in quick proto tcp to any port 8080
```

Load rules:
```bash
sudo pfctl -f /etc/pf.anchors/html2ndi
sudo pfctl -e  # Enable pf
```

---

## Testing & Verification

### Test Binding
```bash
# Start html2ndi on specific interface
./html2ndi --http-host 192.168.100.50 --http-port 8080 &

# Check what it's listening on
lsof -i -P | grep html2ndi
# Expected: TCP 192.168.100.50:8080 (LISTEN)
# Bad:      TCP *:8080 (LISTEN)

# Test from same machine
curl http://192.168.100.50:8080/status
# Should work

curl http://localhost:8080/status
# Should fail (not bound to localhost)

# Kill background process
killall html2ndi
```

### Test API Key
```bash
# Without key (should fail)
curl http://192.168.100.50:8080/status
# Expected: 401 Unauthorized (when auth is implemented)

# With key (should succeed)
curl http://192.168.100.50:8080/status \
  -H "X-API-Key: your-api-key-here"
# Expected: 200 OK
```

### Test from Remote Machine
```bash
# From another machine on broadcast network
ping 192.168.100.50  # Ensure network connectivity

curl http://192.168.100.50:8080/status \
  -H "X-API-Key: your-api-key-here"
```

---

## Troubleshooting

### "Address already in use"
```bash
# Find what's using the port
lsof -i :8080

# Kill the process
kill -9 <PID>

# Or use different port
./html2ndi --http-port 8081
```

### "Cannot bind to interface"
```bash
# Verify IP exists
ifconfig | grep "192.168.100.50"

# Check if you have permission
# On macOS, binding to specific IPs doesn't require sudo for ports > 1024

# Try binding to 0.0.0.0 to test
./html2ndi --http-host 0.0.0.0
```

### "Connection refused from remote machine"
```bash
# 1. Verify binding
lsof -i -P | grep html2ndi

# 2. Test from local machine first
curl http://192.168.100.50:8080/status

# 3. Check firewall
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate

# 4. Disable firewall temporarily to test
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --setglobalstate off
# (Remember to re-enable!)

# 5. Check network connectivity
ping 192.168.100.50
```

### "API key not working"
```bash
# Verify key in Keychain
security find-generic-password -a html2ndi -s html2ndi-api-key -w

# Test exact key
curl http://192.168.100.50:8080/status \
  -H "X-API-Key: $(security find-generic-password -a html2ndi -s html2ndi-api-key -w)"

# Check for whitespace
echo -n "your-key" | xxd  # Should not show \n or \r
```

---

## Production Launch Script

Save as `launch-html2ndi.sh`:

```bash
#!/bin/bash
set -e

# Configuration
BROADCAST_INTERFACE="en7"
HTTP_PORT="8080"
URL="https://graphics.internal/overlay.html"
NDI_NAME="Production Graphics"

# Get interface IP
BROADCAST_IP=$(ifconfig "$BROADCAST_INTERFACE" | grep "inet " | awk '{print $2}')

if [ -z "$BROADCAST_IP" ]; then
    echo "Error: Interface $BROADCAST_INTERFACE has no IP address"
    exit 1
fi

# Get API key from Keychain
API_KEY=$(security find-generic-password -a html2ndi -s html2ndi-api-key -w 2>/dev/null)

if [ -z "$API_KEY" ]; then
    echo "Error: API key not found in Keychain"
    echo "Run: openssl rand -hex 32 | security add-generic-password -a html2ndi -s html2ndi-api-key -w -"
    exit 1
fi

# Find worker binary
WORKER="./build/bin/html2ndi.app/Contents/MacOS/html2ndi"

if [ ! -f "$WORKER" ]; then
    echo "Error: Worker not found at $WORKER"
    exit 1
fi

# Start worker
echo "Starting HTML2NDI on $BROADCAST_IP:$HTTP_PORT"
"$WORKER" \
    --url "$URL" \
    --ndi-name "$NDI_NAME" \
    --http-host "$BROADCAST_IP" \
    --http-port "$HTTP_PORT" \
    --api-key "$API_KEY" \
    --log-file ~/Library/Logs/HTML2NDI/production.log

echo "HTML2NDI started successfully"
echo "Dashboard: http://$BROADCAST_IP:$HTTP_PORT"
```

Make executable and run:
```bash
chmod +x launch-html2ndi.sh
./launch-html2ndi.sh
```

---

## Security Best Practices Summary

| Binding | Security | Use Case | API Key |
|---------|----------|----------|---------|
| `127.0.0.1` | ✅ Maximum | Local workstation | Optional |
| Specific IP (e.g., `192.168.100.50`) | ⚠️ Medium | Broadcast network | **Required** |
| `0.0.0.0` | 🔴 Low | Testing only | **Required** |

### Golden Rules

1. **Default to localhost** - Only bind to network when necessary
2. **Always use API keys** for network binding
3. **Bind to specific IPs** - Never use `0.0.0.0` in production
4. **Use dedicated interfaces** - Separate broadcast and management networks
5. **Monitor access logs** - Watch for unauthorized access attempts
6. **Rotate keys regularly** - Change API keys every 90 days

---

**Last Updated:** December 7, 2025  
**Version:** 1.5.0-alpha

