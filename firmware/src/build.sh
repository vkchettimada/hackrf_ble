#! /bin/sh

arm-none-eabi-as -mcpu=cortex-m4 -mthumb -o startup.o startup.s
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -nostdlib -c -o main.o main.c
arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -nostdlib -Wl,--script=lpc4320.lds,-Map=main.map -o main.elf main.o startup.o
arm-none-eabi-objcopy -O binary main.elf main.bin
cp main.bin main.dfu
dfu-suffix -a main.dfu
dfu-prefix -L -a main.dfu
# dfu-util -v -D main.dfu
