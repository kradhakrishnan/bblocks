#!/bin/bash

OBJDIR=../build
BMARK=$OBJDIR/test/perf/fs/bmark_aio
LOGFILE=/tmp/log

iosizes=( 512 8192 )
qdepths=( 16 32 )

rm -f $LOGFILE
fallocate -l 1G /tmp/disk

for iosize in ${iosizes[@]}; do
	for qdepth in ${qdepths[@]}; do
		echo "** Testing iosize $iosize B qdepth $qdepth"

		$BMARK --devpath /tmp/disk --devsize 1 --iosize $iosize --iotype write \
			--iopattern seq --qdepth $qdepth --s 5 >> $LOGFILE 2>&1 || exit -1
		$BMARK --devpath /tmp/disk --devsize 1 --iosize $iosize --iotype write \
			--iopattern random --qdepth $qdepth --s 5 >> $LOGFILE 2>&1 || exit -1
		$BMARK --devpath /tmp/disk --devsize 1 --iosize $iosize --iotype read \
			--iopattern seq --qdepth $qdepth --s 5 >> $LOGFILE 2>&1 || exit -1
		$BMARK --devpath /tmp/disk --devsize 1 --iosize $iosize --iotype read \
			--iopattern random --qdepth $qdepth --s 5 >> $LOGFILE 2>&1 || exit -1
	done
done
