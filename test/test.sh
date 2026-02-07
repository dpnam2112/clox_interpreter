#!/bin/bash

COMPILER="./bin/clox"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m' # No Color (Reset)

PASS_COUNT=0
FAIL_COUNT=0

EX_DATAERR=65
EX_SOFTWARE=70

clear
echo -e "${BOLD}${CYAN}Starting clox Test Suite...${NC}\n"

for source_file in test/prog/*.clox; do
    name=$(basename "$source_file" .clox)
    expected_file="./test/expected_output/$name.txt"
    
    if [[ "$name" == *"err_comp"* ]]; then
        expected_exit=$EX_DATAERR
    elif [[ "$name" == *"err_run"* ]]; then
        expected_exit=$EX_SOFTWARE
    else
        expected_exit=0
    fi

    echo -n -e "Testing ${BOLD}$name${NC}... "

    # Force line-buffering for both stdout and stderr
    actual_output=$(stdbuf -oL -eL $COMPILER "$source_file" 2>&1)
    actual_exit=$?

    output_diff=$(diff -u "$expected_file" <(echo "$actual_output"))
    
    if [ $actual_exit -eq $expected_exit ] && [ -z "$output_diff" ]; then
        echo -e "${GREEN}PASS${NC}"
        ((PASS_COUNT++))
    else
        echo -e "${RED}FAIL${NC}"
        if [ $actual_exit -ne $expected_exit ]; then
            echo -e "  ${YELLOW}Exit Code Mismatch:${NC} Expected $expected_exit, got $actual_exit"
        fi
        if [ -n "$output_diff" ]; then
            echo -e "  ${YELLOW}Output Mismatch:${NC}"
            echo "$output_diff" | sed 's/^/  /'
        fi
        ((FAIL_COUNT++))
    fi
done

echo -e "\n${CYAN}--------------------------${NC}"
if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "Result: ${GREEN}${BOLD}ALL PASSED${NC} ($PASS_COUNT total)"
else
    echo -e "Result: ${RED}${BOLD}$FAIL_COUNT FAILED${NC}, ${GREEN}$PASS_COUNT Passed${NC}"
fi

[ $FAIL_COUNT -eq 0 ] || exit 1
