/****************************************************************************
 * configs/bambino-200e/scripts/spificonfig.ld
 *
 *   Copyright (C) 2012 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *           Alan Carvalho de Assis acassis@gmail.com [nuttx] <nuttx@yahoogroups.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/
/*
 * Power-Up Reset Overview
 * -----------------------
 *
 * The ARM core starts executing code on reset with the program counter set
 * to 0x0000 0000.  The LPC43xx contains a shadow pointer register that
 * allows areas of memory to be mapped to address 0x0000 0000. The default,
 * reset value of the shadow pointer is 0x1040 0000 so that on reset code in
 * the boot ROM is always executed first.
 *
 * The boot starts after reset is released.  The IRC is selected as CPU clock
 * and the Cortex-M4 starts the boot loader. By default the JTAG access to the
 * chip is disabled at reset.  The boot ROM determines the boot mode based on
 * the OTP BOOT_SRC value or reset state pins.  For flash-based parts, the part
 * boots from internal flash by default.  Otherwse, the boot ROM copies the
 * image to internal SRAM at location 0x1000 0000, sets the ARM's shadow
 * pointer to 0x1000 0000, and jumps to that location.
 *
 * Of course, using JTAG the executable image can be also loaded directly
 * into and executed from SRAM.
 */

/* The LPC4330 on the LPC4330-Xplorer has the following memory resources:
 *
 * 1. 4096Kb of SPIFI FLASH beginning at address 0x1400:0000
 * 2. 264KB of total SRAM:
 *    a. 128KB of SRAM in the CPU block beginning at address 0x1000:0000
 *    b. 72KB beginning at address 0x1008:0000 and
 *    c. 64KB of AHB SRAM in three banks beginning at addresses 0x2000:0000,
 *       0x2000:8000 and 0x2000:C000.
 * 3. No internal FLASH
 *
 * Here we assume that:
 *
 * 1. We will be running out of SPIFI flash at 0x1400:0000, and
 * 2. All .data and .bss will all fit into the 128KB CPU SRAM block.
 */

MEMORY
{
    progmem (rx)  : ORIGIN = 0x14000000, LENGTH = 1024K
    datamem (rwx) : ORIGIN = 0x10000000, LENGTH = 128K

    SRAM2 (rwx) : ORIGIN = 0x10080000, LENGTH = 72K
    AHB0_RAM(rwx) : ORIGIN = 0x20000000, LENGTH = 32K
    AHB1_RAM(rwx) : ORIGIN = 0x20008000, LENGTH = 16K
    ETB(rwx) :  ORIGIN = 0x2000C000, LENGTH = 16K
}

OUTPUT_ARCH(arm)
ENTRY(ResetISR)         /* Treat __start as the anchor for dead code stripping */
EXTERN(.isr_vector)       /* Force the vectors to be included in the output */
SECTIONS
{
	.text : {
		_stext = ABSOLUTE(.);
		*(.vectors)
		*(.text .text.*)
		*(.fixup)
		*(.gnu.warning)
		*(.rodata .rodata.*)
		*(.gnu.linkonce.t.*)
		*(.glue_7)
		*(.glue_7t)
		*(.got)
		*(.gcc_except_table)
		*(.gnu.linkonce.r.*)
		_etext = ABSOLUTE(.);
	} > progmem

	.init_section : {
		_sinit = ABSOLUTE(.);
		*(SORT_BY_NAME(.init_array.*))
		*(.init_array)
		_einit = ABSOLUTE(.);
	} > progmem

	.ARM.extab : {
		*(.ARM.extab*)
	} > progmem

	__exidx_start = ABSOLUTE(.);
	.ARM.exidx : {
		*(.ARM.exidx*)
	} > progmem
	__exidx_end = ABSOLUTE(.);

	_eronly = ABSOLUTE(.);

	.data : {
		_sdata = ABSOLUTE(.);
		*(.data .data.*)
		*(.gnu.linkonce.d.*)
        . = ALIGN(4);
        *(.ramfunctions) /* !!!! Placing functions in .ramfunctions section in RAM */
		SORT_BY_NAME(CONSTRUCTORS)
		_edata = ABSOLUTE(.);
	} > datamem AT > progmem

	.bss : {			/* BSS				*/
		_sbss = ABSOLUTE(.);
		*(.bss .bss.*)
		*(.gnu.linkonce.b.*)
		*(COMMON)
		_ebss = ABSOLUTE(.);
	} > datamem



/* Code can explicitly ask for data to be
       placed in these higher RAM banks where
       they will be left uninitialized.
    */
    .AHBSRAM0 (NOLOAD):
    {
        Image$$RW_IRAM2$$Base = . ;
        PROVIDE(__AHB0_block_start = .);
        *(AHBSRAM0)
        Image$$RW_IRAM2$$ZI$$Limit = .;
        PROVIDE(__AHB0_dyn_start = .);
        PROVIDE(__AHB0_end = ORIGIN(AHB0_RAM) + LENGTH(AHB0_RAM));
    } > AHB0_RAM

    .AHBSRAM1 (NOLOAD):
    {
        Image$$RW_IRAM3$$Base = . ;
        PROVIDE(__AHB1_block_start = .);
        *(AHBSRAM1)
        Image$$RW_IRAM3$$ZI$$Limit = .;
        PROVIDE(__AHB1_dyn_start = .);
        PROVIDE(__AHB1_end = ORIGIN(AHB1_RAM) + LENGTH(AHB1_RAM));
    } > AHB1_RAM

					/* Stabs debugging sections.	*/
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_info 0 : { *(.debug_info) }
	.debug_line 0 : { *(.debug_line) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	.debug_aranges 0 : { *(.debug_aranges) }
}
