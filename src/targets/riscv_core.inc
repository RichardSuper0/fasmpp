; ============================================================
; BACKEND RISC-V BASE
; ============================================================

assemble_riscv_instruction_node:
    ; Supporto:
    ;     add  x1, x2, x3
    ;     addi x1, x2, 10

    cmp byte [esi+0], 'a'
    jne .unsupported

    cmp byte [esi+1], 'd'
    jne .unsupported

    cmp byte [esi+2], 'd'
    jne .check_addi

    mov byte [riscv_fmt_type], 1
    jmp .decode_operands

.check_addi:
    cmp byte [esi+1], 'd'
    jne .unsupported

    cmp byte [esi+2], 'd'
    jne .unsupported

    mov byte [riscv_fmt_type], 1

.decode_operands:
    lea eax, [esi+64]
    mov esi, eax
    call extract_riscv_reg_id
    mov [riscv_rd], al

    add esi, 64
    call extract_riscv_reg_id
    mov [riscv_rs1], al

    add esi, 64

    mov al, [esi]
    cmp al, 'x'
    je  .third_is_register

    cmp al, 'z'
    je  .third_is_register

    mov byte [riscv_fmt_type], 2
    call parse_riscv_immediate
    mov [riscv_imm], eax
    jmp .emit_i_type

.third_is_register:
    mov byte [riscv_fmt_type], 1
    call extract_riscv_reg_id
    mov [riscv_rs2], al

    ; ADD rd, rs1, rs2
    mov eax, 0x33

    movzx ebx, byte [riscv_rd]
    inject_reg_field eax, ebx, 7

    movzx ebx, byte [riscv_rs1]
    inject_reg_field eax, ebx, 15

    movzx ebx, byte [riscv_rs2]
    inject_reg_field eax, ebx, 20

    mov [edi], eax
    add edi, 4
    add dword [location_counter], 4
    ret

.emit_i_type:
    ; ADDI rd, rs1, imm
    mov eax, 0x13

    movzx ebx, byte [riscv_rd]
    inject_reg_field eax, ebx, 7

    movzx ebx, byte [riscv_rs1]
    inject_reg_field eax, ebx, 15

    mov ebx, [riscv_imm]
    inject_imm_field eax, ebx, 0xFFF, 20

    mov [edi], eax
    add edi, 4
    add dword [location_counter], 4
    ret

.unsupported:
    mov dword [core_error_code], 0x05
    ret


extract_riscv_reg_id:
    ; ESI = token registro
    ; AL  = registro 0-31

    cmp byte [esi+0], 'z'
    jne .check_alias

    cmp byte [esi+1], 'e'
    jne .check_alias

    cmp byte [esi+2], 'r'
    jne .check_alias

    cmp byte [esi+3], 'o'
    jne .check_alias

    xor eax, eax
    ret

.check_alias:
    cmp byte [esi+0], 'r'
    jne .check_sp

    cmp byte [esi+1], 'a'
    jne .check_sp

    mov al, 1
    ret

.check_sp:
    cmp byte [esi+0], 's'
    jne .check_gp

    cmp byte [esi+1], 'p'
    jne .check_gp

    mov al, 2
    ret

.check_gp:
    cmp byte [esi+0], 'g'
    jne .numeric

    cmp byte [esi+1], 'p'
    jne .numeric

    mov al, 3
    ret

.numeric:
    cmp byte [esi+0], 'x'
    jne .invalid

    movzx eax, byte [esi+1]
    sub eax, '0'

    cmp eax, 9
    ja  .invalid

    movzx ebx, byte [esi+2]
    test bl, bl
    jz   .done

    cmp bl, '0'
    jb  .done

    cmp bl, '9'
    ja  .done

    sub ebx, '0'
    imul eax, 10
    add eax, ebx

.done:
    mask_reg_id eax
    ret

.invalid:
    xor eax, eax
    ret


parse_riscv_immediate:
    ; Supporta interi decimali positivi e negativi.
    ; ESI = token immediato
    ; EAX = valore risultante

    xor eax, eax
    xor ecx, ecx

    cmp byte [esi], '-'
    jne .digits

    mov ecx, 1
    inc esi

.digits:
    movzx edx, byte [esi]

    test dl, dl
    jz   .finish

    cmp dl, '0'
    jb  .finish

    cmp dl, '9'
    ja  .finish

    sub edx, '0'
    imul eax, 10
    add eax, edx

    inc esi
    jmp .digits

.finish:
    test ecx, ecx
    jz   .done

    neg eax

.done:
    ret


align_to_l1

riscv_fmt_type    db 1
riscv_rd          db 0
riscv_rs1         db 0
riscv_rs2         db 0
riscv_imm         dd 0
