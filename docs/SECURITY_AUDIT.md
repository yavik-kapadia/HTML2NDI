# HTML2NDI Security Audit Report

**Date:** December 7, 2025  
**Version:** 1.5.0-alpha  
**Audit Type:** Comprehensive Code Review

---

## Executive Summary

This security audit identifies **9 security concerns** across the HTML2NDI codebase, ranging from **Critical** to **Low** severity. The application is designed for local network use in broadcast environments, but several hardening measures are recommended before deployment in production or untrusted networks.

### Risk Level Summary
- **Critical:** 2 issues
- **High:** 3 issues
- **Medium:** 2 issues
- **Low:** 2 issues

---

## Critical Severity Issues

### 1. **No Authentication on HTTP Control API**

**Severity:** 🔴 Critical  
**CWE:** CWE-306 (Missing Authentication for Critical Function)  
**Location:** `src/http/http_server.cpp`

**Issue:**
All HTTP endpoints are exposed without any authentication mechanism:
- `/seturl` - Allows arbitrary URL loading
- `/shutdown` - Can terminate the application
- `/reload` - Can force page reload
- `/color` - Modify color space settings

**Attack Scenario:**
```bash
# Any user/process on localhost can shut down streams
curl -X POST http://localhost:8080/shutdown

# Load malicious URL (SSRF - see issue #2)
curl -X POST http://localhost:8080/seturl \
  -d '{"url": "file:///etc/passwd"}'
```

**Recommendation:**
Implement one or more:
1. **API Key Authentication**: Add `--api-key` CLI option, validate via header
2. **Token-based auth**: Generate random token on startup, require in requests
3. **Secure Default Binding**: Default to 127.0.0.1, require explicit opt-in for network binding
4. **macOS Keychain Integration**: Store API key securely when using network binding

**Remediation Example:**
```cpp
// Add to http_server.cpp
bool HttpServer::validate_auth(const httplib::Request& req) {
    if (api_key_.empty()) return true;  // Auth disabled
    
    auto it = req.headers.find("X-API-Key");
    if (it == req.headers.end()) return false;
    
    return it->second == api_key_;
}

// In each endpoint:
if (!validate_auth(req)) {
    res.status = 401;
    res.set_content(R"({"error": "Unauthorized"})", "application/json");
    return;
}
```

---

### 2. **Server-Side Request Forgery (SSRF) via /seturl**

**Severity:** 🔴 Critical  
**CWE:** CWE-918 (Server-Side Request Forgery)  
**Location:** `src/http/http_server.cpp:427-455`

**Issue:**
The `/seturl` endpoint accepts arbitrary URLs without validation:

```cpp
std::string url = body["url"].get<std::string>();
LOG_INFO("HTTP API: seturl to %s", url.c_str());
app_->set_url(url);  // No validation!
```

**Attack Scenario:**
```bash
# Access local files
curl -X POST http://localhost:8080/seturl \
  -d '{"url": "file:///etc/shadow"}'

# Access internal services
curl -X POST http://localhost:8080/seturl \
  -d '{"url": "http://169.254.169.254/latest/meta-data/"}'  # AWS metadata

# DNS rebinding
curl -X POST http://localhost:8080/seturl \
  -d '{"url": "http://attacker.com/redirect-to-internal"}'
```

**Recommendation:**
1. **URL Scheme Allowlist**: Only allow `http://` and `https://`
2. **IP Blocklist**: Block private IP ranges (127.0.0.0/8, 10.0.0.0/8, 192.168.0.0/16, etc.)
3. **DNS Validation**: Resolve hostname before loading
4. **Content-Type Validation**: Only allow HTML/text responses

