#! /bin/sh

arm-none-eabi-as -mcpu=cortex-m4 -mthumb -o startup.o startup.s
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -nostdlib -c -o main.o main.c
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -nostdlib -Wl,--script=lpc4320.lds,-Map=main.map -o main.elf main.o startup.o
arm-none-eabi-objcopy -O binary main.elf main.bin
cp main.bin tmp.dfu
dfu-suffix -a tmp.dfu
dfu-prefix -s 0 -a tmp.dfu
python2 -c "import os.path; import struct; print('0000000: da ff ' + ' '.join(map(lambda s: '%02x' % ord(s), struct.pack('<H', os.path.getsize('main.bin') / 512 + 1    ))) + ' ff ff ff ff')" | xxd -g1 -r > header.bin
cat header.bin tmp.dfu > main.dfu
rm header.bin tmp.dfu
# dfu-util -v -D main.dfu
