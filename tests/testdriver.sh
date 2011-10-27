#!/bin/bash

# strict mode, all commands must have 0 exit code
set -e

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