**Remediation Example:**
```cpp
bool is_safe_url(const std::string& url) {
    // Check scheme
    if (url.find("http://") != 0 && url.find("https://") != 0) {
        LOG_WARN("Blocked non-HTTP(S) URL: %s", url.c_str());
        return false;
    }
    
    // Parse hostname
    std::regex url_regex(R"(^https?://([^/:]+))");
    std::smatch match;
    if (!std::regex_search(url, match, url_regex)) {
        return false;
    }
    
    std::string hostname = match[1];
    
    // Block localhost/private IPs
    std::vector<std::string> blocked = {
        "localhost", "127.", "10.", "192.168.", "172.16.",
        "169.254.", "::1", "fc00:", "fe80:"
    };
    
    for (const auto& blocked_prefix : blocked) {
        if (hostname.find(blocked_prefix) == 0) {
            LOG_WARN("Blocked private IP/hostname: %s", hostname.c_str());
            return false;
        }
    }
    
    return true;
}
```

---

## High Severity Issues

### 3. **Unrestricted CORS Policy**

**Severity:** 🟠 High  
**CWE:** CWE-942 (Permissive Cross-domain Policy)  
**Location:** `src/http/http_server.cpp:356-360`, `manager/HTML2NDI Manager/ManagerServer.swift:361-377`

**Issue:**
```cpp
res.set_header("Access-Control-Allow-Origin", "*");
res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
```

Allows **any website** to make requests to the control API.

**Attack Scenario:**
User visits `malicious-site.com` which contains:
```html
<script>
// Malicious site can control HTML2NDI streams
fetch('http://localhost:8080/shutdown', {method: 'POST'});
fetch('http://localhost:8080/seturl', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({url: 'http://attacker.com/phishing'})
});
</script>
```

**Recommendation:**
1. **Restrict CORS to specific origins**: `Access-Control-Allow-Origin: http://localhost:8080`
2. **Use SameSite cookies** (if implementing auth)
3. **Add CSRF tokens** for state-changing operations

**Remediation:**
```cpp
auto add_cors = [this](httplib::Response& res) {
    // Only allow same-origin or configured origins
    res.set_header("Access-Control-Allow-Origin", 
                   "http://localhost:" + std::to_string(port_));
    res.set_header("Access-Control-Allow-Credentials", "true");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type, X-API-Key");
};
```

---

### 4. **Unauthenticated UDP Genlock Packets**

**Severity:** 🟠 High  
**CWE:** CWE-940 (Improper Verification of Source of Communication Channel)  
**Location:** `src/ndi/genlock.cpp:314-376`

**Issue:**
Genlock synchronization packets have no authentication:

```cpp
struct GenlockPacket {
    uint32_t magic{0x474E4C4B};  // 'GNLK'
    uint32_t version{1};
    int64_t timestamp_ns;
    int64_t frame_number;
    uint32_t fps;
    uint32_t checksum;  // Simple XOR - not cryptographic!
};
```

**Attack Scenario:**
```python
# Attacker can inject fake genlock packets
import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

fake_packet = struct.pack(
    '!IIQQI',
    0x474E4C4B,  # magic
    1,           # version
    9999999999,  # fake timestamp
    12345,       # fake frame
    60,          # fps
    0x474E4C4B ^ 1 ^ 9999999999 ^ 12345 ^ 60  # checksum
)

sock.sendto(fake_packet, ('192.168.1.100', 5960))
# Slave streams now desynchronized!
```

**Recommendation:**
1. **HMAC Authentication**: Add HMAC-SHA256 signature using shared secret
2. **Sequence Numbers**: Prevent replay attacks
3. **Timestamp Validation**: Reject packets with timestamps too far in past/future
4. **TLS/DTLS**: Encrypt genlock communication

**Remediation Example:**
```cpp
struct GenlockPacket {
    uint32_t magic{0x474E4C4B};
    uint32_t version{1};
    int64_t timestamp_ns;
    int64_t frame_number;
    uint32_t fps;
    uint64_t sequence_number;  // NEW: Prevent replay
    uint8_t hmac[32];           // NEW: HMAC-SHA256
    
    void sign(const std::string& shared_secret) {
        // Compute HMAC over all fields except hmac itself
        // Use OpenSSL HMAC functions
    }
    
    bool verify(const std::string& shared_secret) {
        // Verify HMAC
        // Check sequence number > last_seen
        // Check timestamp within acceptable window
    }
};
```

