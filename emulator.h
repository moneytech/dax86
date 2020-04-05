#ifndef EMULATOR_H_
#define EMULATOR_H_

#include <stdint.h>

/*
 * In order of REG of ModR/M
 * EAX: 000, ECX: 001 ... EDI: 111
 * 
 * 32  | 16 | 8  | 8
 * EAX | AX | AH | AL
 * EBX | BX | BH | BL
 * ECX | CX | CH | CL
 * EDX | DX | DH | DL
 * ESI
 * EDI
 * ESP
 * EBP
 */
enum Register
{
    EAX,
    ECX,
    EDX,
    EBX,
    ESP,
    EBP,
    ESI,
    EDI,
    REGISTERS_COUNT,
    AL = EAX,
    CL = ECX,
    DL = EDX,
    BL = EBX,
    AH = AL + 4,
    CH = CL + 4,
    DH = DL + 4,
    BH = BL + 4
};

enum SegmentRegister
{
    ES,
    CS,
    SS,
    DS,
    FS,
    GS,
    SEGMENT_REGISTERS_COUNT
};

enum ControlRegister
{
    CR0,
    CR1,
    CR2,
    CR3,
    CR4,
    CONTROL_REGISTER_COUNT
};

/* GDTR:
 * |0      15|16        47|
 * |  LIMIT  |    BASE    |
 * |_________|____________|
 */
typedef struct
{
    uint16_t limit;
    uint32_t base;
} Gdtr;

enum Exception
{
    NO_ERR,
    E_DE,
    E_DB,
    E_BP,
    E_OF,
    E_BR,
    E_UD,
    E_NM,
    E_DF,
    E_TS,
    E_NP,
    E_SS,
    E_GP,
    E_PF,
    E_MF,
    E_AC,
    E_MC,
    E_XF,
    E_VE,
    E_SX
};

/*
 * FLAGS:
 * | n-th bit                                      |
 * | 0   | 1   | 2   | 3   | 4   | 5   | 6   | 7   |
 * | CF  | Rsv | PF  | Rsv | AF  | Rsv | ZF  | SF  |
 * 
 * | 8   | 9   | 10  | 11  | 12  | 13  | 14  | 15  |
 * | TF  | IF  | DF  | OF  |   IOPL    | NT* | Rsv |
 * 
 * EFLAGS:
 * | 16  | 17  | 18  | 19  | 20  | 21  | 22  | 23  |
 * | RF  | VM  | AC  | VIF | VIP | ID  |    Rsv    |
 * 
 * | 24  | 25  | 26  | 27  | 28  | 29  | 30  | 31  |
 * |                     Rsvd                      |
 * 
 * NT: nested task flag
 * 
 * RFLAGS:
 * Rsv...
 */
typedef struct
{
    uint32_t eflags;
    uint32_t registers[REGISTERS_COUNT];
    uint16_t segment_registers[SEGMENT_REGISTERS_COUNT];
    uint32_t control_registers[CONTROL_REGISTER_COUNT];
    uint8_t *memory;
    uint32_t eip;
    Gdtr gdtr;
    uint8_t exception;
} Emulator;

#endif