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


DEVICES_PATH="/sys/bus/platform/devices/"
DCC_NUM=$(ls $DEVICES_PATH | grep -e "dcc$")
CORESIGHT_BUS_PATH="/sys/bus/coresight/"
CORESIGHT_DEVICES_PATH="/sys/bus/coresight/devices/"
DCC_PATH=$DEVICES_PATH$DCC_NUM
READ_PATH="/system/bin/r"

if [[ $(($# * 1)) -ne 2 ]]; then
	echo "sh dcc.sh <IMEM_ADDR> <NUM_OF_REGS> should be input\n"
	echo "For example : sh dcc.sh 0x8600000 10\n"
	echo "Test Failed\n"
	exit 1
fi

IMEM_ADDR=$1

sink[0]="sram"
sink[1]="atb"

Dec_to_Hex() {
	Dec=$1
	Hex=`printf "%08x" $Dec`
	echo 0x$Hex
}

if [ ! -d $CORESIGHT_BUS_PATH ]; then
	echo "CoreSight Drivers aren't enable in this build\n"
	echo "Test Failed!!\n"
	exit 1
fi

if [ ! -d $DCC_PATH ]; then
	echo "DCC isn't included in this build\n"
	echo "Test Failed!!"
	exit 1
fi

if [ !  -d "/data/coresight-test" ]; then
	mkdir -p "/data/coresight-test"
fi

for x in $(seq 0 1)

do

	for i in $(seq 1 $2)

	do
		k=$(($i-1))
		ADDRi=$(($k*4+$IMEM_ADDR))
		ADDRi=$(Dec_to_Hex $ADDRi)
		DATAVal=$((256+$k*4))
		DATAVal=$(Dec_to_Hex $DATAVal)
		#write data_val with /system/bin/r addr_val
		$READ_PATH $ADDRi $DATAVal
	done

	echo 0 > $DCC_PATH/enable
	echo cap > $DCC_PATH/func_type
	if [ $x -eq 1 ]; then
		echo 1 > $CORESIGHT_DEVICES_PATH/coresight-tpdm-dcc/enable
		echo 1 > $CORESIGHT_DEVICES_PATH/coresight-tmc-etr/curr_sink
		echo 1 > $CORESIGHT_DEVICES_PATH/coresight-tmc-etf/curr_sink
	fi
	echo ${sink[$x]} > $DCC_PATH/data_sink
	echo 1 > $DCC_PATH/config_reset
	echo $(Dec_to_Hex $IMEM_ADDR) $2 > $DCC_PATH/config
	echo 1 > $DCC_PATH/enable
	echo 0 > $CORESIGHT_DEVICES_PATH/coresight-stm/enable
	echo 1 > $DCC_PATH/trigger
	echo 1 > $CORESIGHT_DEVICES_PATH/coresight-stm/enable

	if [ $x -eq 0 ]; then
		echo "Saving dumps for SRAM as sink in /data/sram.bin."
		cat /dev/dcc_sram > /data/sram.bin
	elif [ $x -eq 1 ]; then
		echo "Saving dumps for ATB as sink in /data/dcc_etf.bin."
		cat /dev/coresight-tmc-etf >  /data/coresight-test/dcc_etf.bin
	fi

	echo "CAPTURE test success!! $2 values starting with $1 address"
	echo "Values are in AP with a=100 d=4 and n=$2"
done

echo "---------------------------------------------------------\n"

echo 0 > $DCC_PATH/enable
echo 1 > $DCC_PATH/config_reset
echo $(Dec_to_Hex $IMEM_ADDR) 1 > $DCC_PATH/config
echo crc > $DCC_PATH/func_type
echo 1 > $DCC_PATH/enable
echo  1 > $DCC_PATH/trigger
$READ_PATH $IMEM_ADDR $(Dec_to_Hex 257)
echo  1 > $DCC_PATH/trigger
sys_ready=`cat $DCC_PATH/ready`

if [ $sys_ready -eq 1 ]; then
	crc_error=`cat $DCC_PATH/crc_error`
	if [ $crc_error -eq 1 ]; then
		echo "CRC test PASSED!"
		exit 0
	else
		echo "CRC test FAILED!"
		exit 1
	fi
else
	echo "System not ready!"
	echo "CRC test FAILED!"
	exit 1
fi
