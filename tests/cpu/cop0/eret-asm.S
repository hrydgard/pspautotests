.set noreorder
.set noat

# void EretTest(u32 statusBits, u32 *results)
# Sets statusBits (2 = EXL, 4 = ERL) in Status, points EPC and ErrorEPC at different places and runs
# eret. results[0]: 1 if it went to EPC, 2 to ErrorEPC, 0 if it fell through. results[1]: Status's
# EXL and ERL bits afterwards. results[2]: 1 if the instruction after eret ran. Status, EPC and
# ErrorEPC are restored before returning. Call with interrupts suspended.
.global EretTest
.ent EretTest
EretTest:
	mfc0	$t0, $12
	mfc0	$t1, $14
	mfc0	$t2, $30
	li	$v0, 0
	li	$v1, 0
	lui	$t3, %hi(ToEpc)
	addiu	$t3, $t3, %lo(ToEpc)
	mtc0	$t3, $14
	lui	$t3, %hi(ToErrorEpc)
	addiu	$t3, $t3, %lo(ToErrorEpc)
	mtc0	$t3, $30
	or	$t4, $t0, $a0
	mtc0	$t4, $12
	sync
	nop
	nop
	eret
	li	$v0, 1
	j	Landed
	nop
ToEpc:
	j	Landed
	li	$v1, 1
ToErrorEpc:
	li	$v1, 2
Landed:
	mfc0	$t5, $12
	mtc0	$t0, $12
	mtc0	$t1, $14
	mtc0	$t2, $30
	sync
	andi	$t5, $t5, 6
	sw	$v1, 0($a1)
	sw	$t5, 4($a1)
	jr	$ra
	sw	$v0, 8($a1)
.end EretTest
