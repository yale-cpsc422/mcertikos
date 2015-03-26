#!/bin/bash

yellow='\033[1;33m'
green='\033[0;32m'
red='\033[0;31m'
NC='\033[0m' # No Color

function check
{
	if [ $1 -eq 0 ]; then
		echo -e "${green} >>> done! <<< ${NC}"
	else
		echo -e "${red} >>> failed. <<< ${NC}"
		exit 1
	fi
}

function title
{
	echo -e "${yellow} *** $1 *** ${NC}"
}

function clear_screen
{
	clear
	clear
}

function error
{
	echo -e "${red} $1 ${NC}"
}

disk="sdc"
error_file="error.cerr"
build_log="build.log"
error_file_swp=".${error_file}.swp"

# FLAGS="DEBUG_MSG=1 SERIAL_DEBUG=1 PROFILING_ALL=1 CONFIG_APP_USER_PROC=1"
FLAGS="DEBUG_MSG=1 SERIAL_DEBUG=1 CONFIG_APP_USER_PROC=1"

need_clean="no"
use_gcc="no"

for i in "$@"
do
	case $i in
		rebuild)
		need_clean="yes"
		shift
		;;
		gcc)
		use_gcc="yes"
		shift
		;;
		*)
		;;
	esac
done

echo "clean: " $need_clean ", gcc: " $use_gcc
echo "target: " $@

target="$@"
clear_screen

title "make certikos kernel ($FLAGS): $target"


if [ "$need_clean" = "yes" ]; then
	make clean
fi

marg=""
if [ "$use_gcc" = "yes" ]; then
	marg="USE_GCC=1"
fi

make -j ${marg} $FLAGS ${target} 2> ${error_file} 1>> ${error_file} 1> ${build_log}
status=$?

if [ "$status" != "0" ]; then
	echo "compile error, open report viewer..."
	if [ -e "${error_file_swp}" ]; then
		echo "please reopen error report."
	else
	  cat error.cerr
	fi
else
	num_cc=$( grep -c '+ cc\[' build.log )
	num_ccomp=$( grep -c '+ ccomp' build.log )
	num_ld=$( grep -c '+ ld' build.log )
	num_as=$( grep -c '+ as' build.log )
	title "[as: ${num_as}, cc: ${num_cc}, ccomp: ${num_ccomp}, ld: ${num_ld}]"
fi
check $status

title "build image"
sudo ./build_image.py
check $?

title "check $disk is a usb stick"
model=$( cat /sys/block/${disk}/device/model  )

if [[ $model == "USB Flash Drive"* ]]; then
	echo "yes"
else
	echo "no, ${disk} is a ${model}. abort!"
	exit 1
fi

title "write image to usb stick"
sudo dd if=certikos.img of=/dev/${disk} bs=2M conv=notrunc,noerror
check $?

title "all done!"
eject_script="
#!/bin/bash

sync
sudo sync
sudo eject ${disk}
"
echo -e "$eject_script" > eject.sh
chmod u+x eject.sh

echo -e "please use ${green}./eject.sh${NC} to safely remove the USB stick"

