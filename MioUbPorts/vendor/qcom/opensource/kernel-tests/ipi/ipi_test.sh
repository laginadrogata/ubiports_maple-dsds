# Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

if [ -d /system/lib/modules/ ]; then
	modpath=/system/lib/modules
else
	modpath=/kernel-tests/modules/lib/modules/$(uname -r)/extra
fi

# Mount debugfs
mount -t debugfs nodev /sys/kernel/debug 2>/dev/null

CPUHOTPLUG_DIR="/sys/devices/system/cpu"
CORECTL_MODULE="/system/lib/modules/core_ctl.ko"
CORECTL_PRESENT=$(lsmod | grep core_ctl)
sys_core_ctl0="$CPUHOTPLUG_DIR/cpu0/core_ctl/disable"
sys_core_ctl4="$CPUHOTPLUG_DIR/cpu4/core_ctl/disable"
core_ctl_config="none"

get_num_cpu(){
num_cpu=`ls $CPUHOTPLUG_DIR | grep "cpu[0-9]" | wc -l`
}

ipi_test_mod=${modpath}/ipi_test_module.ko
ipi_test_iter="/sys/kernel/debug/ipi_test/iteration"
ipi_test_result="/sys/kernel/debug/ipi_test/result"
ipi_test_start="/sys/kernel/debug/ipi_test/start"

# Begin script execution here
# Sets 10000 as a default interation number of times to test IPI
test_iter=10000
test_type=1
verbose=0

while [ $# -gt 0 ]
do
	case $1 in
	-n | --nominal)
		shift 1
		;;
	-a | --adversarial)
		shift 1
		;;
	-r | --repeatability)
		test_iter=1000000
		shift 1
		;;
	-s | --stress)
		test_type=3
		test_iter=500000
		shift 1
		;;
	-v | --verbose)
		verbose=1
		shift 1
		;;
	-t | --times)
		test_iter=$2
		shift 2
		;;
	-e | --test_type)
		test_type=$2
		shift 2
		;;
	-h | --help | *)
	echo " Usage: $0 [[(-t | --times) <iterations>] [(-e | --test_type) <test_type>]"
	echo "		 [-s | --stress] [-v | --verbose]"
	echo " iterations: the times number of ipi to test"
	echo " test_type: "
	echo "1. Use smp_single: send IPI one by one to all cores"
	echo "2. Use smp_many: send IPI to all cores at once"
	echo "3. Use Both: (1 & 2)"
	echo "Recommended maximum iterations: 1000000"
	exit 1
	;;
	esac
done

save_disable_core_ctl() {

    if [ -n "$CORECTL_PRESENT" ]; then
	if [ "$verbose" -gt 0 ];then
	    echo "Core control module detected. Saving and disabling core control..."
	fi
	core_ctl_config="module"
    elif [  -f $sys_core_ctl0 ];then
	if [ "$verbose" -gt 0 ];then
	    echo "Core control sys entry detected. Saving and disabling core control..."
	fi
	core_ctl_config="static"
    else
	if [ "$verbose" -gt 0 ];then
		echo "Core control is not available.."
	fi
	return 1
    fi

    for i in $(seq 0 $num_cpu_test)
    do
        if [ -d $CPUHOTPLUG_DIR/cpu$i/core_ctl ]; then
            CORECTLDIR=$CPUHOTPLUG_DIR/cpu$i/core_ctl
            BUSY_DOWN_THRES[i]=$(cat $CORECTLDIR/busy_down_thres)
            BUSY_UP_THRES[i]=$(cat $CORECTLDIR/busy_up_thres)
            IS_BIG_CLUSTER[i]=$(cat $CORECTLDIR/is_big_cluster)
            MAX_CPUS[i]=$(cat $CORECTLDIR/max_cpus)
            MIN_CPUS[i]=$(cat $CORECTLDIR/min_cpus)
            OFFLINE_DELAY_MS[i]=$(cat $CORECTLDIR/offline_delay_ms)
            TASK_THRES[i]=$(cat $CORECTLDIR/task_thres)
	    DISABLE[i]=$(cat $CORECTLDIR/disable)
        fi
    done

    if [ "$core_ctl_config" = "module" ]; then
	RQ_AVG_PERIOD_MS=$(cat /sys/module/core_ctl/parameters/rq_avg_period_ms)
	rmmod core_ctl
    else
	echo 1 > $sys_core_ctl0
	if [ -f $sys_core_ctl4 ]; then
	    echo 1 > $sys_core_ctl4
	fi
    fi

    # Put all CPUs back online
    for i in $(seq 0 $num_cpu_test)
    do
        echo 1 > $CPUHOTPLUG_DIR/cpu$i/online
    done
}

