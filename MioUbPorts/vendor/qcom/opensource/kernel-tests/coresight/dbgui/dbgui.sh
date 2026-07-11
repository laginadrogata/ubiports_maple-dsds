#! /bin/sh

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


CORESIGHT_BUS_PATH="/sys/bus/coresight/"
DBGUI_PATH="/sys/bus/coresight/devices/coresight-dbgui/"
READ_PATH="/system/bin/r"

if [[ $(($# * 1)) -ne 2 ]]; then
	echo "sh dbgui.sh <IMEM_ADDR> <NUM_OF_AHB_REGS> should be input\n"
	echo "For example : sh dbgui.sh 0x8600000 0x10\n"
	echo "Test Failed\n"
	exit 1
fi

IMEM_ADDR=$1
MAX_AHB=0x10

Dec_to_Hex() {
	Dec=$1
	Hex=`printf "%08x" $Dec`
	echo 0x$Hex
}

#CoreSight isn't included in perf/Secondary build
if [ ! -d "$CORESIGHT_BUS_PATH" ]; then
	echo "CoreSight Drivers aren't enable in this build\n"
	echo "Test Failed!!\n"
	exit 1
fi

if [ ! -d "$DBGUI_PATH" ]; then
	echo "DebugUI isn't included in this build\n"
	echo "Test Failed!!"
	exit 1
fi

DBGUI_SIZE=`cat $DBGUI_PATH/size`

if [[ $(($2 * 1)) -gt $(( $MAX_AHB * 1)) ]]; then
	echo "Changing Number of AHB registers to $MAX_AHB.\n"
	echo "Maximum $MAX_AHB AHB registers allowed.\n"
	$2=$MAX_AHB
fi

echo 0x0 > $DBGUI_PATH/nr_apb_regs
echo $2 > $DBGUI_PATH/nr_ahb_regs

for i in $(seq 1 $2)

do
	index=$(($i-1))
	k=$(($i-1))
	index=$(Dec_to_Hex $index)
	echo $index > $DBGUI_PATH/addr_idx
	ADDRi=$(($k*4+$IMEM_ADDR))
	ADDRi=$(Dec_to_Hex $ADDRi)
	echo $ADDRi > $DBGUI_PATH/addr_val
	DATAVal=$((0x00001000+$k*4))
	DATAVal=$(Dec_to_Hex $DATAVal)
	#write data_val with /system/bin/r addr_val
	$READ_PATH $ADDRi $DATAVal
done

echo 0 > $DBGUI_PATH/capture_enable
echo "echo 0 > $DBGUI_PATH/capture_enable"
echo 1 > $DBGUI_PATH/capture_enable
echo "echo 1 > $DBGUI_PATH/capture_enable"
echo 1 > $DBGUI_PATH/sw_trig
echo "echo 1 > $DBGUI_PATH/sw_trig"

for j in $(seq 1 $2)

do
	k=$(($j-1))
	ADDRj=$(($k*4+$IMEM_ADDR))
	ADDRj=$(Dec_to_Hex $ADDRj)
	DATAVal=$((0x00001000+$k*4))
	DATAVal=$(Dec_to_Hex $DATAVal)
	DATA0=`cat $DBGUI_PATH/data_val | sed -n ${j}p| cut -b 8-16`

	if [ 0x$DATA0 != $DATAVal ]; then
		echo "debugUI" $j "Test Failed"
		exit 1
	else
		echo $j is "passed"
	fi
done

#fire a software trigger

echo "Test Passed!!"
