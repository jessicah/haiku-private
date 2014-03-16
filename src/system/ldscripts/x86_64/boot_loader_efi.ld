/* Same as elf_x86_64_fbsd_efi.lds, except for OUTPUT_FORMAT, _ctor_list and
	 .rela.ctors - KEEP IN SYNC */
OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)
SECTIONS
{
	. = 0;
	ImageBase = .;
	.hash : { *(.hash) }	/* this MUST come first! */
	. = ALIGN(4096);
	.eh_frame :
	{
		*(.eh_frame)
	}
	. = ALIGN(4096);
	.text :
	{
	 *(.text)
	 *(.text.*)
	 *(.gnu.linkonce.t.*)
	}
	. = ALIGN(4096);
	.reloc :
	{
	 *(.reloc)
	}
	. = ALIGN(4096);
	.data :
	{
	. = ALIGN(0x4);
	__ctor_list = .;
	*(.ctors)
	__ctor_end = .;
	 *(.rodata*)
	 *(.got.plt)
	 *(.got)
	 *(.data*)
	 *(.sdata)
	/* the EFI loader doesn't seem to like a .bss section, so we stick it all into
		.data: */
	 *(.sbss)
	 *(.scommon)
	 *(.dynbss)
	 *(.bss)
	. = ALIGN(0x1000);
	 *(COMMON)
	 *(.rel.local)
	}
	. = ALIGN(4096);
	.dynamic	: { *(.dynamic) }
	. = ALIGN(4096);
	.rela :
	{
		*(.rela.data*)
		*(.rela.ctors)
		*(.rela.got)
		*(.rela.stab)
	}
	. = ALIGN(4096);
	.dynsym	 : { *(.dynsym) }
	. = ALIGN(4096);
	.dynstr	 : { *(.dynstr) }
	. = ALIGN(4096);
	.ignored.reloc :
	{
		*(.rela.reloc)
		*(.eh_frame)
		*(.note.GNU-stack)
	}
	.comment 0 : { *(.comment) }
}
