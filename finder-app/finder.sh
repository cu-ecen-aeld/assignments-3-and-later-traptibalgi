#!/bin/sh

# Finder script
# Author: Trapti Damodar Balgi
# References: 1. https://ryanstutorials.net/bash-scripting-tutorial/bash-if-statements.php
# 			  2. https://unix.stackexchange.com/questions/1125/how-can-i-get-a-count-of-files-in-a-directory-using-the-command-line

# Extract the CL arguments
filesdir=$1
searchstr=$2

# If both arguments not specified, exit with an error
if [ $# -ne 2 ]
then
	echo "Expecting 2 parameters"
	echo "Usage: ./finder.sh <directory-path> <string-to-be-searched>"
	exit 1
# If $filesdir is not a directory, exit with an error
elif [ ! -d "$filesdir" ] 
then
	echo ""$filesdir" is not a directory!"
	exit 1
fi

# Search for number of files in $filesdir
# This line was based on content at https://unix.stackexchange.com/questions/1125/how-can-i-get-a-count-of-files-in-a-directory-using-the-command-line with modifications for path
num_files=$(find "$filesdir" -type f | wc -l)

# Count the number of matches of $searchstr in $filesdir
instance_count=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are "$num_files" and the number of matching lines are "$instance_count""

# Search for number of files in $filesdir
num_files=$(find "$filesdir" -type f | wc -l)