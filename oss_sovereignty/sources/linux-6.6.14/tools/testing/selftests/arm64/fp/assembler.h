



#ifndef ASSEMBLER_H
#define ASSEMBLER_H

.macro __for from:req, to:req
	.if (\from) == (\to)
		_for__body %\from
	.else
		__for \from, %(\from) + ((\to) - (\from)) / 2
		__for %(\from) + ((\to) - (\from)) / 2 + 1, \to
	.endif
.endm

.macro _for var:req, from:req, to:req, insn:vararg
	.macro _for__body \var:req
		.noaltmacro
		\insn
		.altmacro
	.endm

	.altmacro
	__for \from, \to
	.noaltmacro

	.purgem _for__body
.endm

.macro function name
	.macro endfunction
		.type \name, @function
		.purgem endfunction
	.endm
\name:
.endm

.macro define_accessor name, num, insn
	.macro \name\()_entry n
		\insn \n, 1
		ret
	.endm

function \name
	adr	x2, .L__accessor_tbl\@
	add	x2, x2, x0, lsl #3
	br	x2

.L__accessor_tbl\@:
	_for x, 0, (\num) - 1, \name\()_entry \x
endfunction

	.purgem \name\()_entry
.endm



.macro puts string
	.pushsection .rodata.str1.1, "aMS", @progbits, 1
.L__puts_literal\@: .string "\string"
	.popsection

	ldr	x0, =.L__puts_literal\@
	bl	puts
.endm

#endif 
