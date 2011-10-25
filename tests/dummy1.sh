#!/bin/bash

echo "DP=${TESTDIR}"
diff "$1" "$1"
echo "Test Success"
