#!/bin/bash

testFile() {
  local FILENAME=$1
  local BIN=${FILENAME%.*}


  local COMPILER=$(./fgc $1 ${FILENAME%.*})
  local COMPILER_RESULT=$?
  local COMPILER_EXPECT="OK"
  local COMPILER_EXPECT_RESULT=0

  if [ "$COMPILER" = "$COMPILER_EXPECT" ]; then
    echo "Compiler: OK"
  else
    echo "Compiler - Expected: $COMPILER_EXPECT"
    echo "Compiler - Actual: $COMPILER"
    echo "Compiler: Fail"
    return 1
  fi

  if [ "$COMPILER_RESULT" = "$COMPILER_EXPECT_RESULT" ]; then
    echo "Compiler Exit Code: OK"
  else
    echo "Compiler Exit Code - Expected: $COMPILER_EXPECT_RESULT"
    echo "Compiler Exit Code - Actual: $COMPILER_RESULT"
    echo "Compiler Exit: Fail"
    return 1
  fi

  local OUTPUT=$($BIN)
  local OUTPUT_EXPECTED="hello world"

  if [ "$OUTPUT" = "$OUTPUT_EXPECTED" ]; then
    echo "Program: OK"
  else
    echo "Program: Fail"
    echo "Expected Output: $OUTPUT_EXPECTED"
    echo "Program Output: $OUTPUT"
    return 1
  fi
}

testFile $1
