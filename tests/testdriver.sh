#!/bin/bash

# strict mode, all commands must have 0 exit code
set -e

# Enter script location
cd "$(dirname "$0")"

# Get scriptname
TESTSCRIPT="$1"
UFTT="$2"
source "${TESTSCRIPT}"
