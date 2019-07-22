#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "instructions.h"
#include "emulator_functions.h"
#include "modrm.h"

/*
 * jmp (short): 2 bytes
 * Jumps with 8-bit signed offset.
 * 1 byte: op (EB)
 * 1 byte: offset from eip (8 bit signed) -127 ~ 127
 */
static void short_jump(Emulator *emu)
{
    int8_t offset = get_sign_code8(emu, 1);
    emu->eip += (offset + 2);
}

/*
 * jmp (near): 5 bytes
 * Jumps with 32-bit signed offset.
 * 1 byte: op (E9)
 * 4 byte: offset from eip (32 bit signed)
 */
static void near_jump(Emulator *emu)
{
    int32_t diff = get_sign_code32(emu, 1);
    emu->eip += (diff + 5);
}

/*
 * mov r32 imm32: 5 bytes
 * Copies imm value to register specified in op code (r32: 32 bit register).
 * 1 byte: op (B8) + reg (3bits)
 * 4 bytes: value (32 bit unsigned)
 */
static void mov_r32_imm32(Emulator *emu)
{
    uint8_t reg = get_code8(emu, 0) - 0xB8;
    uint32_t value = get_code32(emu, 1);
    emu->registers[reg] = value; // maps to Register Enum
    emu->eip += 5;
}

/*
 * mov rm32 imm32: 5 bytes
 * Copies imm value to register or memory specified by ModR/M (rm32).
 * 1 byte: op (C7)
 * 1 byte: ModR/M
 * 4 bytes: value (32)
 */
static void mov_rm32_imm32(Emulator *emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t value = get_code32(emu, 0);
    emu->eip += 4;
    set_rm32(emu, &modrm, value);
}

/*
 * mov rm32 r32: 2 bytes
 * Copies value of register specified by REG to ModR/M.
 * 1 byte: op (89)
 * 1 byte: ModR/M
 */
static void mov_rm32_r32(Emulator *emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    /* Reads value from register specified by REG bits. */
    uint32_t r32 = get_r32(emu, &modrm);
    /* Sets value on memory/register specified by ModR/M. */
    set_rm32(emu, &modrm, r32);
}

/*
 * mov r32 rm32: 2 bytes
 * Copies value of ModR/M to REG.
 * 1 byte: op (8B)
 * 1 byte: ModR/M
 */
static void mov_r32_rm32(Emulator *emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    /* Reads value from memory/register specified by ModR/M. */
    uint32_t rm32 = get_rm32(emu, &modrm);
    /* Sets value on register specified by REG bits. */
    set_r32(emu, &modrm, rm32);
}

/*
 * add rm32 r32: 2 bytes
 * Adds value of REG to ModR/M.
 * 1 byte: (01)
 * 1 byte: ModR/M
 */
static void add_rm32_r32(Emulator *emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    set_rm32(emu, &modrm, r32 + rm32);
}

/*
 * add rm32 imm8: 3 bytes
 * Adds imm8 to RM32. Op code 83 and ModR/M op code: 0 execute this.
 * 1 byte: shared op (83)
 * 1 byte: ModR/M
 * 1 byte: imm8 to add
 */
static void add_rm32_imm8(Emulator *emu, ModRM *modrm)
{
    uint32_t rm32 = get_rm32(emu, modrm);
    uint32_t imm8 = (int32_t)get_sign_code8(emu, 0);
    emu->eip += 1;
    set_rm32(emu, modrm, rm32 + imm8);
}

/*
 * sub rm32 imm8: 3 bytes
 * Subtracts imm8 from RM32. Op code 83 and ModR/M op code: 101 execute this.
 * 1 byte: shared op (83)
 * 1 byte: ModR/M
 * 1 byte: imm8 to subtract
 */
static void sub_rm32_imm8(Emulator *emu, ModRM *modrm)
{
    uint32_t rm32 = get_rm32(emu, modrm);
    uint32_t imm8 = (int32_t)get_sign_code8(emu, 0);
    emu->eip += 1;
    uint64_t result = (uint64_t)rm32 - (uint64_t)imm8;

    set_rm32(emu, modrm, result);
    update_eflags_sub(emu, rm32, imm8, result);
}

