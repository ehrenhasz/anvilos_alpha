


_heap_end = ORIGIN(dram0_0_seg) + LENGTH(dram0_0_seg);

PHDRS
{
    dport0_0_phdr PT_LOAD;
    dram0_0_phdr PT_LOAD;
    dram0_0_bss_phdr PT_LOAD;
    iram1_0_phdr PT_LOAD;
    irom0_0_phdr PT_LOAD;
}

ENTRY(firmware_start)
EXTERN(_DebugExceptionVector)
EXTERN(_DoubleExceptionVector)
EXTERN(_KernelExceptionVector)
EXTERN(_NMIExceptionVector)
EXTERN(_UserExceptionVector)

_firmware_size = ORIGIN(irom0_0_seg) + LENGTH(irom0_0_seg) - 0x40200000;

PROVIDE(_memmap_vecbase_reset = 0x40000000);


_memmap_cacheattr_wb_base = 0x00000110;
_memmap_cacheattr_wt_base = 0x00000110;
_memmap_cacheattr_bp_base = 0x00000220;
_memmap_cacheattr_unused_mask = 0xFFFFF00F;
_memmap_cacheattr_wb_trapnull = 0x2222211F;
_memmap_cacheattr_wba_trapnull = 0x2222211F;
_memmap_cacheattr_wbna_trapnull = 0x2222211F;
_memmap_cacheattr_wt_trapnull = 0x2222211F;
_memmap_cacheattr_bp_trapnull = 0x2222222F;
_memmap_cacheattr_wb_strict = 0xFFFFF11F;
_memmap_cacheattr_wt_strict = 0xFFFFF11F;
_memmap_cacheattr_bp_strict = 0xFFFFF22F;
_memmap_cacheattr_wb_allvalid = 0x22222112;
_memmap_cacheattr_wt_allvalid = 0x22222112;
_memmap_cacheattr_bp_allvalid = 0x22222222;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_wb_trapnull);

