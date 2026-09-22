.set noreorder

# void minmax_pressure(const int *in21, const float *pairs, int *out21, float *results)
#
# JIT stress test, see minmax.c. One straight-line block with 21 integer registers live and
# dirty across a run of vmin/vmax on pairs that include NaNs. A JIT needs a scratch integer
# register for the NaN handling in those, and with this many live it has to spill one to get
# it. The spill has to happen on every path through the min/max, not just the NaN one.
#
# pairs: 8 x (a.q, b.q) -> results: 8 x min.q, then 8 x max.q

.global minmax_pressure
.ent    minmax_pressure
minmax_pressure:
	addiu $sp, $sp, -48
	sw    $s0, 0($sp)
	sw    $s1, 4($sp)
	sw    $s2, 8($sp)
	sw    $s3, 12($sp)
	sw    $s4, 16($sp)
	sw    $s5, 20($sp)
	sw    $s6, 24($sp)
	sw    $s7, 28($sp)
	sw    $s8, 32($sp)

	lw    $t0, 0($a0)
	lw    $t1, 4($a0)
	lw    $t2, 8($a0)
	lw    $t3, 12($a0)
	lw    $t4, 16($a0)
	lw    $t5, 20($a0)
	lw    $t6, 24($a0)
	lw    $t7, 28($a0)
	lw    $t8, 32($a0)
	lw    $t9, 36($a0)
	lw    $s0, 40($a0)
	lw    $s1, 44($a0)
	lw    $s2, 48($a0)
	lw    $s3, 52($a0)
	lw    $s4, 56($a0)
	lw    $s5, 60($a0)
	lw    $s6, 64($a0)
	lw    $s7, 68($a0)
	lw    $s8, 72($a0)
	lw    $v0, 76($a0)
	lw    $v1, 80($a0)

	# Dirty every one of them.
	addiu $t0, $t0, 1
	addiu $t1, $t1, 1
	addiu $t2, $t2, 1
	addiu $t3, $t3, 1
	addiu $t4, $t4, 1
	addiu $t5, $t5, 1
	addiu $t6, $t6, 1
	addiu $t7, $t7, 1
	addiu $t8, $t8, 1
	addiu $t9, $t9, 1
	addiu $s0, $s0, 1
	addiu $s1, $s1, 1
	addiu $s2, $s2, 1
	addiu $s3, $s3, 1
	addiu $s4, $s4, 1
	addiu $s5, $s5, 1
	addiu $s6, $s6, 1
	addiu $s7, $s7, 1
	addiu $s8, $s8, 1
	addiu $v0, $v0, 1
	addiu $v1, $v1, 1

	lv.q   C100, 0($a1)
	lv.q   C110, 16($a1)
	lv.q   C120, 32($a1)
	lv.q   C130, 48($a1)
	lv.q   C200, 64($a1)
	lv.q   C210, 80($a1)
	lv.q   C220, 96($a1)
	lv.q   C230, 112($a1)
	lv.q   C300, 128($a1)
	lv.q   C310, 144($a1)
	lv.q   C320, 160($a1)
	lv.q   C330, 176($a1)
	lv.q   C400, 192($a1)
	lv.q   C410, 208($a1)
	lv.q   C420, 224($a1)
	lv.q   C430, 240($a1)

	vmin.q C500, C100, C110
	vmin.q C510, C120, C130
	vmin.q C520, C200, C210
	vmin.q C530, C220, C230
	vmin.q C600, C300, C310
	vmin.q C610, C320, C330
	vmin.q C620, C400, C410
	vmin.q C630, C420, C430
	vmax.q C700, C100, C110
	vmax.q C710, C120, C130
	vmax.q C720, C200, C210
	vmax.q C730, C220, C230
	vmax.q C000, C300, C310
	vmax.q C010, C320, C330
	vmax.q C020, C400, C410
	vmax.q C030, C420, C430

	sv.q   C500, 0($a3)
	sv.q   C510, 16($a3)
	sv.q   C520, 32($a3)
	sv.q   C530, 48($a3)
	sv.q   C600, 64($a3)
	sv.q   C610, 80($a3)
	sv.q   C620, 96($a3)
	sv.q   C630, 112($a3)
	sv.q   C700, 128($a3)
	sv.q   C710, 144($a3)
	sv.q   C720, 160($a3)
	sv.q   C730, 176($a3)
	sv.q   C000, 192($a3)
	sv.q   C010, 208($a3)
	sv.q   C020, 224($a3)
	sv.q   C030, 240($a3)

	sw    $t0, 0($a2)
	sw    $t1, 4($a2)
	sw    $t2, 8($a2)
	sw    $t3, 12($a2)
	sw    $t4, 16($a2)
	sw    $t5, 20($a2)
	sw    $t6, 24($a2)
	sw    $t7, 28($a2)
	sw    $t8, 32($a2)
	sw    $t9, 36($a2)
	sw    $s0, 40($a2)
	sw    $s1, 44($a2)
	sw    $s2, 48($a2)
	sw    $s3, 52($a2)
	sw    $s4, 56($a2)
	sw    $s5, 60($a2)
	sw    $s6, 64($a2)
	sw    $s7, 68($a2)
	sw    $s8, 72($a2)
	sw    $v0, 76($a2)
	sw    $v1, 80($a2)

	lw    $s0, 0($sp)
	lw    $s1, 4($sp)
	lw    $s2, 8($sp)
	lw    $s3, 12($sp)
	lw    $s4, 16($sp)
	lw    $s5, 20($sp)
	lw    $s6, 24($sp)
	lw    $s7, 28($sp)
	lw    $s8, 32($sp)
	jr    $ra
	addiu $sp, $sp, 48
.end minmax_pressure
