#!/bin/bash

serverlog=/tmp/server.log
clientlog=/tmp/client.log

function cleanup()
{
	echo '** Killing server'

	pids=`ps -ef | grep bmark_tcp | grep -v grep | awk '{ print $2 }'`

	for pid in $pids;
	do
		echo 'Killing process ' $pid
		kill -9 $pid || echo '** ERROR ** Failed to stop process'
	done
}

function quit()
{
	code=$1

	if [ $code -ne 0 ]; then
		cat $serverlog
		cat $clientlog
		echo '** ERROR **'
	fi

	exit $code
}

function check_precondition()
{
	#
	# Precondition
	# No benchmark should be running or port be used
	#
	echo '** Checking precondition ...'

	ps -ef | grep bmark_tcp | grep -v grep &&
	netstat -plnt | egrep "\:$serverport|\:$clientport"

	if [ $? -eq 0 ]; then
		echo '** ERROR ** Precondition failed'
		exit -1
	fi
}

function run_test()
{
	iosize=$1
	qdepth=$2
	serverport=$3
	clientport=$4

	echo "* Testing iosize $iosize qdepth $qdepth server $serverport client $clientport"

	echo "[ === Server : $serverport ==== ]" >> $serverlog
	echo "[ === Client : $clientport ==== ]" >> $clientlog

	#
	# Run client
	#
	../build/test/perf/net/bmark_tcp --client --laddr "127.0.0.1:$clientport" \
					 --raddr "127.0.0.1:$serverport" --iosize 4000 \
					 --conn 1 --s 5 >> $clientlog 2>&1

	if [ $? -ne 0 ]; then
		echo '** ERROR ** error running benchmark client'
		quit -1
	fi
}

#
# Run client for varied IO size and qdepth
#
echo '** Running test'

cleanup
check_precondition

serverport=8000
clientport=9000

#
# Start the server
#
echo '** Starting server ...'

../build/test/perf/net/bmark_tcp --server --laddr "127.0.0.1:$serverport" >> $serverlog 2>&1 &

#
# Wait for the server to start listening
#
while [ $? -ne 0 ];  do
	echo '** Waiting for server'
	sleep 1

	netstat -plnt | grep LISTEN | grep bmark_tcp | grep -q 127\.0\.0\.1\:$serverport
done

#
# Run tests
#
iosizes=( 2000, 4000, 8000 )
qdepths=( 1, 8, 16, 32 )

rm -f $serverlog $clientlog || exit -1

for iosize in "${iosizes[@]}"; do
	for qdepth in "${qdepths[@]}"; do
		run_test $iosize $qdepth $serverport $clientport
		clientport=$((clientport + 1))
	done 
done

#
# Cleanup
#
cleanup

echo '** DONE **'

quit 0
