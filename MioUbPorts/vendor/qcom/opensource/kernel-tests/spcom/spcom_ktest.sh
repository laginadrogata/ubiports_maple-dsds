# Copyright (c) 2016, The Linux Foundation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 and
# only version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

if [ -d /system/lib/modules/ ]; then
        modpath=/system/lib/modules
else
        modpath=/kernel-tests/modules/lib/modules/$(uname -r)/extra
fi

# Mount debugfs
mount -t debugfs nodev /sys/kernel/debug 2>/dev/null

test_module=${modpath}/spcom_ktest.ko
test_module_name="spcom_ktest"

# device node for communication with SPSS main task
test_module_dev=/dev/sp_kernel

#DLKM module params
test_name="link_up_test"
ch_name="sp_kernel"
test_loops=1
check_loopback_data=0

# test_result
test_result=-1

check_dependencies(){
        if [ ! -d $test_module_dev ]; then
                echo "Could not find $test_module_dev ... needs PIL-SPSS"

            if [ ! -d /firmware/image ]; then
                        echo "Mount /firmware"
                        mount -t vfat /dev/block/bootdevice/by-name/modem /firmware

                        #sleep 1s
                        echo get > /sys/kernel/debug/msm_subsys/spss
                        #sleep 5s
                        if [ ! -d $test_module_dev ]; then
                                echo "$test_module_dev is missing."
                                echo "Test failed"
                                exit 1
                        fi
                fi
        fi
}

load_test_module(){
        # insert test_module
        echo "load test module"
        echo "Run: insmod $test_module test_name=$test_name ch_name=$ch_name test_loops=$test_loops check_loopback_data=$check_loopback_data"
        insmod $test_module test_name=$test_name ch_name=$ch_name test_loops=$test_loops check_loopback_data=$check_loopback_data
        test_result=$?
}

unload_test_module(){
        #remove test_module after test
        echo "unload test module"
        lsmod | grep "$test_module_name" >> /dev/null
        if [ $? -eq 0 ]; then
                rmmod $test_module_name
                if [ $? -ne 0 ]; then
                        echo "ERROR: failed to remove module $test_module"
                fi
        fi
}

run_test(){
        check_dependencies

        load_test_module

        unload_test_module

        if [ $test_result -ne 0 ]; then
                echo "Test failed"
                echo "test_result = $test_result"
        else
                echo "Test passed"
        fi
}

#=============================================================================
# Begin script execution here
while [ $# -gt 0 ]
do
        case $1 in
        -n | --nominal)
                test_name="connection_test"
                shift 1
                ;;
        -a | --adversarial)
                test_name="client_test"
                shift 1
                ;;
        -r | --repeatability)
                test_name="client_test"
                test_loops=10
                shift 1
                ;;
        -s | --stress)
                test_name="client_test"
                test_loops=1000
                shift 1
                ;;
        -h | --help | *)
    echo "Usage: $0 [-n | --nominal] [-a | --adversarial] [-r | --repeatability] [-s | --stress]"
        exit 1
        ;;
        esac
done

run_test
