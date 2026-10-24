; SPDX-License-Identifier: MIT
; EATLOW.COM: memory requirement test aid with a 1 KB footprint (nasm,
; 8086). Like tools/eatmem.c, but small enough to leave a game nearly all
; of DOS memory: shrinks itself to 64 paragraphs, takes a block so that
; only <KB> kilobytes stay free as the largest block, then runs <program>
; with the rest of its command line and returns its exit code.
;
;   EATLOW <KB> <program> [args]        (nasm -f bin -o EATLOW.COM)
        cpu 8086
        org 100h
start:
        mov sp, stack_top
        mov ah, 4Ah                     ; shrink to 64 paragraphs (1 KB)
        mov bx, 64
        int 21h
        mov si, 81h
.sp1:   lodsb
        cmp al, ' '
        je .sp1
        xor bx, bx                      ; KB to leave
.dig:   cmp al, '0'
        jb .num
        cmp al, '9'
        ja .num
        sub al, '0'
        xor ah, ah
        mov cx, ax
        mov ax, bx
        mov dx, 10
        mul dx
        add ax, cx
        mov bx, ax
        lodsb
        jmp .dig
.num:   mov [keep], bx
.sp2:   cmp al, ' '                     ; program name
        jne .nm
        lodsb
        jmp .sp2
.nm:    mov di, name
.nm1:   cmp al, ' '
        je .nmend
        cmp al, 0Dh
        je .nmend
        stosb
        lodsb
        jmp .nm1
.nmend: mov byte [di], 0
        dec si                          ; the rest of the line is the tail
        mov di, tail + 1
        xor cx, cx
.tl:    lodsb
        stosb
        cmp al, 0Dh
        je .tlend
        inc cx
        jmp .tl
.tlend: mov [tail], cl
        mov ah, 48h                     ; largest block
        mov bx, 0FFFFh
        int 21h
        mov ax, [keep]
        mov cl, 6
        shl ax, cl                      ; KB -> paragraphs
        cmp bx, ax
        jbe .run
        sub bx, ax
        mov ah, 48h
        int 21h
.run:   mov [pb_tail + 2], cs
        mov [pb_f1 + 2], cs
        mov [pb_f2 + 2], cs
        mov [save_sp], sp
        mov ax, cs
        mov es, ax
        mov dx, name
        mov bx, pblock
        mov ax, 4B00h
        int 21h
        mov ax, cs                      ; DOS 2 loses SS:SP across EXEC
        mov ss, ax
        mov sp, [cs:save_sp]
        mov ds, ax
        jc .err
        mov ah, 4Dh
        int 21h
        mov ah, 4Ch
        int 21h
.err:   mov ax, 4C03h
        int 21h

keep:    dw 0
save_sp: dw 0
pblock:  dw 0                           ; inherit the environment
pb_tail: dw tail, 0
pb_f1:   dw 5Ch, 0
pb_f2:   dw 6Ch, 0
name:    times 64 db 0
tail:    times 130 db 0
        align 2
        times 1024 - 256 - 16 - ($ - $$) db 0
stack_top:
