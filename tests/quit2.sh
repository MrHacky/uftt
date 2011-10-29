#!/bin/bash

start_uftt_bg

"${UFTT}" --quit

wait ${UFTTPID}
