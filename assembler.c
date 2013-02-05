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

#define _GNU_SOURCE

#include <assert.h>
#include "common.h"
#include "constants.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef struct source {
    int32_t * ptr;
    struct source * next;
} source_t;

typedef struct label {
    unsigned resolved;
    void * ptr;

    struct source * sources;
    struct label * next;
} label_t;

typedef struct assembler_buffer {
    unsigned finalized;
    void * buffer;
    size_t buffer_size;
    size_t offset;
    struct label * labels;
} assembler_buffer_t;

/* Prototypes */
assembler_buffer_t * new_assembler_buffer(void);
void * finalize_assembler_buffer(assembler_buffer_t * buf);
void delete_assembler_buffer(assembler_buffer_t * buf);
label_t * new_label(void);
void delete_label(label_t * lab);
void emit_add_rm8_imm8( assembler_buffer_t * buf, asm_register_t reg, uint8_t imm);
void emit_add_r_immz32( assembler_buffer_t * buf, asm_register_t reg, uint32_t imm);
void emit_and_r_immz32( assembler_buffer_t * buf, asm_register_t reg, uint32_t imm);
void emit_call(         assembler_buffer_t * buf, uintptr_t imm);
void emit_cmp_rm8_imm8( assembler_buffer_t * buf, asm_register_t reg, uint8_t imm);
void emit_cmp_r_immz32( assembler_buffer_t * buf, asm_register_t reg, uint32_t imm);
void emit_cmp_r_r(      assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg);
void emit_je(           assembler_buffer_t * buf, label_t * lab);
void emit_jle(          assembler_buffer_t * buf, label_t * lab);
void emit_jmp(          assembler_buffer_t * buf, label_t * lab);
void emit_jne(          assembler_buffer_t * buf, label_t * lab);
void emit_leave(        assembler_buffer_t * buf);
void emit_mov_r8_rm8(   assembler_buffer_t * buf, asm_register_t reg, asm_register_t sreg);
void emit_mov_rm8_r8(   assembler_buffer_t * buf, asm_register_t reg, asm_register_t sreg);
void emit_mov_r_r(      assembler_buffer_t * buf, asm_register_t reg, asm_register_t sreg);
void emit_mov_r_immptr( assembler_buffer_t * buf, asm_register_t reg, uintptr_t imm);
void emit_mov_rm_rint(  assembler_buffer_t * buf, asm_register_t reg, asm_register_t sreg);
void emit_pop_r(        assembler_buffer_t * buf, asm_register_t reg);
void emit_push_r(       assembler_buffer_t * buf, asm_register_t reg);
void emit_push_label(   assembler_buffer_t * buf, struct label * lab);
void emit_ret(          assembler_buffer_t * buf);
void emit_sub_r_immz32( assembler_buffer_t * buf, asm_register_t reg, uint32_t imm);
void emit_xor_r_r(      assembler_buffer_t * buf, asm_register_t reg, asm_register_t sreg);

/* Internal functions */

static int check_space( const struct assembler_buffer * buf, size_t o) {
    /* TODO Check for overflow */
    assert(buf);
    return (buf->offset + o < buf->buffer_size);
}

static void emit_u8(    struct assembler_buffer * buf, uint8_t v) {
    /* assert(check_space(buf, sizeof(v))); */

    *((uint8_t *) buf->buffer + buf->offset) = v;
    buf->offset += sizeof(v);
}

static void emit_u32(   struct assembler_buffer * buf, uint32_t v) {
    /* assert(check_space(buf, sizeof(v))); */

    *((uint32_t *) ((uint8_t *) buf->buffer + buf->offset)) = v;
    buf->offset += sizeof(v);
}

static void emit_ptr(   struct assembler_buffer * buf, uintptr_t v) {
    /* assert(check_space(buf, sizeof(v))); */

    *((uintptr_t *) ((uint8_t *) buf->buffer + buf->offset)) = v;
    buf->offset += sizeof(v);
}

