#!/bin/bash
# Simple script to run the audio communication test
# This script helps test the audio communication by running all components

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/../../build/examples/audio-comm-test"
SIGNALING_DIR="${SCRIPT_DIR}/../signaling-server-python"

echo "==================================="
echo "Audio Communication Test Runner"
echo "==================================="
echo ""

# Check if binaries exist
if [ ! -f "${BUILD_DIR}/audio-comm-server" ] || [ ! -f "${BUILD_DIR}/audio-comm-client" ]; then
    echo "Error: Binaries not found. Please build the project first:"
    echo "  cd ../../"
    echo "  cmake -B build -DCMAKE_BUILD_TYPE=Release"
    echo "  cd build"
    echo "  make -j2"
    exit 1
fi

# Check if Python signaling server exists
if [ ! -f "${SIGNALING_DIR}/signaling-server.py" ]; then
    echo "Error: Signaling server not found at ${SIGNALING_DIR}"
    exit 1
fi

# Check if websockets module is installed
if ! python3 -c "import websockets" 2>/dev/null; then
    echo "Installing websockets module..."
    pip3 install websockets
fi

echo "Starting signaling server on port 8080..."
python3 "${SIGNALING_DIR}/signaling-server.py" 8080 &
SIGNALING_PID=$!
sleep 2

echo "Starting audio communication server..."
"${BUILD_DIR}/audio-comm-server" &
SERVER_PID=$!
sleep 2

echo ""
echo "==================================="
echo "Running audio communication client..."
echo "==================================="
echo ""
sleep 1

"${BUILD_DIR}/audio-comm-client"
CLIENT_EXIT_CODE=$?

echo ""
echo "==================================="
echo "Test completed with exit code: ${CLIENT_EXIT_CODE}"
echo "==================================="
echo ""

# Cleanup
echo "Cleaning up processes..."
kill $SERVER_PID 2>/dev/null || true
kill $SIGNALING_PID 2>/dev/null || true

if [ $CLIENT_EXIT_CODE -eq 0 ]; then
    echo "✓ Test PASSED"
    exit 0
else
    echo "✗ Test FAILED"
    exit 1
fi
