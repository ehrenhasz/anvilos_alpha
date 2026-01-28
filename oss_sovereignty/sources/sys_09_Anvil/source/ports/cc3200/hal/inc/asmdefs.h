








































#ifndef __ASMDEFS_H__
#define __ASMDEFS_H__






#ifdef codered





    .syntax unified
    .thumb




#define __LIBRARY__             @
#define __TEXT__                .text
#define __DATA__                .data
#define __BSS__                 .bss
#define __TEXT_NOROOT__         .text




#define __ALIGN__               .balign 4
#define __END__                 .end
#define __EXPORT__              .globl
#define __IMPORT__              .extern
#define __LABEL__               :
#define __STR__                 .ascii
#define __THUMB_LABEL__         .thumb_func
#define __WORD__                .word
#define __INLINE_DATA__

#endif 






#ifdef ewarm




#define __LIBRARY__             module
#define __TEXT__                rseg CODE:CODE(2)
#define __DATA__                rseg DATA:DATA(2)
#define __BSS__                 rseg DATA:DATA(2)
#define __TEXT_NOROOT__         rseg CODE:CODE:NOROOT(2)




#define __ALIGN__               alignrom 2
#define __END__                 end
#define __EXPORT__              export
#define __IMPORT__              import
#define __LABEL__
#define __STR__                 dcb
#define __THUMB_LABEL__         thumb
#define __WORD__                dcd
#define __INLINE_DATA__         data

#endif 






#if defined(gcc)





    .syntax unified
    .thumb




#define __LIBRARY__             @
#define __TEXT__                .text
#define __DATA__                .data
#define __BSS__                 .bss
#define __TEXT_NOROOT__         .text




#define __ALIGN__               .balign 4
#define __END__                 .end
#define __EXPORT__              .globl
#define __IMPORT__              .extern
#define __LABEL__               :
#define __STR__                 .ascii
#define __THUMB_LABEL__         .thumb_func
#define __WORD__                .word
#define __INLINE_DATA__

#endif 






#ifdef rvmdk





    thumb
    require8
    preserve8




#define __LIBRARY__             ;
#define __TEXT__                area ||.text||, code, readonly, align=2
#define __DATA__                area ||.data||, data, align=2
#define __BSS__                 area ||.bss||, noinit, align=2
#define __TEXT_NOROOT__         area ||.text||, code, readonly, align=2




#define __ALIGN__               align 4
#define __END__                 end
#define __EXPORT__              export
#define __IMPORT__              import
#define __LABEL__
#define __STR__                 dcb
#define __THUMB_LABEL__
#define __WORD__                dcd
#define __INLINE_DATA__

#endif 






#if defined(sourcerygxx)





    .syntax unified
    .thumb




#define __LIBRARY__             @
#define __TEXT__                .text
#define __DATA__                .data
#define __BSS__                 .bss
#define __TEXT_NOROOT__         .text




#define __ALIGN__               .balign 4
#define __END__                 .end
#define __EXPORT__              .globl
#define __IMPORT__              .extern
#define __LABEL__               :
#define __STR__                 .ascii
#define __THUMB_LABEL__         .thumb_func
#define __WORD__                .word
#define __INLINE_DATA__

#endif 

#endif 
