#!/bin/ash

# Running mm_bs Function
#
# (C) 2020.03.03 BuddyZhang1 <buddy.zhang@aliyun.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.


OPT=$1
ARGS=$2

init_filter()
{
	echo "*_bs" > /sys/kernel/debug/tracing/set_ftrace_filter
	cat /sys/kernel/debug/tracing/set_ftrace_filter
	echo function > /sys/kernel/debug/tracing/current_tracer
	echo 1 > /sys/kernel/debug/tracing/tracing_on
	echo "Staring tracing .... :)"
}

clear_filter()
{
	echo > /sys/kernel/debug/tracing/set_ftrace_filter
}

trace_on()
{
	echo 1 > /sys/kernel/debug/tracing/tracing_on
}

trace_off()
{
	echo 0 > /sys/kernel/debug/tracing/tracing_on
}

add_filter()
{
	echo $ARGS > /sys/kernel/debug/tracing/set_ftrace_filter
}

sub_filter()
{
	echo $ARGS > /sys/kernel/debug/tracing/set_ftrace_notrace
}

show_trace()
{
	cat /sys/kernel/debug/tracing/trace
}

mount_fs()
{
	insmod /lib/modules/$(uname -r)/extra/mm_bs-0.0.1.ko
#	insmod /lib/modules/$(uname -r)/extra/mm_bs-0.0.1-swap.ko
	mkdir -p /tmpfs_bs
	mount -t tmpfs_bs BiscuitOS_tmpfs /tmpfs_bs
}

umount_fs()
{
	echo "Hello"
}

case $OPT in
	"init")
		init_filter
	;;
	"on")
		trace_on
	;;
	"off")
		trace_off
	;;
	"clear")
		trace_off
		trace_on
	;;
	"add")
		add_filter
	;;
	"sub")
		sub_filter
	;;
	"show")
		show_trace
	;;
	"mount")
		mount_fs
	;;
esac
