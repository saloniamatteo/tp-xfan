#!/bin/sh
# This script sets the level/speed of the ThinkPad fan.
# Must be run as root.
# Written by Salonia Matteo on 28/05/2021.

# check if we are root
if [ $EUID != 0 ]; then
	printf "You must be root to run this! Exiting with ENOTRECOVERABLE.\n"
	exit 131 # ENOTRECOVERABLE
fi

# Check if user passed an argument
if [ -z "$1" ]; then
	printf "Please enter a value! Exiting with EILSEQ.\n"
	exit 84 # EILSEQ
else
	level="$1"
fi

# Check speed level
if [[ $level = 0 || $level = "auto" ]]; then
	level="auto"
elif [[ $level = 8 || $level = "max" ]]; then
	level="disengaged"
elif [[ $level < 0 || $level > 8 ]]; then
	printf "Speed level not valid! Exiting with EOVERFLOW.\n"
	exit 75 # EOVERFLOW
fi

while [ $(cat /proc/acpi/ibm/fan | grep level -m1 | awk '{print $2}') != "$level" ]; do
	echo "level $level" | $root tee "/proc/acpi/ibm/fan"
done

exit 0 # Success
