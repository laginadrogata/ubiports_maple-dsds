#
# Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#

. $TEST_ENV_SETUP

#Get the command line arguments
cmd_line_args=$@
args_count=$#

#Change the permissions for qcedev_test
chmod 755 qcedev_test

#Target_TYPE is gotten from the TEST_ENV_SETUP script
#8960 target uses the qce4.ko module
#non-8960 targets use qce.ko module
# For Android Targets
if [ $TEST_TARGET -eq "ANDROID"]
	case $TARGET_TYPE in
		8960)
			qce_module_path=/system/lib/modules/qce4.ko
		    ;;
		*)
			qce_module_path=/system/lib/modules/qce.ko
		esac

	#Path of the QCEDEV driver
	qcedev_module_path=/system/lib/modules/qcedev.ko

	#Insert QCE module
	insmod "$qce_module_path"
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to load module $qce_module_path";
		echo "Test Failed";
		exit 1
	fi

	#Insert QCEDEV module
	insmod "$qcedev_module_path"
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to load module $qcedev_module_path"
		echo "Test Failed"
		exit 1
	fi
else
#For Non-Android Targets
	#Insert QCE module
	case $TARGET_TYPE in
		8960)
			modprobe qce40
			if [ $? -ne 0 ]; then
				echo "ERROR: failed to load module qce40.ko"
				echo "Test Failed"
				exit 1
			fi
		    ;;
		*)
			modprobe qce
			if [ $? -ne 0 ]; then
				echo "ERROR: failed to load module qce.ko"
				echo "Test Failed"
				exit 1
			fi
		esac
	#Insert QCEDEV module
	modprobe qcedev
	if [ $? -ne 0 ]; then
		echo "ERROR: failed to load module qcedev.ko"
		echo "Test Failed"
		exit 1
	fi
fi

#Parse the arguments list
#only -n,-a,-r,-s are allowed for automation tests
while [ $# -gt 0 ]; do
        case $1 in
	-n | --nominal)
		shift 1
		;;
	-a | --adversarial)
		shift 1
		;;
	-r | --repeatability)
		shift 1
		;;
	-s | --stress)
		shift 1
		;;
	-h | --help | *)
		exit 1
		;;
        esac
done

#Execute the tests
#If there are 0 arguments the default test case to be run is nominal
# -n option
if [ $args_count -ne 0 ]; then
	./qcedev_test $cmd_line_args
else
	./qcedev_test -n
fi

retval=$?

#Check for pass or fail status
if [ $? -ne 0 ];then
    echo "Test Failed"
    return_value=1;
else
    echo "Test Passed"
    return_value=0;
fi

#Remove Modules QCEDEV and QCE
if [ $TEST_TARGET -eq "ANDROID"]
	rmmod "qcedev"
	case $TARGET_TYPE in
		8960)
			rmmod "qce40"
		    ;;
		*)
			rmmod "qce"
		esac



else
#For Non-Android Targets
	modprobe -r qcedev
	#Remove QCE module
	case $TARGET_TYPE in
		8960)
			modprobe -r qce40
		    ;;
		*)
			modprobe -r qce
		esac
fi

#return with exit value 0(PASS)/1(FAIL)
exit $return_value