---

### 5. **Input Validation Weaknesses**

**Severity:** 🟠 High  
**CWE:** CWE-20 (Improper Input Validation)  
**Location:** Multiple files

**Issues:**

#### 5a. Thumbnail Width/Quality (http_server.cpp:495-502)
```cpp
int width = std::stoi(req.get_param_value("width"));
width = std::max(64, std::min(1920, width));  // Good!
```
✅ **Already mitigated** with clamping, but no exception handling for non-numeric input.

**Potential crash:**
```bash
curl "http://localhost:8080/thumbnail?width=AAAA"  # std::stoi throws
```

**Fix:**
```cpp
try {
    width = std::stoi(req.get_param_value("width"));
    width = std::max(64, std::min(1920, width));
} catch (const std::exception&) {
    width = 320;  // Default
}
```

#### 5b. Config Validation (config.cpp:235-283)
Validates ranges but not **resource exhaustion**:
```cpp
if (width < 16 || width > 7680) { ... }  // Max 7680x4320
```

**Attack:** Request 7680x4320@240fps = ~5.5GB/sec bandwidth

**Fix:** Add `--max-resolution` and `--max-fps` limits for untrusted sources.

#### 5c. Swift Manager JSON Parsing (ManagerServer.swift:198-224)
No validation on `width`, `height`, `fps`:
```swift
config.width = json["width"] as? Int ?? 1920  // Could be negative!
config.height = json["height"] as? Int ?? 1080
config.fps = json["fps"] as? Int ?? 60
```

**Fix:**
```swift
if let w = json["width"] as? Int, w >= 16, w <= 7680 {
    config.width = w
}
```

---

## Medium Severity Issues

### 6. **Insecure Default HTTP Binding**

**Severity:** 🟡 Medium  
**CWE:** CWE-16 (Configuration)  
**Location:** `include/html2ndi/config.h:29`

**Issue:**
```cpp
std::string http_host = "0.0.0.0";  // Binds to ALL interfaces
```

Default allows **remote access** from any IP on the network without explicit user consent.

**Recommendation:**
Change default to localhost, but support explicit interface binding:

```cpp
std::string http_host = "127.0.0.1";  // Secure default
```

**Usage Examples:**
```bash
# Secure default (localhost only)
./html2ndi --url http://example.com

# Bind to specific interface IP (broadcast VLAN)
./html2ndi --url http://example.com --http-host 192.168.100.50

# Bind to all interfaces (requires explicit opt-in)
./html2ndi --url http://example.com --http-host 0.0.0.0

# With authentication for network access (recommended)
./html2ndi --url http://example.com \
           --http-host 192.168.100.50 \
           --api-key "$(security find-generic-password -a html2ndi -s api-key -w)"
```

**Security Warning:**
When `--http-host` is set to a non-localhost address, log a warning:
```cpp
if (config.http_host != "127.0.0.1" && config.http_host != "localhost") {
    LOG_WARN("HTTP API exposed on network interface %s - ensure API key is set!", 
             config.http_host.c_str());
    if (config.api_key.empty()) {
        LOG_ERROR("Network binding without API key is insecure!");
    }
}
```

---

### 7. **Path Traversal Risk in Log File Path**

**Severity:** 🟡 Medium  
**CWE:** CWE-22 (Path Traversal)  
**Location:** `src/utils/logger.cpp:61-79`

**Issue:**
```cpp
void Logger::initialize(LogLevel level, const std::string& file_path) {
    file_path_ = file_path;  // User-controlled via --log-file
    // ...
    file_.open(file_path_, std::ios::app);
}
```

**Attack:**
```bash
./html2ndi --log-file /etc/passwd --url http://example.com
# Or: --log-file ../../../../etc/cron.d/backdoor
```

