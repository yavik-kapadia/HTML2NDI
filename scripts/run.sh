#!/bin/bash
#
# HTML2NDI Runner Script
# 
# This script provides a convenient way to run HTML2NDI with common configurations.
#

set -e

# Configuration defaults
URL="${URL:-about:blank}"
WIDTH="${WIDTH:-1920}"
HEIGHT="${HEIGHT:-1080}"
FPS="${FPS:-60}"
NDI_NAME="${NDI_NAME:-HTML2NDI}"
HTTP_PORT="${HTTP_PORT:-8080}"
LOG_LEVEL="${LOG_LEVEL:-info}"

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# Find the executable
if [[ -x "$PROJECT_DIR/build/bin/html2ndi" ]]; then
    HTML2NDI="$PROJECT_DIR/build/bin/html2ndi"
elif [[ -x "/usr/local/bin/html2ndi" ]]; then
    HTML2NDI="/usr/local/bin/html2ndi"
else
    echo "Error: html2ndi executable not found"
    echo "Please build the project first: cmake --build build"
    exit 1
fi

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -u|--url)
            URL="$2"
            shift 2
            ;;
        -w|--width)
            WIDTH="$2"
            shift 2
            ;;
        -h|--height)
            HEIGHT="$2"
            shift 2
            ;;
        -f|--fps)
            FPS="$2"
            shift 2
            ;;
        -n|--name)
            NDI_NAME="$2"
            shift 2
            ;;
        -p|--port)
            HTTP_PORT="$2"
            shift 2
            ;;
        --test)
            URL="file://$PROJECT_DIR/resources/test.html"
            shift
            ;;
        --help)
            echo "HTML2NDI Runner Script"
            echo ""
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -u, --url URL       URL to render (default: about:blank)"
            echo "  -w, --width N       Frame width (default: 1920)"
            echo "  -h, --height N      Frame height (default: 1080)"
            echo "  -f, --fps N         Target FPS (default: 60)"
            echo "  -n, --name NAME     NDI source name (default: HTML2NDI)"
            echo "  -p, --port PORT     HTTP API port (default: 8080)"
            echo "  --test              Use built-in test page"
            echo "  --help              Show this help"
            echo ""
            echo "Environment variables:"
            echo "  URL, WIDTH, HEIGHT, FPS, NDI_NAME, HTTP_PORT, LOG_LEVEL"
            echo ""
            echo "Examples:"
            echo "  $0 --url https://example.com"
            echo "  $0 --test --fps 30"
            echo "  URL=https://mysite.com WIDTH=1280 HEIGHT=720 $0"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Build command
CMD=(
    "$HTML2NDI"
    --url "$URL"
    --width "$WIDTH"
    --height "$HEIGHT"
    --fps "$FPS"
    --ndi-name "$NDI_NAME"
    --http-port "$HTTP_PORT"
)

# Add verbose flag for debug log level
if [[ "$LOG_LEVEL" == "debug" ]]; then
    CMD+=("--verbose")
fi

echo "Starting HTML2NDI..."
echo "  URL:        $URL"
echo "  Resolution: ${WIDTH}x${HEIGHT} @ ${FPS}fps"
echo "  NDI Name:   $NDI_NAME"
echo "  HTTP API:   http://127.0.0.1:$HTTP_PORT"
echo ""

# Run
exec "${CMD[@]}"