/*
 * cmp rm32 imm8: 3 bytes
 * Compares RM32 value and imm8 value by subtracting in order.
 * Op code 83 and ModR/M op code: 111 execute this.
 * 1 byte: shared op (83)
 * 1 byte: ModR/M
 * 1 byte: imm8 to subtract
 */
static void cmp_rm32_imm8(Emulator *emu, ModRM *modrm)
{
    uint32_t rm32 = get_rm32(emu, modrm);
    uint32_t imm8 = (int32_t)get_sign_code8(emu, 0);
    emu->eip += 1;
    uint64_t result = (uint64_t)rm32 - (uint64_t)imm8;

    update_eflags_sub(emu, rm32, imm8, result);
}

/*
 * cmp r32 rm32: 2 bytes
 * Compares register 32-bit value and RM32 value by subtracting in order.
 * 1 byte: op (3B)
 * 1 byte: ModR/M
 */
static void cmp_r32_rm32(Emulator *emu)
{
    emu->eip += 1; // op code
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    uint64_t result = (uint64_t)r32 - (uint64_t)rm32;
    update_eflags_sub(emu, r32, rm32, result);
}

static void code_83(Emulator *emu)
{
    /* Proceed 1 byte for op code 83. */
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);

    switch (modrm.opcode)
    {
    case 0:
        add_rm32_imm8(emu, &modrm);
        break;
    case 5:
        sub_rm32_imm8(emu, &modrm);
        break;
    case 7:
        cmp_rm32_imm8(emu, &modrm);
        break;
    default:
        printf("Not implemented: Op: 83 with ModR/M Op: %d\n", modrm.opcode);
        exit(1);
    }
}

/*
 * inc rm32: 2 bytes
 * Increments ModR/M. Op code 83 and ModR/M op code: 000 execute this.
 * 1 byte: shared op (FF)
 */
static void inc_rm32(Emulator *emu, ModRM *modrm)
{
    uint32_t value = get_rm32(emu, modrm);
    set_rm32(emu, modrm, value + 1);
}

static void code_ff(Emulator *emu)
{
    emu->eip += 1;
    ModRM modrm;
    parse_modrm(emu, &modrm);

    switch (modrm.opcode)
    {
    case 0:
        inc_rm32(emu, &modrm);
        break;

    default:
        printf("Not implemented: Op: FF with ModR/M Op: %d\n", modrm.opcode);
        exit(1);
    }
}

/*
 * push r32: 1 bytes
 * Pushes 32-bit value into memory stack from specified register.
 * 1 byte: op (50) + reg
 */
static void push_r32(Emulator *emu)
{
    u_int8_t reg = get_code8(emu, 0) - 0x50;
    push32(emu, get_register32(emu, reg));
    emu->eip += 1;
}

/*
 * push imm32: 5 bytes
 * Pushes 32-bit immediate value into stack.
 * 1 byte: op (68)
 * 4 bytes: immediate 32-bit value
 */
static void push_imm32(Emulator *emu)
{
    uint32_t value = get_code32(emu, 1);
    push32(emu, value);
    emu->eip += 5;
}

/*
 * push imm8: 2 bytes
 * Pushes 8-bit immediate value into stack.
 * 1 byte: op (6A)
 * 1 byte: immediate 8-bit value
 */
static void push_imm8(Emulator *emu)
{
    uint32_t value = get_code8(emu, 1);
    push32(emu, value);
    emu->eip += 2;
}

/*
 * pop r32: 1 bytes
 * Pops 32-bit value into specified register from memory.
 * 1 byte: op (58) + reg
 */
static void pop_r32(Emulator *emu)
{
    u_int8_t reg = get_code8(emu, 0) - 0x58;
    set_register32(emu, reg, pop32(emu));
    emu->eip += 1;
}

/*
 * call rel32: 5 bytes
 * Jumps by 32-bit number relatively from next address.
 * 1 byte: op (E8)
 * 4 bytes: relative number to jump.
 */
static void call_rel32(Emulator *emu)
{
    /* Offset value should be after the op code of 1 byte. */
    int32_t offset = get_sign_code32(emu, 1);
    /* Pushes the address after this call instruction. */
    push32(emu, emu->eip + 5);
    /* Adds the offset to EIP. */
    emu->eip += (offset + 5);
}

