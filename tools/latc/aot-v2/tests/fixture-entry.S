.section .text.lat.tu,"ax",@progbits
.align 2
.global lat_aot_fixture_entry
.hidden lat_aot_fixture_entry
.type lat_aot_fixture_entry, @function
lat_aot_fixture_entry:
    addi.d $sp, $sp, -16
    st.d $ra, $sp, 0
    bl lat_aot_runtime_abi_version
    addi.d $a0, $a0, 40
    ld.d $ra, $sp, 0
    addi.d $sp, $sp, 16
    jr $ra
.size lat_aot_fixture_entry, .-lat_aot_fixture_entry

.global lat_aot_fixture_text_end
.hidden lat_aot_fixture_text_end
lat_aot_fixture_text_end:
