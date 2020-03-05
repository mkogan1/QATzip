#***************************************************************************
#
#   BSD LICENSE
#
#   Copyright(c) 2007-2020 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#**************************************************************************
#! /bin/bash

#set -e
echo "***QZ_ROOT run_test_sw.sh start"
test_main=${QZ_ROOT}/test/test
echo "test for hw down sw up and down start"
#get the type of QAT hardware
$QZ_TOOL/get_platform/get_platforminfo.sh
platform=`cat $QZ_TOOL/get_platform/PlatformInfo`
echo "platform=$platform"
if [ $platform == "C3000" ]
then
    echo "The SW test case does not need to run on C3000 platform!"
    exit 0
fi
#hw down
$ICP_BUILD_OUTPUT/adf_ctl down


if [[ ($platform == "37c8" || $platform == "C62x" || $platform == "DH895XCC")]]
then
#sw permitted
    $test_main -m 4 -O gzip -B 1 > resultsw 2>&1
    qz_sw_status=$(cat resultsw | grep -c "QZ_NO_HW")
    sw_throught_ave_software=$(cat resultsw | grep -n "INFO" | awk -v sum=0 '{sum+=$8} END{print sum}')
    rm -f resultsw
fi
echo "qz_sw_status=$qz_sw_status"
echo "sw_throught_ave_software=$sw_throught_ave_software"


if [[ ($platform == "37c8" || $platform == "C62x")&& \
       $qz_sw_status != 0 && \
       $(echo "$sw_throught_ave_software < 0.4" | bc) = 1 ]]
then
    echo -e "hw down sw up run test PASSED:)\n"
elif [[ $platform == "DH895XCC" && \
       $qz_sw_status != 0 && \
       $(echo "$sw_throught_ave_software < 2.5" | bc) = 1 ]]
then
    echo -e "hw down sw up run test PASSED:)\n"
else
    echo "hw down sw up run test FAILED!!! :("
#hw up
     $ICP_BUILD_OUTPUT/adf_ctl up
    exit 1
 fi

if [[ ($platform == "37c8" || $platform == "C62x" || $platform == "DH895XCC")]]
then
#sw not permitted
    $test_main -m 4 -B 0 > resultnosw 2>&1
    qz_nosw_status=$(cat resultnosw | grep -c "QZ_NOSW_NO_HW")
    rm -f resultnosw
fi
#deterimine if this test passed
echo "qz_nosw_status=$qz_nosw_status"
if [[ ($platform == "37c8" || $platform == "C62x" || $platform == "DH895XCC")&& \
      $qz_nosw_status != 0 ]]
then
    echo -e "hw down sw down run test PASSED:)\n"
#hw up
    $ICP_BUILD_OUTPUT/adf_ctl up
else
    echo "hw down sw down run test FAILED!!! :("
#hw up
    $ICP_BUILD_OUTPUT/adf_ctl up
    exit 1
fi
echo "***QZ_ROOT run_test_sw.sh end"
exit 0