#!/bin/sh

# Writer script
# Author: Trapti Damodar Balgi
# References: 1. https://ryanstutorials.net/bash-scripting-tutorial/bash-if-statements.php

# Extract the CL arguments
writefile=$1
writestr=$2

# If both arguments not specified, send out an error
if [ $# -ne 2 ]
then
	echo "ERROR: Expecting 2 parameters"
    echo "Usage: ./writer.sh <file-path> <content-to-be-written-in-file>"
	exit 1
fi

# Lines 22 - 27 were partially generated using ChatGPT at https://chat.openai.com/ with prompts including [Given a filepath, how to create a file if the path doesnt already exist].
# Extract the directory name
directory_path=$(dirname "$writefile")

# Check if directory already exists. If not, create directory. If mkdir fails, exit with an error
if [ ! -d "$directory_path" ]
then   
    mkdir -p "$directory_path"
    if [ $? -ne 0 ]
    then
        echo "ERROR: Could not create directory "$directory_path""
        exit 1
    fi
fi

# Touch file. Creates file if doesn't already exist. If creation fails, exit with an error
touch "$writefile"
if [ $? -ne 0 ]
then
    echo "ERROR: Could not create file "$writefile""
    exit 1
fi

# Write the string to the file
echo "$writestr" > "$writefile"



