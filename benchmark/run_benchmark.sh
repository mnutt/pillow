#!/bin/bash

# Pillow HTTP Server Benchmark Script
# Uses wrk to test performance with many connections

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_BIN="${SCRIPT_DIR}/bench_server"
PORT=${PORT:-8080}
DURATION=${DURATION:-10s}
THREADS=${THREADS:-4}
CONNECTIONS=${CONNECTIONS:-100}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

print_usage() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -p, --port PORT         Server port (default: 8080)"
    echo "  -d, --duration DURATION Test duration (default: 10s)"
    echo "  -t, --threads THREADS   Number of wrk threads (default: 4)"
    echo "  -c, --connections CONNS Number of connections (default: 100)"
    echo "  -s, --skip-build        Skip building the server"
    echo "  -h, --help              Show this help"
    echo ""
    echo "Environment variables:"
    echo "  PORT, DURATION, THREADS, CONNECTIONS"
    echo ""
    echo "Example:"
    echo "  $0 -c 1000 -t 8 -d 30s"
}

SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -d|--duration)
            DURATION="$2"
            shift 2
            ;;
        -t|--threads)
            THREADS="$2"
            shift 2
            ;;
        -c|--connections)
            CONNECTIONS="$2"
            shift 2
            ;;
        -s|--skip-build)
            SKIP_BUILD=1
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Check for wrk
if ! command -v wrk &> /dev/null; then
    echo -e "${RED}Error: wrk is not installed${NC}"
    echo "Install with: brew install wrk"
    exit 1
fi

# Build if needed
if [[ $SKIP_BUILD -eq 0 ]]; then
    echo -e "${YELLOW}Building benchmark server...${NC}"

    BUILD_DIR="${SCRIPT_DIR}/../build"
    if [[ ! -d "$BUILD_DIR" ]]; then
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake ..
    fi

    cd "$BUILD_DIR"
    make bench_server -j$(sysctl -n hw.ncpu 2>/dev/null || nproc)

    SERVER_BIN="${BUILD_DIR}/benchmark/bench_server"
fi

if [[ ! -x "$SERVER_BIN" ]]; then
    echo -e "${RED}Error: Server binary not found at $SERVER_BIN${NC}"
    exit 1
fi

# Kill any existing server on the port
pkill -f "bench_server.*-p.*$PORT" 2>/dev/null || true
sleep 0.5

echo -e "${GREEN}Starting Pillow benchmark server on port $PORT...${NC}"
"$SERVER_BIN" -p "$PORT" &
SERVER_PID=$!

# Wait for server to start
sleep 1

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo -e "${RED}Error: Server failed to start${NC}"
    exit 1
fi

cleanup() {
    echo -e "\n${YELLOW}Stopping server...${NC}"
    kill $SERVER_PID 2>/dev/null || true
    wait $SERVER_PID 2>/dev/null || true
}
trap cleanup EXIT

URL="http://127.0.0.1:$PORT/"

echo ""
echo -e "${GREEN}=== Pillow HTTP Server Benchmark ===${NC}"
echo "URL: $URL"
echo "Duration: $DURATION"
echo "Threads: $THREADS"
echo "Connections: $CONNECTIONS"
echo ""

# Quick warmup
echo -e "${YELLOW}Warming up...${NC}"
wrk -t2 -c10 -d2s "$URL" > /dev/null 2>&1

echo -e "${GREEN}Running benchmark...${NC}"
echo ""

wrk -t"$THREADS" -c"$CONNECTIONS" -d"$DURATION" --latency "$URL"

echo ""
echo -e "${GREEN}Benchmark complete!${NC}"