/* 
 * ret: 1 byte
 * Jumps to the address pushed by call (address after call instruction).
 * 1 byte: op (C3)
 */
static void ret(Emulator *emu)
{
    emu->eip = pop32(emu);
}

/*
 * leave: 1byte
 * Set of mov esp & pop ebp.
 * 1 byte: op (C9)
 */
static void leave(Emulator *emu)
{
    uint32_t ebp_val = get_register32(emu, EBP);
    /* Update ESP with EBP value. */
    set_register32(emu, ESP, ebp_val);
    /* Pop from stack and set it on EBP. */
    set_register32(emu, EBP, pop32(emu));
    emu->eip += 1;
}

/*
 * Macro for jump with eflag.
 * Replaces the matching text and ## concatenates strings.
 * Defined until unescaped line break.
 */
#define DEFINE_JX(flag, is_flag)                              \
    static void j##flag(Emulator *emu)                        \
    {                                                         \
        int diff = is_flag(emu) ? get_sign_code8(emu, 1) : 0; \
        emu->eip += (diff + 2);                               \
    }                                                         \
    static void jn##flag(Emulator *emu)                       \
    {                                                         \
        int diff = is_flag(emu) ? 0 : get_sign_code8(emu, 1); \
        emu->eip += (diff + 2);                               \
    }

/*
 * jc (72), jnc (73), jz (74), jnz (75), js (78), jns (79), jo (70), jno (71): 2 bytes
 * 1 byte: op code
 * 1 byte: offset to jump
 */
DEFINE_JX(c, is_carry)
DEFINE_JX(z, is_zero)
DEFINE_JX(s, is_sign)
DEFINE_JX(o, is_overflow)

#undef DEFINE_JX

/*
 * jl (7C) and jle (7E)
 * Assuming there's no overflow, which means overflow_flag == 0,
 * if sign_flag is 0, left is bigger. Ex:
 * 3, 2: 3 - 2 = 1 => sign_flag: 0 => larger
 * -3, 2: -3 - (2) = -5 => sign_flag: 1 => smaller
 * -1, -4: -1 - (-4) = 3 => sign_flag: 0 => larger
 */
static void jl(Emulator *emu)
{
    int diff = (is_sign(emu) != is_overflow(emu)) ? get_sign_code8(emu, 1) : 0;
    emu->eip += (diff + 2);
}

static void jle(Emulator *emu)
{
    int diff = (is_zero(emu) || (is_sign(emu) != is_overflow(emu))) ? get_sign_code8(emu, 1) : 0;
    emu->eip += (diff + 2);
}

instruction_func_t *instructions[256];

void init_instructions(void)
{
    int i;
    memset(instructions, 0, sizeof(instructions));

    instructions[0x3B] = cmp_r32_rm32;

    /* Last 3 digits indicates 8 different registers in op code. */
    for (i = 0; i < 8; i++)
    {
        instructions[0x50 + i] = push_r32;
    }

    for (i = 0; i < 8; i++)
    {
        instructions[0x58 + i] = pop_r32;
    }

    instructions[0x68] = push_imm32;
    instructions[0x6A] = push_imm8;

    instructions[0x70] = jo;
    instructions[0x71] = jno;
    instructions[0x72] = jc;
    instructions[0x73] = jnc;
    instructions[0x74] = jz;
    instructions[0x75] = jnz;
    instructions[0x78] = js;
    instructions[0x79] = jns;
    instructions[0x7C] = jl;
    instructions[0x7E] = jle;

    /* Why 0xB8 ~ 0xBF: op code includes 8 registers in 1 byte. */
    for (i = 0; i < 8; i++)
    {
        instructions[0xB8 + i] = mov_r32_imm32;
    }
    instructions[0x01] = add_rm32_r32;
    instructions[0x83] = code_83;
    instructions[0x89] = mov_rm32_r32;
    instructions[0x8B] = mov_r32_rm32;
    instructions[0xC7] = mov_rm32_imm32;
    instructions[0xE9] = near_jump;
    instructions[0xEB] = short_jump;
    instructions[0xFF] = code_ff;

    instructions[0xC3] = ret;
    instructions[0xE8] = call_rel32;
    instructions[0xC9] = leave;
}