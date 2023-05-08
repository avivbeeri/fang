#!/bin/bash

ERRORS=""
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
TOTAL=0
FAILURES=0

# Tests are defined here so make adding them easier
allTests() {
  testFile examples/error.fg "[line 3; pos 1] Error at '}': Expect ';' after expression." 1
  testFile examples/helloworld.fg "OK" 0 "hello world" 0
  testFile examples/empty.fg "OK" 0 "" 0
  testFile examples/return.fg "OK" 0 "" 42
}


testFile() {
  local FILENAME=$1
  local BIN=${FILENAME%.*}

  local COMPILER
  COMPILER=$(./fgc $FILENAME $BIN)
  local COMPILER_CODE=$?

  local COMPILER_EXPECT=$2
  
  local COMPILER_CODE_EXPECTED=$3
  TOTAL=$(($TOTAL + 1))

  if [ "$COMPILER" != "$COMPILER_EXPECT" ]; then
    echo -e "${RED}[FAIL]${NC}: $1"
    ERRORS+="${RED}[FAIL]${NC}: $1 - Compiler Output"
    ERRORS+=$'\n'
    ERRORS+="        Expected: $COMPILER_EXPECT"
    ERRORS+=$'\n'
    ERRORS+="        Actual: \"$COMPILER \""
    ERRORS+=$'\n\n'
    ERRORS+="---------------------------------------"
    ERRORS+=$'\n'
    FAILURES=$(($FAILURES + 1))
    return 1
  fi

  if [ "$COMPILER_CODE" != "$COMPILER_CODE_EXPECTED" ]; then
    echo -e "${RED}[FAIL]${NC}: $1"
    ERRORS+="${RED}[FAIL]${NC}: $1 - Compiler Exit Code"
    ERRORS+=$'\n'
    ERRORS+="        Expected: $COMPILER_CODE_EXPECTED"
    ERRORS+=$'\n'
    ERRORS+="        Actual: $COMPILER_CODE"
    ERRORS+=$'\n\n'
    ERRORS+="---------------------------------------"
    ERRORS+=$'\n'
    FAILURES=$(($FAILURES + 1))
    return 1
  fi
  
  if test -f $BIN; then
    local OUTPUT
    OUTPUT=$(${BIN})
    local OUTPUT_CODE=$?
    local OUTPUT_EXPECTED=$4
    local OUTPUT_CODE_EXPECTED=$5
    rm -f $BIN

    if [ "$OUTPUT" != "$OUTPUT_EXPECTED" ]; then
      echo -e "${RED}[FAIL]${NC}: $1"
      ERRORS+="${RED}[FAIL]${NC}: $1 - Program output"
      ERRORS+=$'\n'
      ERRORS+="        Expected: $OUTPUT_EXPECTED"
      ERRORS+=$'\n'
      ERRORS+="        Actual: \"$OUTPUT\""
      ERRORS+=$'\n\n'
      ERRORS+="---------------------------------------"
      ERRORS+=$'\n'
      FAILURES=$(($FAILURES + 1))
      return 1
    fi
    if [ "$OUTPUT_CODE" != "$OUTPUT_CODE_EXPECTED" ]; then
      echo -e "${RED}[FAIL]${NC}: $1"
      ERRORS+="${RED}[FAIL]${NC}: $1 - Program Exit Code"
      ERRORS+=$'\n'
      ERRORS+="        Expected: $OUTPUT_CODE_EXPECTED"
      ERRORS+=$'\n'
      ERRORS+="        Actual: $OUTPUT_CODE"
      ERRORS+=$'\n'
      ERRORS+="---------------------------------------"
      ERRORS+=$'\n'
      FAILURES=$(($FAILURES + 1))
      return 1
    fi
  fi
  echo -e "${GREEN}[PASS]${NC}: $1"
}

allTests

if (($TOTAL != 0)); then
  echo -e '\n\n----------Failure Results--------------\n'
  echo -e "$ERRORS"
  echo "${FAILURES} of ${TOTAL} tests failed."
else
  echo "All tests passed."
fi

