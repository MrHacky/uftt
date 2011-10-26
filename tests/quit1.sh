#!/bin/bash

# Test twice, the first one could close an existing instance
"${UFTT}" --quit
"${UFTT}" --quit
