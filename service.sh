#!/bin/sh

# Starts and stops the server file service. Relies on the server creating a
# `.pid` file in $RUNDIR, so that a signal can be sent to that process to stop
# the service

# directory that the script is lives in.
# From http://stackoverflow.com/questions/59895
RUNDIR="$( cd "$( dirname "$0" )" && pwd )"

server_name="server"
daemon_name="serviced"
pidfile=$RUNDIR/$daemon_name.pid

usage="$0 [start | stop]"

start() {
	if [ -f $pidfile ]
	then
		echo "Daemon is already running"
		exit
	else
		cmd="${RUNDIR}/${server_name} $pidfile"
		echo "Running $cmd"
		$cmd
	fi
}

stop() {
	if [ -f $pidfile ]
	then
		local pid=$(cat $pidfile 2> /dev/null)
		echo "Stopping service"
		kill $pid
	else
		echo "Daemon is not currently running"
		exit
	fi
}


if [ "$#" != 1 ]
then
	echo $usage
	exit
fi

case "$1" in
	"start")
		start
		;;
	"stop")
		stop
		;;
	*)
		echo $usage
		exit
		;;
esac
