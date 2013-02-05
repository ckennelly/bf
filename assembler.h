/**
 * bf - A JIT'ing Interpreter for a Turing Tarpit
 * (c) 2012 - Chris Kennelly (chris@ckennelly.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BF__ASSEMBLER_H__
#define __BF__ASSEMBLER_H__

#include "constants.h"
#include <stdint.h>

typedef void* assembler_buffer_t;
typedef void* label_t;

assembler_buffer_t new_assembler_buffer(void);
void * finalize_assembler_buffer(assembler_buffer_t);
void delete_assembler_buffer(assembler_buffer_t);

label_t new_label(void);
void delete_label(label_t);

void emit_add_rm8_imm8( assembler_buffer_t, asm_register_t reg, uint8_t imm);
void emit_add_r_immz32( assembler_buffer_t, asm_register_t reg, uint32_t imm);
void emit_and_r_immz32( assembler_buffer_t, asm_register_t reg, uint32_t imm);
void emit_call(         assembler_buffer_t, uintptr_t imm);
void emit_cmp_rm8_imm8( assembler_buffer_t, asm_register_t reg, uint8_t imm);
void emit_cmp_r_immz32( assembler_buffer_t, asm_register_t reg, uint32_t imm);
void emit_cmp_r_r(      assembler_buffer_t, asm_register_t reg, asm_register_t srcreg);
void emit_je(           assembler_buffer_t, label_t lab);
void emit_jle(          assembler_buffer_t, label_t lab);
void emit_jmp(          assembler_buffer_t, label_t lab);
void emit_jne(          assembler_buffer_t, label_t lab);
void emit_leave(        assembler_buffer_t);
void emit_mov_r8_rm8(   assembler_buffer_t, asm_register_t reg, asm_register_t srcreg);
void emit_mov_rm8_r8(   assembler_buffer_t, asm_register_t reg, asm_register_t srcreg);
void emit_mov_r_r(      assembler_buffer_t, asm_register_t reg, asm_register_t srcreg);
void emit_mov_r_immptr( assembler_buffer_t, asm_register_t reg, uintptr_t imm);
void emit_mov_rm_rint(  assembler_buffer_t, asm_register_t reg, asm_register_t srcreg);
void emit_pop_r(        assembler_buffer_t, asm_register_t reg);
void emit_push_r(       assembler_buffer_t, asm_register_t reg);
void emit_push_rint(    assembler_buffer_t, asm_register_t reg);
void emit_push_label(   assembler_buffer_t, label_t lab);
void emit_ret(          assembler_buffer_t);
void emit_sub_r_immz32( assembler_buffer_t, asm_register_t reg, uint32_t imm);
void emit_xor_r_r(      assembler_buffer_t, asm_register_t reg, asm_register_t srcreg);

#endif // __BF__ASSEMBLER_H__
