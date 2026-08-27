#!/bin/bash

if [ $# -ne 2 ]; then
    echo Requires 2 parameters
    exit 1
fi

writefile=$1
writestr=$2

mkdir -p "$(dirname "$writefile")"
if [ $? -ne 0 ]; then
    echo Could not create the directory.
    exit 1
fi

echo "$writestr" > $writefile
if [ $? -ne 0 ]; then
    echo Could not write file $writefile.
    exit 1
fi