enable_restore_core_ctl() {

    if [ "$core_ctl_config" = "module" ]; then
	if [ "$verbose" -gt 0 ];then
	     echo "Insert core control module"
	fi
	insmod $CORECTL_MODULE
    elif [ "$core_ctl_config" = "static" ]; then
	if [ "$verbose" -gt 0 ];then
	     echo "Restore previous core control state in sysfs.."
	fi
	echo ${DISABLE[0]} > $sys_core_ctl0
	if [ -f $sys_core_ctl4 ]; then
	     echo ${DISABLE[4]} > $sys_core_ctl4
	fi
    else
	if [ "$verbose" -gt 0 ];then
	    echo "Core control not available.."
	fi
	return 1
    fi

    for i in $(seq 0 $num_cpu_test)
    do
        if [ -d $CPUHOTPLUG_DIR/cpu$i/core_ctl ]; then
            CORECTLDIR=$CPUHOTPLUG_DIR/cpu$i/core_ctl
            echo ${BUSY_DOWN_THRES[$i]} > $CORECTLDIR/busy_down_thres
            echo ${BUSY_UP_THRES[$i]} > $CORECTLDIR/busy_up_thres
            echo ${IS_BIG_CLUSTER[$i]} > $CORECTLDIR/is_big_cluster
            echo ${MIN_CPUS[$i]} > $CORECTLDIR/min_cpus
            echo ${MAX_CPUS[$i]} > $CORECTLDIR/max_cpus
            echo ${OFFLINE_DELAY_MS[$i]} > $CORECTLDIR/offline_delay_ms
            echo ${TASK_THRES[$i]} > $CORECTLDIR/task_thres
        fi
    done

    if [ "$core_ctl_config" = "module" ]; then
	echo $RQ_AVG_PERIOD_MS > /sys/module/core_ctl/parameters/rq_avg_period_ms
    fi
}

ipi_test(){

	get_num_cpu
	num_cpu_test=$(($num_cpu - 1))

	# insert ipi_test_module
	insmod $ipi_test_mod
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to load module $ipi_test_mod"
		exit 1
	fi

	# Remove core ctl to avoid cpu hotplug in middle of the test.
	save_disable_core_ctl

	if [ "$test_iter" -gt 1000000 ]; then
		echo "Recommended iterations are: 1000000"
		echo "so truncated to max iterations."
		test_iter=1000000
	fi

	echo $test_iter > ${ipi_test_iter}
	echo $test_type > ${ipi_test_start}
	if [ $? -eq 0 ]; then
		echo "Max time taken for IPI: (nsec)"
		cat ${ipi_test_start}
		res=0
	else
		res=1
	fi

	if [ "$verbose" -gt 0 ]; then
		cat ${ipi_test_result}
	fi

	enable_restore_core_ctl
	#remove ipi_test_module after test
	rmmod ipi_test_module > /dev/null 2>&1
	if [ $? -ne 0 ]; then
		echo "Failed to remove module $ipi_test_mod"
		res=1
	fi

	if [ "$res" -eq 0 ]; then
		echo "IPI Test: PASS.."
		exit 0
	else
		echo "IPI Test: FAILED."
		exit 1
	fi
}

ipi_test $test_iter $test_type
