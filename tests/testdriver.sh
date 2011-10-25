#!/bin/bash

# strict mode, all commands must have 0 exit code
set -e

# Enter script location
cd "$(dirname "$0")"

# Get scriptname
TESTSCRIPT="$1"
# Shift TARGET ($2) so its the first parameter for the test script
shift
# Include the test script
source "${TESTSCRIPT}"
