format binary                           ; Sputa codice macchina crudo senza header OS
org 0x0                                 ; Base flat per il caricamento del blob in RAM

; --- CONFIGURAZIONE E MACRO ---
include 'src/macro.inc'                 ; Macro velocizzate per la gestione dei bit

; --- PUNTO DI INGRESSO (BOOTSTRAP) ---
include 'src/trigger.inc'               ; Gestore CLI polimorfico a 1 o 2 argomenti

; --- PIPELINE CORE DELL'ASSEMBLATORE ---
include 'src/lexer.inc'                 ; Flusso caratteri e gestione include
include 'src/tokenizer.inc'             ; Affettatrice in slot da 64 byte per cache L1
include 'src/parser.inc'                ; Cervello del codice e calcolatore simboli
include 'src/assembler.inc'             ; Regista centrale dell'emissione dei bit

; --- SOTTO-MOTORI DI CODIFICA (TARGETS INCLUSI) ---
include 'src/targets/x86_core.inc'      ; Generatore bit x86 (16-bit fino a v5/APX)
include 'src/targets/arm_core.asm'      ; Generatore bit ARM (v1-v9)
include 'src/targets/riscv_core.inc'    ; Generatore bit RISC-V (Tutte le versioni)

; --- BLOCCO UNICO DEI DATI STATICI (A CODA DEL FILE) ---
align 64                                ; Allineamento rigido per la cache L1
source_file_path_ptr dd 0               ; Indirizzo stringa percorso file sorgente
output_file_path_ptr dd 0               ; Indirizzo stringa percorso file output
output_base_ptr      dd 0               ; Indirizzo RAM inizio codice macchina generato
output_current_ptr   dd 0               ; Puntatore di scrittura corrente per gli opcodes
core_error_code      dd 0               ; Registro codice errore interno

active_isa_target          db 0         ; ISA attiva (1=x86, 2=ARM, 3=RISC-V)
default_host_isa_context   db 1         ; Contesto predefinito del blob corrente (1=x86)
output_format_type         db 0         ; Formato: 0=binary, 1=ELF32, 2=ELF64, 3=PE32, 4=PE64
location_counter           dd 0         ; Il sacro registro hardware '$' interno
hardware_entry_point       dd 0         ; Registro RAM per salvare l'indirizzo di entry

include_path_buffer    rb 256           ; Buffer temporaneo percorso include
include_file_load_area rb 256 * 1024    ; RAM per caricamento file .inc ricorsivi
token_scratch_matrix   rb 64 * 16384    ; Matrice slot token allineata a 64 byte
