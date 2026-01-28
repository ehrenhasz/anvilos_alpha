#ifndef _ASM_NIOS2_ASMMACROS_H
#define _ASM_NIOS2_ASMMACROS_H
.macro ANDI32	reg1, reg2, mask
.if \mask & 0xffff
	.if \mask & 0xffff0000
		movhi	\reg1, %hi(\mask)
		movui	\reg1, %lo(\mask)
		and	\reg1, \reg1, \reg2
	.else
		andi	\reg1, \reg2, %lo(\mask)
	.endif
.else
	andhi	\reg1, \reg2, %hi(\mask)
.endif
.endm
.macro ORI32	reg1, reg2, mask
.if \mask & 0xffff
	.if \mask & 0xffff0000
		orhi	\reg1, \reg2, %hi(\mask)
		ori	\reg1, \reg2, %lo(\mask)
	.else
		ori	\reg1, \reg2, %lo(\mask)
	.endif
.else
	orhi	\reg1, \reg2, %hi(\mask)
.endif
.endm
.macro XORI32	reg1, reg2, mask
.if \mask & 0xffff
	.if \mask & 0xffff0000
		xorhi	\reg1, \reg2, %hi(\mask)
		xori	\reg1, \reg1, %lo(\mask)
	.else
		xori	\reg1, \reg2, %lo(\mask)
	.endif
.else
	xorhi	\reg1, \reg2, %hi(\mask)
.endif
.endm
.macro BT	reg1, reg2, bit
.if \bit > 31
	.err
.else
	.if \bit < 16
		andi	\reg1, \reg2, (1 << \bit)
	.else
		andhi	\reg1, \reg2, (1 << (\bit - 16))
	.endif
.endif
.endm
.macro BTBZ	reg1, reg2, bit, label
	BT	\reg1, \reg2, \bit
	beq	\reg1, r0, \label
.endm
.macro BTBNZ	reg1, reg2, bit, label
	BT	\reg1, \reg2, \bit
	bne	\reg1, r0, \label
.endm
.macro BTC	reg1, reg2, bit
.if \bit > 31
	.err
.else
	.if \bit < 16
		andi	\reg1, \reg2, (1 << \bit)
		xori	\reg2, \reg2, (1 << \bit)
	.else
		andhi	\reg1, \reg2, (1 << (\bit - 16))
		xorhi	\reg2, \reg2, (1 << (\bit - 16))
	.endif
.endif
.endm
.macro BTS	reg1, reg2, bit
.if \bit > 31
	.err
.else
	.if \bit < 16
		andi	\reg1, \reg2, (1 << \bit)
		ori	\reg2, \reg2, (1 << \bit)
	.else
		andhi	\reg1, \reg2, (1 << (\bit - 16))
		orhi	\reg2, \reg2, (1 << (\bit - 16))
	.endif
.endif
.endm
.macro BTR	reg1, reg2, bit
.if \bit > 31
	.err
.else
	.if \bit < 16
		andi	\reg1, \reg2, (1 << \bit)
		andi	\reg2, \reg2, %lo(~(1 << \bit))
	.else
		andhi	\reg1, \reg2, (1 << (\bit - 16))
		andhi	\reg2, \reg2, %lo(~(1 << (\bit - 16)))
	.endif
.endif
.endm
.macro BTCBZ	reg1, reg2, bit, label
	BTC	\reg1, \reg2, \bit
	beq	\reg1, r0, \label
.endm
.macro BTCBNZ	reg1, reg2, bit, label
	BTC	\reg1, \reg2, \bit
	bne	\reg1, r0, \label
.endm
.macro BTSBZ	reg1, reg2, bit, label
	BTS	\reg1, \reg2, \bit
	beq	\reg1, r0, \label
.endm
.macro BTSBNZ	reg1, reg2, bit, label
	BTS	\reg1, \reg2, \bit
	bne	\reg1, r0, \label
.endm
.macro BTRBZ	reg1, reg2, bit, label
	BTR	\reg1, \reg2, \bit
	bne	\reg1, r0, \label
.endm
.macro BTRBNZ	reg1, reg2, bit, label
	BTR	\reg1, \reg2, \bit
	bne	\reg1, r0, \label
.endm
.macro TSTBZ	reg1, reg2, mask, label
	ANDI32	\reg1, \reg2, \mask
	beq	\reg1, r0, \label
.endm
.macro TSTBNZ	reg1, reg2, mask, label
	ANDI32	\reg1, \reg2, \mask
	bne	\reg1, r0, \label
.endm
.macro PUSH	reg
	addi	sp, sp, -4
	stw	\reg, 0(sp)
.endm
.macro POP	reg
	ldw	\reg, 0(sp)
	addi	sp, sp, 4
.endm
#endif  
