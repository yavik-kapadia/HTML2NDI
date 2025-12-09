# HTML2NDI Security Hardening Checklist

Quick reference for securing your HTML2NDI deployment.

## 🔴 Critical - Implement Before Production

- [ ] **Add API Authentication**
  ```bash
  ./html2ndi --api-key "your-secret-key-here"
  # Require X-API-Key header in all HTTP requests
  ```

- [ ] **Validate URLs in /seturl**
  - Block `file://`, `ftp://`, and other non-HTTP(S) schemes
  - Block private IP ranges (127.0.0.1, 10.x.x.x, 192.168.x.x, etc.)
  - Block cloud metadata endpoints (169.254.169.254)

- [ ] **Use Secure HTTP Binding**
  ```bash
  # Default (localhost only - most secure)
  ./html2ndi --url http://example.com
  
  # Specific interface (broadcast VLAN - requires API key)
  ./html2ndi --http-host 192.168.100.50 --api-key "your-key"
  
  # NEVER use 0.0.0.0 without strong authentication
  ```

## 🟠 High Priority - Recommended

- [ ] **Restrict CORS Origins**
  ```cpp
  Access-Control-Allow-Origin: http://localhost:8080
  // NOT: Access-Control-Allow-Origin: *
  ```

- [ ] **Add Genlock Authentication**
  - Implement HMAC-SHA256 for UDP packets
  - Add `--genlock-key` option for shared secret

- [ ] **Input Validation**
  - Add try/catch for all `std::stoi()` calls
  - Validate width/height/fps ranges in Swift manager

## 🟡 Medium Priority - Enhanced Security

- [ ] **Validate Log File Paths**
  ```bash
  # Only allow logs in ~/Library/Logs/HTML2NDI/
  ./html2ndi --log-file /safe/path/only
  ```

- [ ] **Enable HTTPS**
  ```bash
  ./html2ndi --http-tls \
             --http-cert /path/to/cert.pem \
             --http-key /path/to/key.pem
  ```

- [ ] **Implement Rate Limiting**
  - Max 10 requests/second per IP
  - Prevent thumbnail DoS attacks

## 🟢 Low Priority - Best Practices

- [ ] **Code Signing & Notarization**
  ```bash
  codesign --sign "Developer ID" "HTML2NDI Manager.app"
  xcrun notarytool submit ...
  ```

- [ ] **Enable Hardened Runtime**
  - Update entitlements.plist
  - Disable unsigned executable memory

- [ ] **Dependency Scanning**
  ```bash
  brew install osv-scanner
  osv-scanner scan .
  ```

- [ ] **Security Tests**
  - Add tests/security/test_auth.cpp
  - Add tests/security/test_ssrf.cpp
  - Run OWASP ZAP before release

## Broadcast Network Deployment Checklist

For professional broadcast environments with dedicated network interfaces:

- [ ] **Identify Network Interfaces**
  ```bash
  ifconfig | grep inet
  # Note your broadcast interface IP (e.g., en7: 192.168.100.50)
  ```

- [ ] **Bind to Specific Interface (Not 0.0.0.0)**
  ```bash
  # Get your interface IP
  BROADCAST_IP=$(ifconfig en7 | grep "inet " | awk '{print $2}')
  
  # Bind to specific IP
  ./html2ndi --http-host $BROADCAST_IP --api-key "..."
  ```

- [ ] **Configure API Key for Network Access**
  ```bash
  # Generate and store in Keychain
  openssl rand -hex 32 | \
    security add-generic-password -a html2ndi -s api-key -w -
  
  # Retrieve for use
  API_KEY=$(security find-generic-password -a html2ndi -s api-key -w)
  ```

- [ ] **Verify Binding**
  ```bash
  lsof -i -P | grep html2ndi
  # Should show specific IP, not *:8080
  # Good: 192.168.100.50:8080
  # Bad:  *:8080
  ```

- [ ] **Test from Control Station**
  ```bash
  # From another machine on broadcast network
  curl http://192.168.100.50:8080/status \
    -H "X-API-Key: your-api-key"
  ```

- [ ] **Document Network Topology**
  - Interface name (en0, en1, en7, etc.)
  - IP address and subnet
  - VLAN ID (if applicable)
  - Allowed client IPs
  - Firewall rules

## Network Deployment Checklist

If deploying on untrusted networks:

- [ ] **Firewall Rules**
  ```bash
  # macOS Application Firewall
  /usr/libexec/ApplicationFirewall/socketfilterfw --add /path/to/html2ndi
  ```

- [ ] **VPN/Tunnel**
  - Use WireGuard or Tailscale for remote access
  - Never expose HTTP API to public internet

- [ ] **Network Segmentation**
  - Isolate HTML2NDI on dedicated VLAN
  - Restrict access to trusted IPs only

- [ ] **Monitor Access Logs**
  ```bash
  tail -f ~/Library/Logs/HTML2NDI/html2ndi.log | grep "HTTP API"
  ```

## Quick Security Verification

Run these commands to verify your current security posture:

```bash
# 1. Check HTTP binding
lsof -i -P | grep html2ndi
# Should show 127.0.0.1:8080, NOT *:8080

# 2. Check for authentication
curl http://localhost:8080/status
# Should return 401 if auth enabled

# 3. Test URL validation
curl -X POST http://localhost:8080/seturl \
  -H "Content-Type: application/json" \
  -d '{"url": "file:///etc/passwd"}'
# Should return 400 Bad Request

# 4. Test genlock security (if using genlock)
nmap -sU -p 5960 localhost
# Should be filtered or closed if not actively using genlock

# 5. Check open ports
sudo lsof -i -P | grep LISTEN | grep html2ndi
# Minimize exposed ports
```

## Production Deployment Configuration

Recommended production settings:

```bash
./html2ndi \
  --url "https://trusted-source.com/graphics.html" \
  --http-host 127.0.0.1 \
  --http-port 8080 \
  --api-key "$(openssl rand -hex 32)" \
  --log-file ~/Library/Logs/HTML2NDI/production.log \
  --genlock slave \
  --genlock-master 192.168.1.10:5960 \
  --genlock-key "$(openssl rand -hex 32)" \
  --ndi-name "Production Graphics" \
  --cache-path ~/Library/Caches/HTML2NDI/production
```

Save API key and genlock key to macOS Keychain:

```bash
security add-generic-password \
  -a html2ndi \
  -s html2ndi-api-key \
  -w "your-api-key-here"

security add-generic-password \
  -a html2ndi \
  -s html2ndi-genlock-key \
  -w "your-genlock-key-here"
```

## Incident Response

If you suspect a security breach:

1. **Immediately stop all streams**
   ```bash
   curl -X POST http://localhost:8080/shutdown
   # Or kill via Activity Monitor
   ```

2. **Check logs for suspicious activity**
   ```bash
   grep -E "seturl|HTTP API" ~/Library/Logs/HTML2NDI/*.log
   ```

3. **Review recent URL changes**
   ```bash
   grep "seturl to" ~/Library/Logs/HTML2NDI/*.log
   ```

4. **Rotate API keys**
   ```bash
   openssl rand -hex 32 > new-api-key.txt
   ```

5. **Update firewall rules**
   ```bash
   /usr/libexec/ApplicationFirewall/socketfilterfw --blockapp /path/to/html2ndi
   # Then re-enable after securing
   ```

## Security Contact

For security vulnerabilities, please report to:
- **Email:** security@example.com (replace with your contact)
- **PGP Key:** (if applicable)

**Do not** open public GitHub issues for security vulnerabilities.

---

**Last Updated:** December 7, 2025  
**Version:** 1.5.0-alpha