static void emit_source(struct assembler_buffer * buf, struct label * lab) {
    assert(buf);
    assert(lab);
    assert(check_space(buf, sizeof(int32_t)));

    if (lab->resolved) {
        uint8_t * base = (uint8_t *) buf->buffer + buf->offset;
        uint8_t * next = base + sizeof(int32_t);
        int32_t offset = (int32_t) ((uint8_t *)lab->ptr - next);
        *(int32_t *) base = offset; 
    } else {
        /* Link source in */
        struct source * src = malloc(sizeof(struct source));
        assert(src); /* Handle better TODO */
        src->next = lab->sources;
        lab->sources = src;

        /* Note location so we can resolve this later. */ 
        src->ptr  = (int32_t *) ((uint8_t *) buf->buffer + buf->offset);

        /* Write a temporary value */
        *(src->ptr) = 0;
    }

    buf->offset += sizeof(int32_t);
}

/* Function defintions */

assembler_buffer_t * new_assembler_buffer(void) {
    assembler_buffer_t * ret = malloc(sizeof(assembler_buffer_t)); 
    if (ret) {
        ret->labels = NULL;
        ret->finalized = 0u;

        ret->buffer_size = 1u << 20;
        ret->buffer = mmap(NULL, ret->buffer_size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ret->buffer == MAP_FAILED) {
            /* Unable to map memory */
            free(ret);
            return NULL;
        }

        ret->offset = 0u;
    }

    return ret;
}

void * finalize_assembler_buffer(assembler_buffer_t * buf) {
    assert(buf);

    if (buf->finalized == 0u) {
        buf->finalized = 1u;

        int ret = mprotect(buf->buffer, buf->buffer_size,
            PROT_READ | PROT_EXEC);
        if (ret == -1) {
            return NULL;
        }
    }

    return buf->buffer;
}

void delete_assembler_buffer(assembler_buffer_t * buf) {
    if (!(buf)) {
        return;
    }

    struct label * lab = buf->labels;
    while (lab) {
        assert(lab->sources == NULL);

        struct label * next = lab->next;
        delete_label(lab);
        lab = next;
    }

    munmap(buf->buffer, buf->buffer_size);
    free(buf);
}

label_t * new_label(void) {
    label_t * ret = malloc(sizeof(label_t));
    if (ret) {
        ret->resolved = 0u;
        ret->sources = NULL;
    }

    return ret;
}

void delete_label(label_t * lab) {
    assert(lab);
    assert(lab->resolved || lab->sources == NULL);

    free(lab);
}

void emit_add_rm8_imm8( assembler_buffer_t * buf, asm_register_t reg, uint8_t imm) {
    assert(reg < 8);
    assert(check_space(buf, 3));

    /* 0x80 /0 ib */
    emit_u8(buf, 0x80);
    emit_u8(buf, (uint8_t) reg);
    emit_u8(buf, imm);
}

