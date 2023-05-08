#!/bin/bash

FILENAME=$1
BIN=${FILENAME%.*}
./fgc $1 ${FILENAME%.*}
VAR1=$($BIN)
VAR2="hello world"

echo "Expected Output: $VAR2"
echo "Program Output: $VAR1"
if [ "$VAR1" = "$VAR2" ]; then
    echo "Strings are equal."
else
    echo "Strings are not equal."
fi

