MEMORY {
	ROM (rx) : ORIGIN = 0x00000000, LENGTH = 96K
	RAM (rw) : ORIGIN = 0x10000000, LENGTH = 96K
}

SECTIONS {

	.text : {
		KEEP(*(.irq_vectors))
		*(.text*)
		*(.rodata*)
	} > ROM

	. = ALIGN(4);

	__text_end = .;

	.data : AT (__text_end) {
		__data_start = .;
		*(.data*)
		__data_end = .;
	} > RAM

	.bss : {
		__bss_start = .;
		*(.bss*)
		__bss_end = .;
	} > RAM

	__ram_end = ORIGIN(RAM) + LENGTH(RAM);
}