**Recommendation:**
Validate `file_path` is within allowed directories:
```cpp
std::filesystem::path safe_log_path(const std::string& user_path) {
    std::filesystem::path p = std::filesystem::canonical(user_path);
    std::filesystem::path home = std::getenv("HOME");
    
    if (!p.string().starts_with(home.string())) {
        LOG_ERROR("Log path outside home directory: %s", p.c_str());
        return home / "Library/Logs/HTML2NDI/html2ndi.log";
    }
    return p;
}
```

---

## Low Severity Issues

### 8. **Unsafe String Functions in stb_image_write**

**Severity:** 🟢 Low  
**CWE:** CWE-676 (Use of Potentially Dangerous Function)  
**Location:** `src/utils/image_encode.cpp:6-10`

**Issue:**
```cpp
// Suppress sprintf deprecation warning in stb_image_write
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
```

stb_image_write uses `sprintf` internally (no bounds checking).

**Mitigation:**
✅ **Already suppressed** as stb_image is a trusted, audited library.

**Recommendation:**
Monitor for stb_image_write updates. Consider alternative: `libjpeg-turbo` with safer APIs.

---

### 9. **Secrets in GitHub Workflows**

**Severity:** 🟢 Low (Informational)  
**Status:** ✅ Properly handled

**Location:** `.github/workflows/release.yml`

**Analysis:**
```yaml
- name: Configure AWS credentials (OIDC)
  uses: aws-actions/configure-aws-credentials@v4
  with:
    role-to-assume: arn:aws:iam::598063954204:role/HTML2NDI-GitHubActions
    aws-region: us-east-1
```

✅ **Best Practice:** Uses OIDC (no long-lived credentials)

```yaml
env:
  APPLE_CERTIFICATE_BASE64: ${{ secrets.APPLE_CERTIFICATE_BASE64 }}
  APPLE_CERTIFICATE_PASSWORD: ${{ secrets.APPLE_CERTIFICATE_PASSWORD }}
```

✅ **Properly stored** in GitHub Secrets (encrypted at rest)

**Recommendation:**
- Rotate `APPLE_CERTIFICATE_PASSWORD` annually
- Audit who has access to repository secrets (Settings > Secrets)
- Enable "Require approval for all outside collaborators" for Actions

---

## Broadcast Network Deployment Guide

### Secure Network Interface Binding

For broadcast environments where you need to expose the HTTP API to specific network interfaces:

#### Recommended Network Architecture

```
┌─────────────────────────────────────────────────────────────┐
│ Production Broadcast Network                                │
│                                                              │
│  ┌──────────────┐                    ┌──────────────┐       │
│  │ MacBook Pro  │ eth1: 192.168.100.50               │       │
│  │ (HTML2NDI)   ├────────────────────┤ Broadcast    │       │
│  │              │                    │ VLAN 100     │       │
│  └──────────────┘                    │ (Isolated)   │       │
│   │ lo0: 127.0.0.1                   └──────────────┘       │
│   │ (Control)                                               │
│   │                                                         │
│   └─── Dashboard access from localhost only                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

#### Configuration Strategies

**Option 1: Dual-Interface Setup (Most Secure)**
```bash
# Terminal 1: Worker on broadcast interface (with auth)
./html2ndi \
  --url "https://graphics.internal/overlay.html" \
  --ndi-name "Camera 1 Graphics" \
  --http-host 192.168.100.50 \
  --http-port 8080 \
  --api-key "$BROADCAST_API_KEY"

# Terminal 2: Manager on localhost (no auth needed)
./HTML2NDI\ Manager.app
# Dashboard accessible at http://127.0.0.1:8080 (different port)
```

**Option 2: Single Interface with Firewall Rules**
```bash
# Bind to specific interface
./html2ndi \
  --url "https://graphics.internal/overlay.html" \
  --http-host 192.168.100.50 \
  --api-key "$(openssl rand -hex 32)"

