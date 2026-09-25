; ============================================================
; DISPATCHER CENTRALE DEI BACKEND
; ============================================================

execute_instruction_assembly_dispatch:
    ; ESI = slot token istruzione
    ; EDI = buffer di output corrente

    movzx eax, byte [active_isa_target]

    cmp eax, 1
    je  .x86

    cmp eax, 2
    je  .arm

    cmp eax, 3
    je  .riscv

    mov dword [core_error_code], 0x04
    ret

.x86:
    call assemble_x86_instruction_node
    jmp  .update_ptr

.arm:
    call assemble_arm_instruction_node
    jmp  .update_ptr

.riscv:
    call assemble_riscv_instruction_node

.update_ptr:
    mov [output_current_ptr], edi
    ret
