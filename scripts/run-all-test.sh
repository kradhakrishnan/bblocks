#!/bin/bash

function quit()
{
	code=$1

	if [ $code -eq 0 ]; then
		echo '** PASSED **'
	else
		echo '** FAILED **'
	fi

	exit $code
}

function run_tests()
{
	args=$1

	echo '-------------------------------------------------------------'
	echo "Running tests [ $args ]"
	echo '-------------------------------------------------------------'

	make clean &&
	make $args run-unit-test &&
	make $args run-valgrind-test

	[ $? -eq 0 ] || quit -1
}

run_tests ""
run_tests "ERRCHECK=enable"
run_tests "OPT=enable DEBUG=disable"

quit 0
