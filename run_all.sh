#!/bin/bash

# 1. Quick Rebuild
echo "[+] Rebuilding toolchain..."
cd build && make -j$(nproc) && cd ..

# 2. Setup Test Tracker
PASSED=0
TOTAL=0

echo -e "\n=================================================="
echo "      STARTING FULL HARDWARE INTEGRATION SCAN"
echo "=================================================="

# 3. Loop through all .txt files in your integration folder
for test_file in tests/integration/*.txt; do
    ((TOTAL++))
    echo -ne "[ TESTING ] $test_file ... "

    # Run the compiler and capture the output
    # We look for the "Hardware Execution" marker and the actual result
    OUTPUT=$(./build/compiler "$test_file" 2>&1)

    # Check for success (e.g., if it didn't throw an 'Unknown instruction' or 'Syntax Error')
    if [[ $OUTPUT == *"Assembler Error"* ]] || [[ $OUTPUT == *"Syntax Error"* ]]; then
        echo -e "\e[31mFAILED (Compiler/Assembler Error)\e[0m"
        echo "--------------------------------------------------"
        echo "$OUTPUT" | grep -E "Error|Line"
        echo "--------------------------------------------------"
    elif [[ $OUTPUT == *"CPU Halted"* ]]; then
        echo -e "\e[32mPASSED (Executed)\e[0m"
        ((PASSED++))
        # Optional: Print the last number seen before shutdown to verify logic
        RESULT=$(echo "$OUTPUT" | grep -v "---" | grep -v ">>" | tail -n 2 | head -n 1)
        echo "            -> CPU Result: $RESULT"
    else
        echo -e "\e[33mTIMEOUT / HUNG\e[0m"
    fi
done

echo "=================================================="
echo " FINAL SCORE: $PASSED / $TOTAL TESTS PASSED"
echo "=================================================="

if [ $PASSED -ne $TOTAL ]; then
    exit 1
fi