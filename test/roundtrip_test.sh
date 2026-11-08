#!/bin/bash
# Roundtrip testing script for SREC utilities
# This script creates test data, converts to SREC, checks CRC, converts back to binary,
# and compares the result with the original data.

set -e  # Exit on error

# Create temporary test files
TEST_DIR="/tmp/srec_test"
mkdir -p $TEST_DIR
BINARY_FILE="$TEST_DIR/test.bin"
SREC_FILE="$TEST_DIR/test.srec"
OUTPUT_FILE="$TEST_DIR/roundtrip.bin"

# Determine base directory for executable paths
# If BUILD_DIR is set, use it; otherwise try to find executables in current directory
if [ -n "$BUILD_DIR" ]; then
    BASE_DIR="$BUILD_DIR"
elif [ -f "./bin2srec" ]; then
    BASE_DIR="."
else
    BASE_DIR=$(dirname $(dirname $(realpath $0)))/build_host
fi

BIN2SREC="$BASE_DIR/bin2srec"
SREC2BIN="$BASE_DIR/srec2bin"
SRECCHECK="$BASE_DIR/sreccheck"

echo "=== SREC Utilities Roundtrip Test ==="
echo "Test directory: $TEST_DIR"
echo "Using executables from: $BASE_DIR"

# Check if executables exist
for exe in "$BIN2SREC" "$SREC2BIN" "$SRECCHECK"; do
    if [ ! -f "$exe" ]; then
        echo "Error: Executable $exe not found."
        echo "Please build the utilities first with './build.sh'"
        exit 1
    fi
done

# Create test data of various sizes
create_test_data() {
    local size=$1
    echo "Creating test data of size $size bytes..."
    
    # Create binary file with random data
    dd if=/dev/urandom of="$BINARY_FILE" bs=1 count=$size 2>/dev/null
    
    echo "Test data created at $BINARY_FILE"
}

# Perform the roundtrip test
run_roundtrip_test() {
    local addrbits=$1
    
    echo "Testing with address size: $addrbits bits"
    
    # Convert binary to SREC
    echo "Converting binary to SREC..."
    "$BIN2SREC" -i "$BINARY_FILE" -o "$SREC_FILE" -b $addrbits -c
    
    # Verify the SREC file with sreccheck
    echo "Verifying SREC file..."
    if "$SRECCHECK" "$SREC_FILE"; then
        echo "CRC check passed!"
    else
        echo "CRC check failed!"
        return 1
    fi
    
    # Convert SREC back to binary
    echo "Converting SREC back to binary..."
    "$SREC2BIN" -i "$SREC_FILE" -o "$OUTPUT_FILE"
    
    # Compare the files
    echo "Comparing original and roundtrip files..."
    if cmp -s "$BINARY_FILE" "$OUTPUT_FILE"; then
        echo "Files are identical. Roundtrip test PASSED!"
        return 0
    else
        echo "Files differ. Roundtrip test FAILED!"
        
        # Show detailed difference if files are small enough
        if [ $(stat -c%s "$BINARY_FILE") -lt 1024 ]; then
            echo "Differences:"
            hexdump -C "$BINARY_FILE" > "$TEST_DIR/original.hex"
            hexdump -C "$OUTPUT_FILE" > "$TEST_DIR/roundtrip.hex"
            diff -u "$TEST_DIR/original.hex" "$TEST_DIR/roundtrip.hex" || true
        fi
        
        return 1
    fi
}

# Malformed input handling: tools must reject bad files with a normal error
# exit (1), never crash/abort (signal, exit >= 128) and never exit 0 on error
run_malformed_tests() {
    local failures=0

    echo ""
    echo "=== Testing malformed input handling ==="

    printf 'S0060000GG\n'  > "$TEST_DIR/bad_crc.srec"    # non-hex in S0 CRC field
    printf 'S1\n'          > "$TEST_DIR/short.srec"      # truncated record
    printf 'S10700001Z00\n' > "$TEST_DIR/bad_hex.srec"   # non-hex in data field

    for f in bad_crc short bad_hex; do
        local rc=0
        "$SRECCHECK" "$TEST_DIR/$f.srec" >/dev/null 2>&1 || rc=$?
        if [ $rc -eq 0 ]; then
            echo "FAILED: sreccheck accepted malformed $f.srec"
            failures=$((failures+1))
        elif [ $rc -ge 128 ]; then
            echo "FAILED: sreccheck crashed on $f.srec (signal $((rc-128)))"
            failures=$((failures+1))
        else
            echo "OK: sreccheck rejected $f.srec (exit $rc)"
        fi

        rc=0
        "$SREC2BIN" -i "$TEST_DIR/$f.srec" -o "$TEST_DIR/bad.bin" >/dev/null 2>&1 || rc=$?
        if [ $rc -eq 0 ]; then
            echo "FAILED: srec2bin accepted malformed $f.srec (must exit non-zero)"
            failures=$((failures+1))
        elif [ $rc -ge 128 ]; then
            echo "FAILED: srec2bin crashed on $f.srec (signal $((rc-128)))"
            failures=$((failures+1))
        else
            echo "OK: srec2bin rejected $f.srec (exit $rc)"
        fi
    done

    return $failures
}

# Test with different file sizes and address modes
run_all_tests() {
    local sizes=(10 100 1000 10000)
    local address_bits=(16 24 32)
    local failures=0
    
    for size in "${sizes[@]}"; do
        echo ""
        echo "=== Testing with file size: $size bytes ==="
        create_test_data $size
        
        for bits in "${address_bits[@]}"; do
            echo ""
            if ! run_roundtrip_test $bits; then
                failures=$((failures+1))
            fi
        done
    done
    
    local malformed=0
    run_malformed_tests || malformed=$?
    failures=$((failures+malformed))

    echo ""
    echo "=== Test Summary ==="
    if [ $failures -eq 0 ]; then
        echo "All tests PASSED!"
        return 0
    else
        echo "$failures test(s) FAILED!"
        return 1
    fi
}

# Run the tests
run_all_tests

# Clean up
echo ""
echo "Cleaning up test files..."
rm -rf "$TEST_DIR"

echo "Test completed!"