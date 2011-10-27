#!/bin/bash

# Create input/output dirs
mkdir in out

# Create test file
dd if=/dev/urandom of=in/file1 bs=1M count=1

# Add share, Download share, quit
"${UFTT}" --add-share in/file1 --download-share uftt-v2://127.0.0.1:47189/file1 out/ --quit

# Check download succeeded
diff -r in out
