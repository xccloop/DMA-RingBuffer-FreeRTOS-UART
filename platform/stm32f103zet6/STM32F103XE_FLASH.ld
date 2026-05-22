/******************************************************************************
 * @file     STM32F103XE_FLASH.ld
 * @brief    GCC linker script for STM32F103ZET6
 *           Flash: 512KB, RAM: 64KB
 ******************************************************************************/

MEMORY
{
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx): ORIGIN = 0x20000000, LENGTH = 64K
}

_stack_size  = 0x1000;   /* 4KB stack */
_heap_size   = 0;         /* heap managed by FreeRTOS heap_4 */

ENTRY(Reset_Handler)

SECTIONS
{
  .vectors :
  {
    KEEP(*(.vectors))
    . = ALIGN(4);
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
    *(.glue_7)
    *(.glue_7t)
    . = ALIGN(4);
  } > FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
  .ARM.exidx   : { *(.ARM.exidx* .gnu.linkonce.armexidx.*) } > FLASH
  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(.fini_array*))
    KEEP(*(SORT(.fini_array.*)))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = . + _heap_size;
    . = . + _stack_size;
    . = ALIGN(8);
    PROVIDE(_estack = .);
  } > RAM
}
