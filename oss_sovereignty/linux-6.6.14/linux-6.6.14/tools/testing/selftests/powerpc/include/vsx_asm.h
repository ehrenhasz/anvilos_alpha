#include "basic_asm.h"
FUNC_START(load_vsx)
	li	r5,0
	lxvd2x	vs20,r5,r3
	addi	r5,r5,16
	lxvd2x	vs21,r5,r3
	addi	r5,r5,16
	lxvd2x	vs22,r5,r3
	addi	r5,r5,16
	lxvd2x	vs23,r5,r3
	addi	r5,r5,16
	lxvd2x	vs24,r5,r3
	addi	r5,r5,16
	lxvd2x	vs25,r5,r3
	addi	r5,r5,16
	lxvd2x	vs26,r5,r3
	addi	r5,r5,16
	lxvd2x	vs27,r5,r3
	addi	r5,r5,16
	lxvd2x	vs28,r5,r3
	addi	r5,r5,16
	lxvd2x	vs29,r5,r3
	addi	r5,r5,16
	lxvd2x	vs30,r5,r3
	addi	r5,r5,16
	lxvd2x	vs31,r5,r3
	blr
FUNC_END(load_vsx)
FUNC_START(store_vsx)
	li	r5,0
	stxvd2x	vs20,r5,r3
	addi	r5,r5,16
	stxvd2x	vs21,r5,r3
	addi	r5,r5,16
	stxvd2x	vs22,r5,r3
	addi	r5,r5,16
	stxvd2x	vs23,r5,r3
	addi	r5,r5,16
	stxvd2x	vs24,r5,r3
	addi	r5,r5,16
	stxvd2x	vs25,r5,r3
	addi	r5,r5,16
	stxvd2x	vs26,r5,r3
	addi	r5,r5,16
	stxvd2x	vs27,r5,r3
	addi	r5,r5,16
	stxvd2x	vs28,r5,r3
	addi	r5,r5,16
	stxvd2x	vs29,r5,r3
	addi	r5,r5,16
	stxvd2x	vs30,r5,r3
	addi	r5,r5,16
	stxvd2x	vs31,r5,r3
	blr
FUNC_END(store_vsx)
