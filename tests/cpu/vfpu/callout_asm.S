.set noreorder

# void callout_fpu(const float *in24, const float *angles4, float *out24, float *sincos8)
#
# JIT stress test, see callout.c. One straight-line block with 24 FPU registers live and dirty
# across VFPU sin and cos, which a JIT typically implements by calling a host function.
# Anything the host call is allowed to clobber must have been saved by then, or the values
# written back at the end are wrong.

.global callout_fpu
.ent    callout_fpu
callout_fpu:
	addiu $sp, $sp, -64
	swc1  $f20, 0($sp)
	swc1  $f21, 4($sp)
	swc1  $f22, 8($sp)
	swc1  $f23, 12($sp)
	swc1  $f24, 16($sp)
	swc1  $f25, 20($sp)
	swc1  $f26, 24($sp)
	swc1  $f27, 28($sp)
	swc1  $f28, 32($sp)
	swc1  $f29, 36($sp)
	swc1  $f30, 40($sp)
	swc1  $f31, 44($sp)

	lwc1  $f0, 0($a0)
	lwc1  $f1, 4($a0)
	lwc1  $f2, 8($a0)
	lwc1  $f3, 12($a0)
	lwc1  $f4, 16($a0)
	lwc1  $f5, 20($a0)
	lwc1  $f6, 24($a0)
	lwc1  $f7, 28($a0)
	lwc1  $f8, 32($a0)
	lwc1  $f9, 36($a0)
	lwc1  $f10, 40($a0)
	lwc1  $f11, 44($a0)
	lwc1  $f12, 48($a0)
	lwc1  $f13, 52($a0)
	lwc1  $f14, 56($a0)
	lwc1  $f15, 60($a0)
	lwc1  $f16, 64($a0)
	lwc1  $f17, 68($a0)
	lwc1  $f18, 72($a0)
	lwc1  $f19, 76($a0)
	lwc1  $f20, 80($a0)
	lwc1  $f21, 84($a0)
	lwc1  $f22, 88($a0)
	lwc1  $f23, 92($a0)

	# Make every one of them dirty: value + value.
	add.s $f0, $f0, $f0
	add.s $f1, $f1, $f1
	add.s $f2, $f2, $f2
	add.s $f3, $f3, $f3
	add.s $f4, $f4, $f4
	add.s $f5, $f5, $f5
	add.s $f6, $f6, $f6
	add.s $f7, $f7, $f7
	add.s $f8, $f8, $f8
	add.s $f9, $f9, $f9
	add.s $f10, $f10, $f10
	add.s $f11, $f11, $f11
	add.s $f12, $f12, $f12
	add.s $f13, $f13, $f13
	add.s $f14, $f14, $f14
	add.s $f15, $f15, $f15
	add.s $f16, $f16, $f16
	add.s $f17, $f17, $f17
	add.s $f18, $f18, $f18
	add.s $f19, $f19, $f19
	add.s $f20, $f20, $f20
	add.s $f21, $f21, $f21
	add.s $f22, $f22, $f22
	add.s $f23, $f23, $f23

	# The call-outs. Four angles in, sin and cos of each out.
	lv.q   C100, 0($a1)
	vsin.q C110, C100
	vcos.q C120, C100
	sv.q   C110, 0($a3)
	sv.q   C120, 16($a3)

	swc1  $f0, 0($a2)
	swc1  $f1, 4($a2)
	swc1  $f2, 8($a2)
	swc1  $f3, 12($a2)
	swc1  $f4, 16($a2)
	swc1  $f5, 20($a2)
	swc1  $f6, 24($a2)
	swc1  $f7, 28($a2)
	swc1  $f8, 32($a2)
	swc1  $f9, 36($a2)
	swc1  $f10, 40($a2)
	swc1  $f11, 44($a2)
	swc1  $f12, 48($a2)
	swc1  $f13, 52($a2)
	swc1  $f14, 56($a2)
	swc1  $f15, 60($a2)
	swc1  $f16, 64($a2)
	swc1  $f17, 68($a2)
	swc1  $f18, 72($a2)
	swc1  $f19, 76($a2)
	swc1  $f20, 80($a2)
	swc1  $f21, 84($a2)
	swc1  $f22, 88($a2)
	swc1  $f23, 92($a2)

	lwc1  $f20, 0($sp)
	lwc1  $f21, 4($sp)
	lwc1  $f22, 8($sp)
	lwc1  $f23, 12($sp)
	lwc1  $f24, 16($sp)
	lwc1  $f25, 20($sp)
	lwc1  $f26, 24($sp)
	lwc1  $f27, 28($sp)
	lwc1  $f28, 32($sp)
	lwc1  $f29, 36($sp)
	lwc1  $f30, 40($sp)
	lwc1  $f31, 44($sp)
	jr    $ra
	addiu $sp, $sp, 64
.end callout_fpu
