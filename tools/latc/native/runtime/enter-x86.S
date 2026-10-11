.text
.align 2
.global lat_native_enter_x86_static_exec
.type lat_native_enter_x86_static_exec, @function
lat_native_enter_x86_static_exec:
    move $t0, $a0
    move $s8, $a1
    move $s4, $a2
    move $fp, $a3
    move $s0, $zero
    move $s1, $zero
    move $s2, $zero
    move $s3, $zero
    move $s5, $zero
    move $s6, $zero
    move $s7, $zero
    move $a2, $zero
    move $a3, $zero
    move $a4, $zero
    move $a5, $zero
    move $a6, $zero
    move $t5, $zero
    move $t6, $zero
    move $t7, $zero
    move $t8, $zero
    jr $t0
.size lat_native_enter_x86_static_exec, .-lat_native_enter_x86_static_exec