void emit_add_r_immz32(assembler_buffer_t * buf, asm_register_t reg, uint32_t imm) {
    assert(reg < 8);

    if (reg == EAX) {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x05 id */
        assert(check_space(buf, 2 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x05 id */
        assert(check_space(buf, 1 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x05);
        emit_u32(buf, imm);
    } else {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x81 /0 id */
        assert(check_space(buf, 3 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x81 /0 id */
        assert(check_space(buf, 2 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x81);
        emit_u8(buf, (uint8_t) (0xC0 | reg));
        emit_u32(buf, imm);
    }
}

void emit_and_r_immz32(assembler_buffer_t * buf, asm_register_t reg, uint32_t imm) {
    assert(reg < 8);

    if (reg == EAX) {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x25 id */
        assert(check_space(buf, 2 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x25 id */
        assert(check_space(buf, 1 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x25);
        emit_u32(buf, imm);
    } else {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x81 /4 id */
        assert(check_space(buf, 3 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x81 /4 id */
        assert(check_space(buf, 2 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x81);
        emit_u8(buf, (uint8_t) (0xE0 | reg));
        emit_u32(buf, imm);
    }
}

void emit_call(         assembler_buffer_t * buf, uintptr_t imm) {
    emit_mov_r_immptr(buf, EAX, imm);

    assert(check_space(buf, 2));

    emit_u8(buf, 0xFF);
    emit_u8(buf, 0xD0);
}

void emit_cmp_rm8_imm8( assembler_buffer_t * buf, asm_register_t reg, uint8_t imm) {
    assert(reg < 8);

    /* 0x80 /7 ib */
    assert(check_space(buf, 3));
    emit_u8(buf, 0x80);
    emit_u8(buf, (uint8_t) (0x38 | reg));
    emit_u8(buf, imm);
}

void emit_cmp_r_immz32(assembler_buffer_t * buf, asm_register_t reg, uint32_t imm) {
    assert(reg < 8);

    if (reg == EAX) {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x3D id */
        assert(check_space(buf, 2 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x3D id */
        assert(check_space(buf, 1 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x3D);
        emit_u32(buf, imm);
    } else {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x81 /7 id */
        assert(check_space(buf, 3 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x81 /7 id */
        assert(check_space(buf, 2 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x81);
        emit_u8(buf, (uint8_t) (0xF8 | reg));
        emit_u32(buf, imm);
    }
}

void emit_cmp_r_r(assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg) {
    assert(reg < 8);
    assert(srcreg < 8);

    #if   defined(HOST_ARCH_X64)
    /* REX.W 0x39 /r */
    assert(check_space(buf, 3));
    emit_u8(buf, 0x48);
    #elif defined(HOST_ARCH_IA32)
    /* 0x39 /r */
    assert(check_space(buf, 2));
    #endif

    emit_u8(buf, 0x39);
    emit_u8(buf, (uint8_t) (0xC0 | (srcreg << 3) | reg));
}

typedef enum cc_enum {
    EQ,
    LE,
    NEQ
} cc_t;

static void emit_jcc(   assembler_buffer_t * buf, label_t * lab, cc_t cc) {
    assert(buf);
    assert(lab);
    assert(check_space(buf, 2 + sizeof(int32_t)));

    switch (cc) {
        case EQ:
            /* OF 84 cd */
            emit_u8(buf, 0x0F);
            emit_u8(buf, 0x84);
            break;
        case LE:
            /* 0F 8E cd */
            emit_u8(buf, 0x0F);
            emit_u8(buf, 0x8E);
            break;
        case NEQ:
            /* 0F 85 cd */ 
            emit_u8(buf, 0x0F);
            emit_u8(buf, 0x85);
            break;
        default:
            assert(0);
            return;
    }

    emit_source(buf, lab);
}

void emit_je(           assembler_buffer_t * buf, label_t * lab) {
    emit_jcc(buf, lab, EQ);
}

void emit_jle(          assembler_buffer_t * buf, label_t * lab) {
    emit_jcc(buf, lab, LE);
}

void emit_jmp(          assembler_buffer_t * buf, label_t * lab) {
    assert(check_space(buf, 1 + sizeof(int32_t)));

    /* E9 cd */
    emit_u8(buf, 0xE9);
    emit_source(buf, lab);
}

void emit_jne(          assembler_buffer_t * buf, label_t * lab) {
    emit_jcc(buf, lab, NEQ);
}

void emit_leave(        assembler_buffer_t * buf) {
    /* 0xC9 */
    assert(check_space(buf, 1));
    emit_u8(buf, 0xC9);
}

void emit_mov_r8_rm8(   assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg) {
    assert(reg < 8);
    assert(srcreg < 8);

    /* 0x8A /r */
    assert(check_space(buf, 2));
    emit_u8(buf, 0x8A);
    emit_u8(buf, (uint8_t) ((reg << 3) | srcreg));
}

void emit_mov_rm8_r8(   assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg) {
    assert(reg < 8);
    assert(srcreg < 8);

    /* 0x88 /r */
    assert(check_space(buf, 2));

    emit_u8(buf, 0x88); 
    emit_u8(buf, (uint8_t) ((srcreg << 3) | reg));
}

void emit_mov_r_r(  assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg) {
    assert(reg < 8);
    assert(srcreg < 8);

    #if   defined(HOST_ARCH_X64)
    /* REX.W 0x8B /r */
    assert(check_space(buf, 3));
    emit_u8(buf, 0x48);
    #elif defined(HOST_ARCH_IA32)
    /* 0x8B /r */
    assert(check_space(buf, 2));
    #endif
    emit_u8(buf, 0x8B);
    emit_u8(buf, (uint8_t) (0xC0 | (reg << 3) | srcreg));
}

void emit_mov_r_immptr(assembler_buffer_t * buf, asm_register_t reg, uintptr_t imm) {
    assert(reg < 8);

    #if   defined(HOST_ARCH_X64)
    /* REX.W B8+rd */
    assert(check_space(buf, 2 + sizeof(imm)));
    emit_u8(buf, 0x48);
    #elif defined(HOST_ARCH_IA32)
    /* B8+rd */
    assert(check_space(buf, 1 + sizeof(imm)));
    #endif

    emit_u8(buf, (uint8_t) (0xB8 + reg));
    emit_ptr(buf, imm);
}

void emit_mov_rm_rint( assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg) {
    assert(reg < 8);
    assert(srcreg < 8);

    if (reg == ESP) {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x89 /r */
        assert(check_space(buf, 4));
        // emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x89 /r */
        assert(check_space(buf, 3));
        #endif

        emit_u8(buf, 0x89);
        emit_u8(buf, (uint8_t) ((srcreg << 3) | reg));
        emit_u8(buf, 0x24);
    } else {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x89 /r */
        assert(check_space(buf, 3));
        // emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x89 /r */
        assert(check_space(buf, 2));
        #endif

        emit_u8(buf, 0x89);
        emit_u8(buf, (uint8_t) ((srcreg << 3) | reg));
    }
}

void emit_pop_r(        assembler_buffer_t * buf, asm_register_t reg) {
    assert(reg < 8);

    /* 58+rd */
    assert(check_space(buf, 1));
    emit_u8(buf, (uint8_t) (0x58 + reg));
}

void emit_push_r(       assembler_buffer_t * buf, asm_register_t reg) {
    assert(reg < 8);

    /* 50+rd */
    assert(check_space(buf, 1));
    emit_u8(buf, (uint8_t) (0x50 + reg));
}

void emit_push_label(   assembler_buffer_t * buf, struct label * lab) {
    assert(buf);
    assert(lab);
    assert(!(lab->resolved));

    /* Resolve */
    lab->ptr        = ((char *) buf->buffer) + buf->offset;
    lab->resolved   = 1u;

    /* Resolve the unresolved */
    while (lab->sources) {
        /* Pop */
        struct source * src = lab->sources;
        lab->sources = src->next;

        uint8_t * next_inst = (uint8_t *) src->ptr + sizeof(int32_t);
        uint8_t * dest_inst = (uint8_t *) lab->ptr;
        int32_t offset = (int32_t) (dest_inst - next_inst);
        *(src->ptr) = offset; 
        free(src);
    }

    /* Link into the assembler_buffer_t to take ownership */
    lab->next = buf->labels;
    buf->labels = lab;
}

void emit_ret(          assembler_buffer_t * buf) {
    assert(check_space(buf, 1));

    /* 0xC3 */
    emit_u8(buf, 0xC3);
}

void emit_sub_r_immz32(assembler_buffer_t * buf, asm_register_t reg, uint32_t imm) {
    assert(reg < 8);

    if (reg == EAX) {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x2D id */
        assert(check_space(buf, 2 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x2D id */
        assert(check_space(buf, 1 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x2D);
        emit_u32(buf, imm);
    } else {
        #if   defined(HOST_ARCH_X64)
        /* REX.W 0x81 /5 id */
        assert(check_space(buf, 3 + sizeof(imm)));
        emit_u8(buf, 0x48);
        #elif defined(HOST_ARCH_IA32)
        /* 0x81 /5 id */
        assert(check_space(buf, 2 + sizeof(imm)));
        #endif

        emit_u8(buf, 0x81);
        emit_u8(buf, (uint8_t) (0xE8 | reg));
        emit_u32(buf, imm);
    }
}

void emit_xor_r_r(  assembler_buffer_t * buf, asm_register_t reg, asm_register_t srcreg) {
    assert(reg < 8);
    assert(srcreg < 8);

    #if   defined(HOST_ARCH_X64)
    assert(check_space(buf, 3));
    /* REX.W 0x31 /r */
    emit_u8(buf, 0x48);
    #elif defined(HOST_ARCH_IA32)
    assert(check_space(buf, 2));
    /* 0x31 /r */
    #endif

    emit_u8(buf, 0x31);
    emit_u8(buf, (uint8_t) (0xC0 | (reg << 3) | srcreg));
}
