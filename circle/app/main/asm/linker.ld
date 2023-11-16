SECTIONS
{
    . = 0x0; /* start address */
    .text : { *(.text) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
