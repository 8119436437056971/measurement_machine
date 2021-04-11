echo "build "

export PICO_SDK_PATH=~/shared/pico-sdk

if true ; then
    rm build/measurement.elf
    rm generated/measurement.pio
fi

mkdir -p build
cd build

# export TYPE=Debug
export TYPE=Production

cmake -GNinja -DCMAKE_BUILD_TYPE=$TYPE ..

ninja -j 4
if [ $? -ne 0 ]
then
   echo "error in build  $?"
   cd ..
   exit 1
fi

cd ..

# load file into pico as described in openocd-usage

openocd -f interface/raspberrypi-swd.cfg -f target/rp2040.cfg -c "program build/pio_sample.elf verify reset exit"
~/picotool/build/picotool info -a build/pio_sample.elf




