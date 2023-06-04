#!/bin/bash
ERRORS=""
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
TOTAL=0
FAILURES=0

# Tests are defined here so make adding them easier
allTests() {
  testFile examples/error.fg "[line 3; pos 1] Error at '}': Expect ';' after expression."$'\n'"Fail" 1
  testFile examples/empty.fg "OK" 1 
  testFile examples/helloworld.fg "OK" 0 "hello world" 0
  testFile examples/minimal.fg "OK" 0 "" 0
  testFile examples/return.fg "OK" 0 "" 42
  testFile examples/arithmetic.fg "OK" 0 "" 1
  testFile examples/char-ops.fg "OK" 0 "OO" 0
  testFile examples/local-allocation.fg "OK" 0 "" 1
  testFile examples/variables.fg "OK" 0 "60" 0
  testFile examples/nestblock.fg "OK" 0 "4020" 0
  testFile examples/do-while-loop.fg "OK" 0 "LOOP" 0
  testFile examples/negative-numbers.fg "OK" 0 "-54 -121 -12" 0
  testFile examples/controlflow.fg "OK" 0 "42"$'\n'"0"$'\n'"-54" 0
  testFile examples/global-var.fg "OK" 0 "554 01235" 0
  testFile examples/global-const.fg "OK" 0 "42AQ"$'\n'"01234" 0
  testFile examples/duplicate.fg "[line 4; pos 7] variable \"i\" is already defined."$'\n'"Fail" 1 
  testFile examples/assign-constant.fg "[line 4; pos 5] attempting to assign to read-only constant \"i\"."$'\n'"Fail" 1 
  testFile examples/duplicate-constant.fg "[line 4; pos 9] constant \"i\" is already defined."$'\n'"Fail" 1 
  testFile examples/duplicate-functions.fg "[line 3; pos 7] function \"main\" is already defined."$'\n'"Fail" 1 
  testFile examples/incompatible-assignment.fg "[line 4; pos 9] Incompatible assignment for variable 'i'"$'\n'"                Expected type 'i8' but instead found 'char'"$'\n'"Fail" 1 
  testFile examples/wrong-init-array.fg "[line 3; pos 20] Incompatible record initializer for an array of 'char'."$'\n'"Fail" 1 
  testFile examples/wrong-init-literal.fg "[line 3; pos 7] Attempting to initialize []char using literal '2345'."$'\n'"Fail" 1 
  testFile examples/arithmetic-incompatible.fg "[line 3; pos 12] Invalid operands to arithmetic operator '+'"$'\n'"                 Operands were of type 'number' and 'char', which are incompatible."$'\n'"Fail" 1 
  testFile examples/bitwise-incompatible.fg "[line 3; pos 12] Invalid operands to bitwise operator '&'"$'\n'"                 Operands were of type 'number' and 'char', which are incompatible."$'\n'"Fail" 1 
  testFile examples/strings.fg "OK" 0 "hello world abcde" 0
  testFile examples/init-array.fg "OK" 0 "656667" 0
  testFile examples/array.fg "OK" 0 "worldhellohellx" 0
  testFile examples/global-string.fg "OK" 0 "c Words are cool" 0
  testFile examples/record-basic.fg "OK" 0 "542Hello" 0
  testFile examples/record-global.fg "OK" 0 "5 42 abcde Hello" 0
  testFile examples/record-args.fg "OK" 0 "5 abcde Hello world" 0
}

testFile() {
  local FILENAME=$1
  local BIN="${FILENAME##*/}"
  local BIN=obj/${BIN%.*}


  local COMPILER
  local COMPILER_CODE
  COMPILER=$(./fgc $FILENAME $BIN)
  COMPILER_CODE=$?

  local COMPILER_EXPECT=$2
  local COMPILER_CODE_EXPECTED=$3
  TOTAL=$(($TOTAL + 1))

  if [[ "$COMPILER" != *"$COMPILER_EXPECT"* ]]; then
    echo -e "${RED}[FAIL]${NC}: $1"
    ERRORS+="${RED}[FAIL]${NC}: $1 - Compiler Output"
    ERRORS+=$'\n'
    ERRORS+="        Expected: $COMPILER_EXPECT"
    ERRORS+=$'\n'
    ERRORS+="        Actual: \"$COMPILER \""
    ERRORS+=$'\n'
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
    ERRORS+=$'\n'
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
      ERRORS+="        Expected: \"$OUTPUT_EXPECTED\""
      ERRORS+=$'\n'
      ERRORS+="        Actual: \"$OUTPUT\""
      ERRORS+=$'\n'
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

if (($FAILURES != 0)); then
  echo -e '\n----------Failure Results--------------'
  echo -e "$ERRORS"
  echo "${FAILURES} of ${TOTAL} tests failed."
  exit 1;
else
  echo "All tests passed."
fi

