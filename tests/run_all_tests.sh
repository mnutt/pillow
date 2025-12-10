#!/bin/bash
# Auto-generated test runner script
set -e

echo "Running Pillow Test Suite"
echo "========================"

# Run all tests using ctest
cd "/Users/mnutt/p/movableink/pillow/tests"
ctest --output-on-failure

echo ""
echo "Test suite completed!"
