#!/bin/bash

if [ $# -ne 2 ]; then
    echo Requires 2 parameters
    exit 1
fi

filesdir=$1
searchstr=$2

if ! [ -d $filesdir ]; then
    echo $filesdir is not a directory
    exit 1
fi

filecount=$(grep -rl $searchstr $filesdir | wc -l)
matchinglinecount=$(grep -r $searchstr $filesdir | wc -l)
echo The number of files are $filecount and the number of matching lines are $matchinglinecount

