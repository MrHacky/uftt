#!/bin/bash

function kill_tree() {
	local target="${1}"
	local pid=
	local ppid=
	local i
	local pidlist=$(ps -f | tail -n +2 | sed -E 's:^ *[^ ]* *([0-9]+) *([0-9]+) .*$:\1 \2:')
	for i in $pidlist; do
		if [ ! -n "$pid" ]; then
			# first in pair
			pid=$i
		else
			# second in pair
			ppid=$i
			(( ppid == target && pid != $$)) && kill_tree $pid
			# reset pid for next pair
			pid=
		fi
	done
	kill $target || true
}

kill_tree "$1"
