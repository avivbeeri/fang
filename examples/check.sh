#!/bin/bash

testFile() {
  local FILENAME=$1
  local BIN=${FILENAME%.*}

  local COMPILER=$(./fgc $1 ${FILENAME%.*})
  local COMPILER_RESULT=$?

  local COMPILER_EXPECT=$2
  
  local COMPILER_EXPECT_RESULT=0

  if [ "$COMPILER" != "$COMPILER_EXPECT" ]; then
    echo "Compiler - Expected: $COMPILER_EXPECT"
    echo "Compiler - Actual: \"$COMPILER \""
    echo "[FAIL]: $1 - Compiler"
    return 1
  fi

  if [ "$COMPILER_RESULT" != "$COMPILER_EXPECT_RESULT" ]; then
    echo "Compiler Exit Code - Expected: $COMPILER_EXPECT_RESULT"
    echo "Compiler Exit Code - Actual: $COMPILER_RESULT"
    echo "[FAIL]: $1 - Compiler exit code"
    return 1
  fi
  
  if test -f $BIN; then
    local OUTPUT=$($BIN)
    local OUTPUT_EXPECTED=$3

    if [ "$OUTPUT" != "$OUTPUT_EXPECTED" ]; then
      echo "Expected Output: $OUTPUT_EXPECTED"
      echo "Program Output: $OUTPUT"
      echo "[FAIL]: $1 - Program"
      return 1
    fi
  fi
  echo "[PASS]: $1"
}

testFile examples/error.fg "[line 3; pos 1] Error at '}': Expect ';' after expression."
testFile examples/helloworld.fg "OK" "hello world"
