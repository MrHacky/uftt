#!/bin/bash

# strict mode, all commands must have 0 exit code
set -e

# Start an uftt instance in the background, and wait for it to spin up
# UFTTPID is set to the pid of the started instance
function start_uftt_bg {
	if (nc -h 2>&1 | grep -o -i bsd); then
		nc -l 47187&
	else
		nc -l -p 47187&
	fi
	NCPID=$!

	"${UFTT}" --notify-socket 47187&
	UFTTPID=$!

	wait ${NCPID}
}

# Find script location
SCRIPTDIR="$(cd $(dirname "$0"); pwd)"

# Make temp location
TEMPDIR="$(mktemp -d --tmpdir uftt.test.XXXXXXXXXX)"

# Enter temp location
cd "${TEMPDIR}"

# Create local settings file for tests to use
touch uftt.dat

# Set uftt Location
UFTT="$2"

# Execute test script
source "${SCRIPTDIR}/$1"

# Clean up temp location
cd ..
rm -r "${TEMPDIR}"
