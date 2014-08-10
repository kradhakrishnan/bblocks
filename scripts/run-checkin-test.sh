#!/bin/bash

function quit()
{
	code=$1

	if [ $code -eq 0 ]; then
		echo '** PASSED ** Please see /tmp/log for details'
	else
		echo '** FAILED **'
		cat /tmp/log
	fi

	exit $code
}

function run_tests()
{
	args=$1

	echo "** Running tests [ $args ] **"

	make clean >> $log 2>&1 &&
	make $args run-unit-test >> $log 2>&1 &&
	make $args run-valgrind-test >> /tmp/log 2>&1

	[ $? -eq 0 ] || quit -1
}

log=/tmp/log
rm -f $log

run_tests ""
run_tests "ERRCHECK=enable"
run_tests "OPT=enable DEBUG=disable"

quit 0
