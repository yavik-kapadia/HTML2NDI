# HTML2NDI — Agent Reference

This file provides quick navigation to project documentation for AI agents and developers.

## Quick Start

- **Project Specification**: See `.cursorrules` (automatically loaded by Cursor)
- **Agent Instructions**: See `docs/AGENT_INSTRUCTIONS.md`
- **User Documentation**: See `README.md`

## Documentation Index

### Core Documentation
- `.cursorrules` — Project architecture, functional requirements, and technical specifications
- `docs/AGENT_INSTRUCTIONS.md` — Development guidelines, version management, testing procedures

### Specialized Guides
- `docs/SECURITY_AUDIT.md` — Security audit findings and recommendations
- `docs/SECURITY_CHECKLIST.md` — Security hardening checklist
- `docs/NETWORK_PERMISSIONS.md` — macOS network permissions guide
- `docs/NETWORK_INTERFACE_GUIDE.md` — Network interface binding strategies
- `docs/genlock-implementation-summary.md` — Multi-stream synchronization guide
- `docs/DASHBOARD_GENLOCK.md` — Genlock UI implementation details

### Testing Documentation
- `tests/integration/README.md` — Integration testing guide
- `tests/web/README.md` — Web UI testing guide

## Key Principles

1. **Code Quality**: Always write tests, run linter before merge
2. **Documentation**: Store all docs in `/docs` except `README.md` and `AGENTS.md`
3. **Version Management**: Follow semantic versioning and pre-release checklists (see `docs/AGENT_INSTRUCTIONS.md`)
4. **Expert Implementation**: Take the harder route; choose easy path only as last resort

## Architecture Overview

```
html2ndi (C++ Worker)          HTML2NDI Manager (Swift)
├─ CEF off-screen rendering    ├─ Menu bar application
├─ NDI video output            ├─ Process management
├─ HTTP control API            ├─ Native SwiftUI dashboard
└─ Genlock synchronization     └─ Web dashboard server
```

## Quick Commands

```bash
# Build C++ worker
cd build && cmake .. -G Ninja && ninja

# Build Swift manager
cd manager && swift build

# Run tests
cd build && ./test_runner
cd manager && swift test

# Create release
git tag -a vX.Y.Z -m "Release vX.Y.Z"
git push origin vX.Y.Z
```

## Need More Details?

→ **Start with**: `docs/AGENT_INSTRUCTIONS.md` for comprehensive development guidelines  
→ **Technical specs**: `.cursorrules` for project architecture  
→ **Security**: `docs/SECURITY_AUDIT.md` for security considerations

---

**Note**: This file is intentionally minimal. Full documentation is organized in:
- `.cursorrules` — Always-applied project rules (auto-loaded by Cursor)
- `docs/AGENT_INSTRUCTIONS.md` — Complete development guidelines