SECTIONS
{

    .dport0.rodata : ALIGN(4)
    {
        _dport0_rodata_start = ABSOLUTE(.);
        *(.dport0.rodata)
        *(.dport.rodata)
        _dport0_rodata_end = ABSOLUTE(.);
    } >dport0_0_seg :dport0_0_phdr

    .dport0.literal : ALIGN(4)
    {
        _dport0_literal_start = ABSOLUTE(.);
        *(.dport0.literal)
        *(.dport.literal)
        _dport0_literal_end = ABSOLUTE(.);
    } >dport0_0_seg :dport0_0_phdr

    .dport0.data : ALIGN(4)
    {
        _dport0_data_start = ABSOLUTE(.);
        *(.dport0.data)
        *(.dport.data)
        _dport0_data_end = ABSOLUTE(.);
    } >dport0_0_seg :dport0_0_phdr

    .irom0.text : ALIGN(4)
    {
        _irom0_text_start = ABSOLUTE(.);
        *(.irom0.literal .irom.literal .irom.text.literal .irom0.text .irom.text)

        

        *libcrypto.a:(.literal.* .text.*)
        *libnet80211.a:(.literal.* .text.*)
        *libwpa.a:(.literal.* .text.*)
        *libwpa2.a:(.literal.* .text.*)
        *libespnow.a:(.literal.* .text.*)

        

        *py/argcheck.o*(.literal* .text*)
        *py/asm*.o*(.literal* .text*)
        *py/bc.o*(.literal* .text*)
        *py/binary.o*(.literal* .text*)
        *py/builtin*.o*(.literal* .text*)
        *py/compile.o*(.literal* .text*)
        *py/emit*.o*(.literal* .text*)
        *py/persistentcode*.o*(.literal* .text*)
        *py/formatfloat.o*(.literal* .text*)
        *py/frozenmod.o*(.literal* .text*)
        *py/gc.o*(.literal* .text*)
        *py/reader*.o*(.literal* .text*)
        *py/lexer*.o*(.literal* .text*)
        *py/malloc*.o*(.literal* .text*)
        *py/map*.o*(.literal* .text*)
        *py/mod*.o*(.literal* .text*)
        *py/mpprint.o*(.literal* .text*)
        *py/mpstate.o*(.literal* .text*)
        *py/mpz.o*(.literal* .text*)
        *py/native*.o*(.literal* .text*)
        *py/nlr*.o*(.literal* .text*)
        *py/obj*.o*(.literal* .text*)
        *py/opmethods.o*(.literal* .text*)
        *py/pairheap*.o*(.literal* .text*)
        *py/parse*.o*(.literal* .text*)
        *py/profile*.o*(.literal* .text*)
        *py/qstr.o*(.literal* .text*)
        *py/repl.o*(.literal* .text*)
        *py/runtime.o*(.literal* .text*)
        *py/scheduler.o*(.literal* .text*)
        *py/scope.o*(.literal* .text*)
        *py/sequence.o*(.literal* .text*)
        *py/showbc.o*(.literal* .text*)
        *py/smallint.o*(.literal* .text*)
        *py/stackctrl.o*(.literal* .text*)
        *py/stream.o*(.literal* .text*)
        *py/unicode.o*(.literal* .text*)
        *py/vm.o*(.literal* .text*)
        *py/vstr.o*(.literal* .text*)
        *py/warning.o*(.literal* .text*)

        *extmodmain.o(.literal* .text*)
        *fatfs_port.o(.literal* .text*)
        *gccollect.o(.literal* .text*)
        *gchelper.o(.literal* .text*)
        *help.o(.literal* .text*)
        *lexerstr32.o(.literal* .text*)
        *utils.o(.literal* .text*)
        *machine_pin.o(.literal*, .text*)
        *machine_pwm.o(.literal*, .text*)
        *machine_rtc.o(.literal*, .text*)
        *machine_adc.o(.literal*, .text*)
        *machine_uart.o(.literal*, .text*)
        *modpybi2c.o(.literal*, .text*)
        *modmachine.o(.literal*, .text*)
        *machine_wdt.o(.literal*, .text*)
        *machine_spi.o(.literal*, .text*)
        *hspi.o(.literal*, .text*)
        *modesp.o(.literal* .text*)
        *modespnow.o(.literal* .text*)
        *modos.o(.literal* .text*)
        *modlwip.o(.literal* .text*)
        *modsocket.o(.literal* .text*)
        *modonewire.o(.literal* .text*)
        *network_wlan.o(.literal* .text*)
        *esp_mphal.o(.literal* .text*)

        
        
        *py/qstr.o(.rodata.const_pool)
        *.o(.rodata.mp_type_*) 
        *.o(.rodata.*_locals_dict*) 
        *.o(.rodata.mp_module_*) 
        */frozen.o(.rodata.mp_frozen_sizes) 
        */frozen.o(.rodata.mp_frozen_content) 

        
        *
        KEEP (*crtbegin.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*(.ctors))
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*(.dtors))
        
        __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);
        *(.xt_except_desc)
        *(.gnu.linkonce.h.*)
        __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
        *(.xt_except_desc_end)
        *(.dynamic)
        *(.gnu.version_d)
        . = ALIGN(4);        
        _bss_table_start = ABSOLUTE(.);
        LONG(_bss_start)
        LONG(_bss_end)
        _bss_table_end = ABSOLUTE(.);
        _rodata_end = ABSOLUTE(.);
    } >dram0_0_seg :dram0_0_phdr

    .bss ALIGN(8) (NOLOAD) : ALIGN(4)
    {
        . = ALIGN (8);
        _bss_start = ABSOLUTE(.);
        *(.dynsbss)
        *(.sbss)
        *(.sbss.*)
        *(.gnu.linkonce.sb.*)
        *(.scommon)
        *(.sbss2)
        *(.sbss2.*)
        *(.gnu.linkonce.sb2.*)
        *(.dynbss)
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN (8);
        _bss_end = ABSOLUTE(.);
        _heap_start = ABSOLUTE(.);
    } >dram0_0_seg :dram0_0_bss_phdr
}


INCLUDE "boards/eagle.rom.addr.v6.ld"
