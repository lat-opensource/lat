.global _start
.text
_start:
 mov 16(%rsp),%rsi
 movzbl (%rsi),%ebx
 cmp $'n',%bl
 je normal
 cmp $'g',%bl
 je group
 cmp $'f',%bl
 je fault
 cmp $'u',%bl
 je illegal
 cmp $'z',%bl
 je divide
 cmp $'k',%bl
 je kill9
 cmp $'t',%bl
 je tgkill9
 cmp $'m',%bl
 je threads
 cmp $'i',%bl
 je disposition
 cmp $'h',%bl
 je disposition
 cmp $'x',%bl
 je seccomp
 cmp $'y',%bl
 je seccomp
 mov %ebx,%esi
 sub $65,%esi
send:
 mov $39,%eax
 syscall
 mov %eax,%edi
 mov $62,%eax
 syscall
 mov $96,%edi
 jmp out
kill9:
 mov $9,%esi
 jmp send
tgkill9:
 mov $39,%eax
 syscall
 mov %eax,%edi
 mov $186,%eax
 syscall
 mov %eax,%esi
 mov $9,%edx
 mov $234,%eax
 syscall
 mov $96,%edi
 jmp out
fault:
 xor %eax,%eax
 mov (%rax),%eax
 jmp normal
illegal:
 ud2
 jmp normal
divide:
 xor %edx,%edx
 mov $1,%eax
 xor %ecx,%ecx
 div %ecx
 jmp normal
threads:
 mov $9,%eax
 xor %edi,%edi
 mov $8192,%esi
 mov $3,%edx
 mov $0x22,%r10d
 mov $-1,%r8
 xor %r9d,%r9d
 syscall
 lea 8192(%rax),%rsi
 mov $0x50f00,%edi
 xor %edx,%edx
 xor %r10d,%r10d
 xor %r8d,%r8d
 mov $56,%eax
 syscall
 test %rax,%rax
 jz child
 xor %edi,%edi
 jmp out
child:
 sub $16,%rsp
 movq $1,(%rsp)
 movq $0,8(%rsp)
 mov %rsp,%rdi
 xor %esi,%esi
 mov $35,%eax
 syscall
group:
 mov $37,%edi
 mov $231,%eax
 syscall
normal:
 mov $37,%edi
out:
 mov $60,%eax
 syscall

disposition:
 sub $32,%rsp
 movq $1,(%rsp)
 cmp $'i',%bl
 je action
 lea handler(%rip),%rax
 mov %rax,(%rsp)
action:
 movq $0x4000000,8(%rsp)
 lea restorer(%rip),%rax
 mov %rax,16(%rsp)
 movq $0,24(%rsp)
 mov $6,%edi
 mov %rsp,%rsi
 xor %edx,%edx
 mov $8,%r10d
 mov $13,%eax
 syscall
 mov $39,%eax
 syscall
 mov %eax,%edi
 mov $6,%esi
 mov $62,%eax
 syscall
 jmp normal
handler:
 ret
restorer:
 mov $15,%eax
 syscall

seccomp:
 mov $157,%eax
 mov $38,%edi
 mov $1,%esi
 xor %edx,%edx
 xor %r10d,%r10d
 xor %r8d,%r8d
 syscall
 sub $32,%rsp
 movq $1,(%rsp)
 lea 16(%rsp),%rax
 mov %rax,8(%rsp)
 mov $6,%eax
 cmp $'x',%bl
 je filter
 movabs $0x8000000000000006,%rax
filter:
 mov %rax,16(%rsp)
 mov $317,%eax
 mov $1,%edi
 xor %esi,%esi
 mov %rsp,%rdx
 syscall
 mov $39,%eax
 syscall
 jmp normal
