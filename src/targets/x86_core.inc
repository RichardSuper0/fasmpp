; ============================================================
; BACKEND X86 BASE
; ============================================================

assemble_x86_instruction_node:
    ; Supporto base:
    ;     add rax, rbx
    ;
    ; Output:
    ;     48 01 D8

    call reset_x86_prefix_states

    cmp byte [esi+0], 'a'
    jne .unsupported

    cmp byte [esi+1], 'd'
    jne .unsupported

    cmp byte [esi+2], 'd'
    jne .unsupported

    ; Destinazione
    lea eax, [esi+64]
    mov esi, eax
    call classify_any_x86_register
    mov [modrm_rm], al

    ; Sorgente
    add esi, 64
    call classify_any_x86_register
    mov [modrm_reg], al

    ; ADD r/m64, r64
    mov al, 0x48
    mov [edi], al
    inc edi

    mov al, 0x01
    mov [edi], al
    inc edi

    mov al, 0xC0

    mov bl, [modrm_reg]
    and bl, 7
    shl bl, 3
    or  al, bl

    mov bl, [modrm_rm]
    and bl, 7
    or  al, bl

    mov [edi], al
    inc edi

    mov dword [location_counter], 3
    ret

.unsupported:
    mov dword [core_error_code], 0x05
    ret


reset_x86_prefix_states:
    mov byte [x86_prefix_type], 0
    mov byte [prefix_size_override], 0
    mov byte [rex_prefix_byte], 0
    mov byte [vex_p0_byte], 0
    mov byte [vex_p1_byte], 0
    mov byte [evex_p0_byte], 0xFC
    mov byte [evex_p1_byte], 0x78
    mov byte [evex_p2_byte], 0
    mov byte [modrm_reg], 0
    mov byte [modrm_rm], 0
    ret


classify_any_x86_register:
    ; ESI = token registro
    ; AL  = numero registro 0-15

    cmp byte [esi+0], 'r'
    jne .invalid

    cmp byte [esi+1], 'a'
    je  .rax

    cmp byte [esi+1], 'c'
    je  .rcx

    cmp byte [esi+1], 'd'
    je  .rdx

    cmp byte [esi+1], 'b'
    je  .rbx

    cmp byte [esi+1], 's'
    je  .rsi_or_rsp

    cmp byte [esi+1], 'd'
    je  .rdx

    cmp byte [esi+1], '8'
    jb  .invalid

    mov al, [esi+1]
    sub al, '0'

    cmp al, 7
    ja  .invalid

    add al, 8
    ret

.rax:
    xor al, al
    ret

.rcx:
    mov al, 1
    ret

.rdx:
    mov al, 2
    ret

.rbx:
    mov al, 3
    ret

.rsi_or_rsp:
    cmp byte [esi+2], 'p'
    je  .rsp

    mov al, 6
    ret

.rsp:
    mov al, 4
    ret

.invalid:
    xor al, al
    ret


align_to_l1

x86_prefix_type      db 0
prefix_size_override db 0
rex_prefix_byte      db 0
vex_p0_byte          db 0
vex_p1_byte          db 0
evex_p0_byte         db 0xFC
evex_p1_byte         db 0x78
evex_p2_byte         db 0
modrm_reg             db 0
modrm_rm              db 0
