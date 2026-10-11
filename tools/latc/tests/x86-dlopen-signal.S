.text

.global latc_plugin_signal_site
.global latc_plugin_signal_fault
.global latc_plugin_signal_resume
.type latc_plugin_signal_site,@function
latc_plugin_signal_site:
    movabs $0x445566778899aabb, %r8
    mov $42, %eax
    xor %edx, %edx
    xor %ecx, %ecx
latc_plugin_signal_fault:
    div %ecx
latc_plugin_signal_resume:
    xor %eax, %eax
    ret
.size latc_plugin_signal_site, .-latc_plugin_signal_site

.section .note.GNU-stack,"",@progbits