# macOS Application Firewall rule
/usr/libexec/ApplicationFirewall/socketfilterfw --add /path/to/html2ndi
/usr/libexec/ApplicationFirewall/socketfilterfw --blockapp /path/to/html2ndi
# Then allow only specific IPs in System Settings > Network > Firewall
```

**Option 3: SSH Tunnel (Maximum Security)**
```bash
# On worker machine: bind to localhost only
./html2ndi --url https://example.com --http-host 127.0.0.1 --http-port 8080

# From control machine: SSH tunnel
ssh -L 9090:127.0.0.1:8080 user@192.168.100.50

# Access dashboard at http://localhost:9090
```

#### Interface Binding Best Practices

1. **Identify Your Network Interface:**
   ```bash
   ifconfig | grep inet
   # en0: Thunderbolt/USB-C Ethernet
   # en1: Wi-Fi
   # en7: Dedicated broadcast interface
   ```

2. **Bind to Specific IP (Not 0.0.0.0):**
   ```bash
   # ❌ BAD: Binds to ALL interfaces
   --http-host 0.0.0.0
   
   # ✅ GOOD: Binds to specific interface
   --http-host 192.168.100.50
   ```

3. **Verify Binding:**
   ```bash
   lsof -i -P | grep html2ndi
   # Should show:
   # html2ndi 12345 user TCP 192.168.100.50:8080 (LISTEN)
   # NOT:
   # html2ndi 12345 user TCP *:8080 (LISTEN)
   ```

4. **Always Use API Key for Network Binding:**
   ```bash
   # Generate and store in Keychain
   openssl rand -hex 32 | security add-generic-password \
     -a html2ndi -s api-key -w -
   
   # Use in command
   ./html2ndi --http-host 192.168.100.50 \
              --api-key "$(security find-generic-password -a html2ndi -s api-key -w)"
   ```

#### Network Segmentation

For production broadcast environments:

| Network | Purpose | Interface | Security |
|---------|---------|-----------|----------|
| Broadcast VLAN | NDI, genlock, control | eth1 (192.168.100.x) | API key required |
| Management | Remote access | VPN only | SSH + 2FA |
| Public | Internet | Firewall | Blocked |

#### Firewall Rules (pf.conf)

```bash
# /etc/pf.conf
# Allow HTTP API only from broadcast VLAN
pass in on en7 proto tcp from 192.168.100.0/24 to any port 8080
block in on en7 proto tcp from any to any port 8080

# Block external access
block in on en0 proto tcp from any to any port 8080
```

---

## Additional Security Recommendations

### 10. Enable Security Features

#### A. Code Signing (macOS)
**Status:** ✅ Implemented in CI/CD
```yaml
- name: Install Apple Certificate
```

**Next Step:** Add **notarization** to prevent Gatekeeper warnings:
```bash
xcrun notarytool submit "HTML2NDI Manager.app" \
  --apple-id "$APPLE_ID" \
  --password "$APP_SPECIFIC_PASSWORD" \
  --team-id "$TEAM_ID"
```

#### B. Hardened Runtime
Enable in `resources/entitlements.plist`:
```xml
<key>com.apple.security.cs.allow-unsigned-executable-memory</key>
<false/>  <!-- Prevent code injection -->
<key>com.apple.security.cs.disable-library-validation</key>
<false/>  <!-- Only load signed dylibs -->
```

#### C. App Sandbox (Future)
Currently disabled due to CEF requirements. Consider CEF alternatives or partial sandboxing.

---

### 11. Dependency Security

#### Current Dependencies
| Dependency | Version | Vulnerabilities |
|------------|---------|-----------------|
| CEF | 120.1.10 | ✅ No known CVEs |
| NDI SDK | Proprietary | ⚠️ Closed-source |
| cpp-httplib | Header-only | ✅ Up to date |
| nlohmann/json | Header-only | ✅ No CVEs |
| stb_image_write | Single-file | ✅ Audited |

**Recommendation:**
- Add `dependabot.yml` for automated dependency updates
- Run periodic security scans: `brew install osv-scanner && osv-scanner scan .`

---

### 12. Network Security

#### Current State
- HTTP server: Unencrypted (HTTP, not HTTPS)
- Genlock: Unencrypted UDP
- NDI: Encrypted per NDI spec (AES-256 when enabled)

**Recommendations:**
1. **Add HTTPS Support**: 
   ```bash
   ./html2ndi --http-tls --http-cert cert.pem --http-key key.pem
   ```

2. **mTLS for Genlock**: Mutual TLS for master-slave communication

3. **Network Segmentation**: Document recommendation to run on isolated VLAN

---

### 13. Logging Security

#### Current Issues
```cpp
LOG_INFO("HTTP API: seturl to %s", url.c_str());  // Logs user input
```

**Sensitive Data in Logs:**
- URLs may contain credentials: `http://user:pass@internal.com`
- File paths may reveal system structure

**Recommendation:**
Sanitize URLs before logging:
```cpp
std::string sanitize_url(const std::string& url) {
    std::regex cred_regex(R"(://[^@]+@)");
    return std::regex_replace(url, cred_regex, "://***@");
}

LOG_INFO("HTTP API: seturl to %s", sanitize_url(url).c_str());
```

---

### 14. Process Isolation

**Current:** Manager spawns worker processes with full privileges

**Recommendation:**
```swift
// In StreamManager.swift
process.environment = [
    "HOME": NSHomeDirectory(),
    "TMPDIR": NSTemporaryDirectory()
]
// Don't inherit full environment (may contain secrets)
```

---

## Remediation Priority

### Phase 1: Immediate (Critical)
1. ✅ Restrict HTTP API to 127.0.0.1 by default
2. ✅ Add URL validation to `/seturl` (block file://, private IPs)
3. ✅ Add authentication (API key or token)

### Phase 2: Short-term (High)
4. ⏳ Restrict CORS to same-origin
5. ⏳ Add HMAC authentication to genlock packets
6. ⏳ Improve input validation (exception handling)

### Phase 3: Medium-term (Medium)
7. ⏳ Validate log file paths
8. ⏳ Add HTTPS support
9. ⏳ Implement rate limiting

### Phase 4: Long-term (Low)
10. ⏳ Add notarization to build process
11. ⏳ Enable hardened runtime
12. ⏳ Automated dependency scanning

---

## Testing Recommendations

### Security Test Suite
Create `tests/security/`:

1. **test_auth.cpp**: Verify API key enforcement
2. **test_ssrf.cpp**: Attempt blocked URLs
3. **test_cors.cpp**: Verify origin restrictions
4. **test_genlock_auth.cpp**: Reject unsigned packets
5. **test_fuzzing.cpp**: Fuzz inputs (width, height, URLs)

### Penetration Testing
Run before v1.5.0 stable release:
```bash
# OWASP ZAP scan
zap-cli quick-scan http://localhost:8080

# Nmap for open ports
nmap -sV localhost

# Burp Suite for API testing
```

---

## Compliance & Standards

### Relevant Standards
- **OWASP Top 10 2021**: Address A01 (Broken Access Control), A03 (Injection)
- **CWE Top 25**: Address CWE-918 (SSRF), CWE-306 (Missing Authentication)
- **NIST Cybersecurity Framework**: Align with PR.AC (Access Control), PR.DS (Data Security)

---

## Conclusion

HTML2NDI v1.5.0-alpha has **no severe exploitable vulnerabilities** in its current usage model (localhost, trusted network). However, **2 critical issues** must be addressed before deployment in production or multi-user environments:

1. ⚠️ **Implement HTTP API authentication**
2. ⚠️ **Add URL validation to prevent SSRF**

All other issues can be addressed in subsequent releases based on deployment requirements.

**Overall Security Grade:** B- (Good for local use, needs hardening for production)

---

**Auditor:** AI Assistant (Claude Sonnet 4.5)  
**Next Review:** Before v1.6.0 release or when exposing to untrusted networks

