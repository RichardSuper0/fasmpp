; ============================================================
; BACKEND ARM BASE
; ============================================================

assemble_arm_instruction_node:
    ; Supporto:
    ;     add w0, w1, w2
    ;     add x0, x1, x2

    cmp byte [esi+0], 'a'
    jne .unsupported

    cmp byte [esi+1], 'd'
    jne .unsupported

    cmp byte [esi+2], 'd'
    jne .unsupported

    lea eax, [esi+64]
    mov esi, eax
    call parse_single_arm_register
    mov [arm_rd], al

    add esi, 64
    call parse_single_arm_register
    mov [arm_rn], al

    add esi, 64
    call parse_single_arm_register
    mov [arm_rm], al

    cmp byte [arm_mode_width], 64
    je  .arm64

    ; ARM32 ADD register:
    ; cond=1110, opcode ADD, Rn/Rd/Rm
    mov eax, 0xE0800000

    movzx ebx, byte [arm_rd]
    inject_reg_field eax, ebx, 12

    movzx ebx, byte [arm_rn]
    inject_reg_field eax, ebx, 16

    movzx ebx, byte [arm_rm]
    inject_reg_field eax, ebx, 0

    mov [edi], eax
    add edi, 4
    add dword [location_counter], 4
    ret

.arm64:
    ; A64 ADD Xd, Xn, Xm
    mov eax, 0x8B000000

    movzx ebx, byte [arm_rd]
    inject_reg_field eax, ebx, 0

    movzx ebx, byte [arm_rn]
    inject_reg_field eax, ebx, 5

    movzx ebx, byte [arm_rm]
    inject_reg_field eax, ebx, 16

    mov [edi], eax
    add edi, 4
    add dword [location_counter], 4
    ret

.unsupported:
    mov dword [core_error_code], 0x05
    ret


parse_single_arm_register:
    ; ESI = token wN, xN o rN
    ; AL  = registro 0-31

    mov al, [esi]

    cmp al, 'w'
    je  .arm64_register

    cmp al, 'x'
    je  .arm64_register

    cmp al, 'r'
    je  .arm32_register

    xor eax, eax
    ret

.arm64_register:
    mov byte [arm_mode_width], 64
    jmp .parse_number

.arm32_register:
    mov byte [arm_mode_width], 32

.parse_number:
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


align_to_l1

arm_mode_width db 64
arm_rd         db 0
arm_rn         db 0
arm_rm         db 0
