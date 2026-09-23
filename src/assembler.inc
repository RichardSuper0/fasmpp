execute_instruction_assembly_dispatch:
    ; ESI = slot token istruzione, EDI = buffer di output crudo final
    
    movzx  eax, byte [active_isa_target] ; Carica e azzera i bit alti per sicurezza
    test   eax, eax                      ; Target non definito (0)?
    jz     .exit
    
    dec    eax                           ; Se active_isa_target era 1 (x86), ora eax è 0
    jz     .x86
    dec    eax                           ; Se active_isa_target era 2 (ARM), ora eax è 0
    jz     .arm
    dec    eax                           ; Se active_isa_target era 3 (RISC-V), ora eax è 0
    jz     .riscv
.exit:
    ret                                 ; Ritorna se ISA non supportata o invalida

.x86:
    call   assemble_x86_instruction_node ; Salta al sotto-motore in targets/x86_core.inc
    jmp    .update_ptr

.arm:
    call   assemble_arm_instruction_node ; Salta al sotto-motore in targets/arm_core.inc
    jmp    .update_ptr

.riscv:
    call   assemble_riscv_instruction_node ; Salta al sotto-motore in targets/riscv_core.inc

.update_ptr:
    mov    [output_current_ptr], edi    ; Aggiorna centralmente il contatore byte macchina
    ret
