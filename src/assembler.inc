execute_instruction_assembly_dispatch:
    ; ESI = slot token corrente, EDI = buffer di output in cui scrivere
    ; Coordina l'avanzamento dei bit interpellando i motori in targets/

    mov    al, [active_isa_target]      ; Carica l'ID dell'architettura attiva
    cmp    al, 1                        ; Controllo target x86
    je     .call_x86_engine
    cmp    al, 2                        ; Controllo target ARM
    je     .call_arm_engine
    cmp    al, 3                        ; Controllo target RISC-V
    je     .call_riscv_engine
    ret                                 ; Salta l'istruzione se non supportata

.call_x86_engine:
    call   assemble_x86_instruction_node ; Salta al calcolatore x86_core.inc
    mov    [output_current_ptr], edi    ; Aggiorna lo stato di scrittura in RAM
    ret

.call_arm_engine:
    call   assemble_arm_instruction_node ; Salta al calcolatore arm_core.inc
    mov    [output_current_ptr], edi    ; Aggiorna lo stato di scrittura in RAM
    ret

.call_arm_engine:
.call_riscv_engine:
    call   assemble_riscv_instruction_node ; Salta al calcolatore riscv_core.inc
    mov    [output_current_ptr], edi    ; Aggiorna lo stato di scrittura in RAM
    ret
