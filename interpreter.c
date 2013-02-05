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

/**
 * To get MAP_ANONYMOUS for mmap.
 */
#define _GNU_SOURCE

#include "assembler.h"
#include <assert.h>
#include "common.h"
#include "interpreter.h"
#include <setjmp.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include <valgrind/memcheck.h>

size_t allocated;
jmp_buf env;
size_t page_size;
char * tape;

size_t pages_forward;
size_t pages_reverse;

typedef enum op {
    op_modify                 = '+',
    op_right                  = '>',
    op_left                   = '<',
    op_get                    = ',',
    op_put                    = '.',
    op_if                     = '[',
    op_endif                  = ']',
    op_invalid                = '\0'
} op_t;

static void handler(int sig, siginfo_t * info, void * context) {
    (void) sig;
    (void) context;

    assert(info);
    char * fault        = info->si_addr;
    char * user_start   = tape + pages_reverse * page_size;
    char * user_end     = tape + allocated - pages_forward * page_size;
    if (fault < tape) {
        /* Return, letting the fault happen. */
        raise(SIGSEGV);
        return;
    }

    char * real_end = tape + allocated;
    if (fault >= real_end) {
        /* Return, letting the fault happen. */
        raise(SIGSEGV);
        return;
    }

           if (fault >= tape     && fault < user_start) {
        /* Hit the left guard: Underflow */
        longjmp(env, interpret_tape_underflow);
    } else if (fault >= user_end && fault < real_end) {
        /* Hit the right guard:  Overflow */
        longjmp(env, interpret_tape_exceeded);
    } else {
        /* We ran out of memory. */
        longjmp(env, interpret_no_memory);
    }
}

static void timer_handler(int sig, siginfo_t * info, void * context) {
    (void) sig;
    (void) info;
    (void) context;

    /* We stop whenever we get an alarm. */
    longjmp(env, interpret_time_exceeded);
}

typedef struct link {
    size_t   offset;
    struct link * previous;
} link_t;

typedef struct stack_item {
    size_t branch_count;
    size_t instruction;
    label_t top;
    label_t end;
} stack_item_t;

typedef struct instruction {
    op_t      op;
    ptrdiff_t val;
    size_t    branch;
} instruction_t;

const char * get_interpret_error_string(int return_code) {
    interpret_error_t err = return_code;

    static const char msg_ok[]         = "Okay.";
    static const char msg_guard[]      = "Error configuring guard pages.";
    static const char msg_handler[]    = "Error configuring SIGSEGV handler.";
    static const char msg_malloc[]     = "Error allocating memory.";
    static const char msg_mmap[]       = "Error during mmap.";
    static const char msg_munmap[]     = "Error during munmap.";
    static const char msg_nomem[]      = "Out of memory.";
    static const char msg_pagesize[]   = "Error retrieving page size.";
    static const char msg_tape[]       = "Tape limit exceeded.";
    static const char msg_tape_under[] = "Tape underflow.";
    static const char msg_time_limit[] = "Time limit exceeded.";
    static const char msg_unbalanced[] = "Unbalanced number of '[' and ']'.";
    static const char msg_unknown[]    = "Unknown error.";

    switch (err) {
        case interpret_ok:
            return msg_ok;
        case interpret_guard_error:
            return msg_guard;
        case interpret_handler:
            return msg_handler;
        case interpret_malloc_error:
            return msg_malloc;
        case interpret_mmap_error:
            return msg_mmap;
        case interpret_munmap_error:
            return msg_munmap;
        case interpret_no_memory:
            return msg_nomem;
        case interpret_page_size:
            return msg_pagesize;
        case interpret_tape_exceeded:
            return msg_tape;
        case interpret_tape_underflow:
            return msg_tape_under;
        case interpret_time_exceeded:
            return msg_time_limit;
        case interpret_unbalanced:
            return msg_unbalanced;
        default:
            return msg_unknown;
    }
}

int interpret(const char * program, size_t program_size, size_t max_data_size,
        const struct timeval * timelimit, getchar_t gcfp, putchar_t pcfp) {
    /* Get page size */
    {
        long page_size_ = sysconf(_SC_PAGESIZE);
        if (page_size_ <= 0) {
            return interpret_page_size;
        }

        page_size = (size_t) page_size_;
    }

    /**
     * Perform a simple scan over the program contents to see how many jumps we
     * need to have a stack for.
     *
     * The branch_count variable tracks how many actual branches ('[') we have
     * and consequently, the number of indices we need to have on hand to track
     * where to jump to.
     */
    size_t instruction;
    size_t branch_count     = 0;
    size_t stack_size       = 0;
    size_t max_stack_size   = 0;
    for (instruction = 0; instruction < program_size; instruction++) {
        char inst = program[instruction];
               if (inst == '[') {
            branch_count++;
            stack_size++;
            if (stack_size > max_stack_size) {
                max_stack_size = stack_size;
            }
        } else if (inst == ']') {
            if (stack_size == 0) {
                /*
                 * The program is malformed.  For the initial prefix of
                 * instructions on the range [0, instruction], it has more ']'
                 * than '['.
                 */
                return interpret_unbalanced;
            }

            stack_size--;
        }
    }

    if (stack_size != 0) {
        /* Unbalanced number of '[' and ']'. */
        return interpret_unbalanced;
    }

    /**
     * Scan for number of distinct instructions (of differing op types)
     */
    op_t   last_op = op_invalid;
    size_t op_count = 0u;
    for (instruction = 0; instruction < program_size; instruction++) {
        op_t next_op = op_invalid;
        switch (program[instruction]) {
            case '+':
            case '-':
                next_op = op_modify;
                break;
            case '>':
                next_op = op_right;
                break;
            case '<':
                next_op = op_left;
                break;
            case ',':
                next_op = op_get;
                break;
            case '.':
                next_op = op_put;
                break;
            case '[':
                next_op = op_if;
                break;
            case ']':
                next_op = op_endif;
                break;
            default:
                break;
        }

        switch (next_op) {
            case op_invalid:
                /* Do nothing */
                break;
            case op_modify:
            case op_left:
            case op_right:
                if (last_op != next_op || last_op == op_invalid) {
                    last_op = next_op;
                    op_count++;
                }

                break;
            case op_get:
            case op_put:
            case op_if:
            case op_endif:
                /* New instruction required */
                last_op = next_op;
                op_count++;
                break;
            default:
                assert(0);
                break;
        }
    }

    /**
     * Allocate instruction space and condense instructions.
     *
     * Assess maximum distance traversed in either direction without
     * interacting with the tape.
     */
    instruction_t * const instructions =
        malloc(sizeof(instruction_t) * op_count);
    if (!(instructions)) {
        return interpret_malloc_error;
    }

    last_op = op_invalid;
    size_t op = 0u;
    for (instruction = 0; instruction < program_size; instruction++) {
        switch (program[instruction]) {
            case '+':
                if (last_op == op_invalid) {
                    instructions[op].op  = last_op = op_modify;
                    instructions[op].val = 0;
                } else if (last_op != op_modify) {
                    op++;
                    assert(op < op_count);

                    instructions[op].op  = last_op = op_modify;
                    instructions[op].val = 0;
                }

                instructions[op].val++;
                break;
            case '-':
                if (last_op == op_invalid) {
                    instructions[op].op  = last_op = op_modify;
                    instructions[op].val = 0;
                } else if (last_op != op_modify) {
                    op++;
                    assert(op < op_count);

                    instructions[op].op  = last_op = op_modify;
                    instructions[op].val = 0;
                }


                instructions[op].val--;
                break;
            case '>':
                if (last_op == op_invalid) {
                    instructions[op].op  = last_op = op_right;
                    instructions[op].val = 0;
                } else if (last_op != op_right) {
                    op++;
                    assert(op < op_count);

                    instructions[op].op  = last_op = op_right;
                    instructions[op].val = 0;
                }

                instructions[op].val++;
                break;
            case '<':
                if (last_op == op_invalid) {
                    instructions[op].op  = last_op = op_left;
                    instructions[op].val = 0;
                } else if (last_op != op_left) {
                    op++;
                    assert(op < op_count);

                    instructions[op].op  = last_op = op_left;
                    instructions[op].val = 0;
                }

                instructions[op].val++;
                break;
            case ',':
                if (last_op != op_invalid) {
                    op++;
                    assert(op < op_count);
                }

                instructions[op].op  = last_op = op_get;
                break;
            case '.':
                if (last_op != op_invalid) {
                    op++;
                    assert(op < op_count);
                }

                instructions[op].op  = last_op = op_put;
                break;
            case '[':
                if (last_op != op_invalid) {
                    op++;
                    assert(op < op_count);
                }

                instructions[op].op  = last_op = op_if;
                break;
            case ']':
                if (last_op != op_invalid) {
                    op++;
                    assert(op < op_count);
                }

                instructions[op].op  = last_op = op_endif;
                break;
            default:
                break;
        }
    }

    assert(op_count == 0 || op == op_count - 1u);

    ptrdiff_t traverse_forward = 0, traverse_reverse = 0;
    for (op = 0; op < op_count; op++) {
        switch (instructions[op].op) {
            case op_left:
                if (traverse_reverse < instructions[op].val) {
                    traverse_reverse = instructions[op].val;
                }
                break;
            case op_right:
                if (traverse_forward < instructions[op].val) {
                    traverse_forward = instructions[op].val;
                }
                break;
            default:
                break;
        }
    }

    /* We're going to overflow something */
    if (traverse_forward >= (ptrdiff_t) (SIZE_MAX / 2 - page_size)) {
        free(instructions);

        return interpret_guard_error;
    }

    if (traverse_reverse >= (ptrdiff_t) (SIZE_MAX / 2 - page_size)) {
        free(instructions);

        return interpret_guard_error;
    }

    /* These casts are safe */
    pages_forward  = (((size_t) traverse_forward) + page_size - 1) / page_size;
    pages_reverse  = (((size_t) traverse_reverse) + page_size - 1) / page_size;

    /* Allocate:
     *
     * Round up to the nearest page size, if for some reason, we have >32kB
     * sized pages, and add two additional guard pages.
     */
    size_t rnd  = (max_data_size + page_size - 1) & ~(page_size - 1);
    allocated   = rnd + (pages_forward + pages_reverse) * page_size;
    tape        = mmap( NULL, allocated, PROT_READ | PROT_WRITE, MAP_PRIVATE |
                        MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (!(tape)) {
        free(instructions);

        return interpret_mmap_error;
    }

    /* Configure guard pages */
    {
        int ret;
        ret = mprotect(tape, pages_reverse * page_size, PROT_NONE);
        if (ret != 0) {
            free(instructions);

            return interpret_guard_error;
        }

        ret = mprotect(tape + pages_reverse * page_size + rnd, pages_forward * page_size, PROT_NONE);
        if (ret != 0) {
            free(instructions);

            return interpret_guard_error;
        }
    }

    /**
     * Per the man page for longjmp:
     *
     *  The values of automatic variables are unspecified after a call to
     *  longjmp() if they meet all the  following criteria:
     *
     *  o they are local to the function that made the corresponding setjmp(3)
     *    call;
     *  o their values are changed between the calls to setjmp(3) and
     *    longjmp(); and
     *  o they are not declared as volatile.
     *
     *  Since we need to clean up the stack, we need access to the contents of
     *  these variables.  They do not change between the calls to setjmp and
     *  longjmp.
     */
    stack_item_t * stack =
        malloc(sizeof(stack_item_t) * max_stack_size);
    if (!(stack)) {
        free(instructions);

        return interpret_malloc_error;
    }

    stack_item_t * branches =
        malloc(sizeof(stack_item_t) * branch_count);
    if (!(branches)) {
        free(instructions);

        return interpret_malloc_error;
    }

    /**
     * Create labels for each loop
     */
    for (op = 0; op < branch_count; op++) {
        branches[op].top = new_label();
        branches[op].end = new_label();
    }

    /**
     * Scan the program once more, this time noting the position of all
     * jumps, using the stack as a buffer.
     */
           branch_count = 0;
    size_t stack_offset = 0;
    for (op = 0; op < op_count; op++) {
        op_t inst = instructions[op].op;
               if (inst == op_if) {
            stack[stack_offset].branch_count = branch_count;
            stack_offset++;
            assert(stack_offset <= max_stack_size);

            branch_count++;
        } else if (inst == op_endif) {
            /* Program has already been checked. */
            assert(stack_offset > 0);

            /* Pop off stack */
            stack_offset--;
            instructions[op].branch = stack[stack_offset].branch_count;
        }
    }

    /**
     * The stack is no longer needed.
     */
    free(stack);

    /**
     * Create an assembler buffer.
     */
    assembler_buffer_t buffer = new_assembler_buffer();

    /**
     * Assemble.
     */
    char * const tape_start = tape + pages_reverse * page_size;

    /* Write preamble
     *
     * pushl %ebp
     * movl %esp, %ebp
     * andl -16, %esp
     * pushl %ebx
     * pushl %edi
     * subl (16 - 2 * sizeof(uintptr_t)), %esp
     */
    emit_push_r(buffer, EBP);
    emit_mov_r_r(buffer, EBP, ESP);
    emit_and_r_immz32(buffer, ESP, ~((uint32_t) 15));

    /* Save EBX/EDI */
    emit_push_r(buffer, EBX);
    emit_push_r(buffer, EDI);
    /* Align */
    const uint32_t stack_adjust = 16u - 2u * sizeof(uintptr_t);
    if (stack_adjust > 0) {
        emit_sub_r_immz32(buffer, ESP, stack_adjust);
    }

    /* Pointer register */
    asm_register_t ptrreg = EBX;
    emit_mov_r_immptr(buffer, ptrreg, (uintptr_t) tape_start);
    branch_count = 0u;
    for (op = 0; op < op_count; op++) {
        switch (instructions[op].op) {
            case op_right:
                if (instructions[op].val == 0) {
                    break;
                }

                /* addl imm, %ptrreg */
                emit_add_r_immz32(buffer, ptrreg, *(uint32_t *) &instructions[op].val);
                break;
            case op_left:
                if (instructions[op].val == 0) {
                    break;
                }

                {
                /* cmpl imm, %ptrreg
                 * jle minlabel
                 * subl imm, %ptrreg
                 * jmp finlabel
                 * minlabel:
                 * movl imm, %ptrreg
                 * finlabel:
                 */
                label_t minlabel = new_label();
                label_t finlabel = new_label();

                uintptr_t min_value = (uintptr_t) (tape_start + instructions[op].val);
                if (sizeof(uintptr_t) == sizeof(uint32_t)) {
                    emit_cmp_r_immz32(buffer, ptrreg, (uint32_t) min_value);
                } else {
                    /* x86_64 does not let us directly compare a register with a 64-bit
                     * immediate, so we stash the value in RDI and compare from there. */
                    emit_mov_r_immptr(buffer, EDI, min_value);
                    emit_cmp_r_r(buffer, ptrreg, EDI);
                }

                emit_jle(buffer, minlabel);
                emit_sub_r_immz32(buffer, ptrreg, *(uint32_t *) &instructions[op].val);
                emit_jmp(buffer, finlabel);
                emit_push_label(buffer, minlabel);
                emit_mov_r_immptr(buffer, ptrreg, (uintptr_t) tape_start);
                emit_push_label(buffer, finlabel);
                }
                break;
            case op_modify:
                if ((instructions[op].val & 0xFF) == 0) {
                    break;
                }

                /* add r/m8 imm8 */
                emit_add_rm8_imm8(buffer, ptrreg, (uint8_t) (instructions[op].val & 0xFF));
                break;
            case op_put:
                /*
                 * xorl %eax, %eax
                 * movl (%ptrreg), %al
                 */
                emit_xor_r_r(buffer, EAX, EAX);
                emit_mov_r8_rm8(buffer, EAX, ptrreg);

                #if   defined(HOST_ARCH_X64)
                /*
                 * movl %rax, %rdi
                 */
                emit_mov_r_r(buffer, EDI, EAX);
                #elif defined(HOST_ARCH_IA32)
                /*
                 * movl %al, (%esp)
                 */
                emit_mov_rm_rint(buffer, ESP, EAX);
                #else
                #error Unsupported architecture.
                #endif

                /* call *pcfp */
                emit_call(buffer, (uintptr_t) pcfp);
                break;
            case op_get:
                {
                /*
                 * call *gcfp
                 * cmpl eax, EOF
                 * jne eoflabel
                 * xorl eax, eax
                 * eoflabel: movl r/m8 r8
                 */
                label_t eof_label = new_label();
                assert(eof_label);

                union {
                    int32_t i;
                    uint32_t u;
                } u;

                u.i = EOF;

                emit_call(buffer, (uintptr_t) gcfp);
                emit_cmp_r_immz32(buffer, EAX, u.u);
                emit_jne(buffer, eof_label);
                emit_xor_r_r(buffer, EAX, EAX);
                emit_push_label(buffer, eof_label);
                emit_mov_rm8_r8(buffer, ptrreg, EAX);
                }
                break;
            case op_if:
                /*
                 * cmp r/m8 0
                 * je end
                 * top:
                 */
                emit_cmp_rm8_imm8(buffer, ptrreg, 0);

                assert(branches[branch_count].end);
                emit_je(buffer, branches[branch_count].end);
                emit_push_label(buffer, branches[branch_count].top);

                branch_count++;
                break;
            case op_endif:
                /*
                 * cmp r/m8 0
                 * jne top
                 * end:
                 */
                emit_cmp_rm8_imm8(buffer, ptrreg, 0);
                emit_jne(buffer, branches[instructions[op].branch].top);
                emit_push_label(buffer, branches[instructions[op].branch].end);
                break;
            default:
                assert(0);
                break;
        }
    }

    /* Coda, restore EBX and leave.
     *
     * addl (16 - 2 * sizeof(uintptr_t)), %esp
     * popl %edi
     * popl %ebx
     * xorl %eax, %eax
     * leave
     * ret
     */
    if (stack_adjust > 0) {
        emit_add_r_immz32(buffer, ESP, stack_adjust);
    }
    emit_pop_r(buffer, EDI);
    emit_pop_r(buffer, EBX);
    emit_xor_r_r(buffer, EAX, EAX);
    emit_leave(buffer);
    emit_ret(buffer);

    /* Cleanup instructions and branches lists */
    free(instructions);
    free(branches);

    /* Finalize assembly */
    typedef void (*vv_t)(void);
    vv_t entry_point = (vv_t) finalize_assembler_buffer(buffer);

    /* Storage for the old SIGSEGV handler. */
    struct sigaction old_sigsegv, old_vtalarm;

    /* Configure a restoration environment */
    int ret = setjmp(env);

    /**
     * Set handler and interpret.
     */
    if (ret == 0) {
        struct sigaction act_sigsegv, act_vtalarm;
        memset(&act_sigsegv, 0, sizeof(act_sigsegv));
        act_sigsegv.sa_sigaction    = handler;
        act_sigsegv.sa_flags        = SA_SIGINFO;

        memset(&act_vtalarm, 0, sizeof(act_vtalarm));
        act_vtalarm.sa_sigaction    = timer_handler;
        act_vtalarm.sa_flags        = SA_SIGINFO;

        int sig_ret = sigaction(SIGSEGV, &act_sigsegv, &old_sigsegv);
        if (sig_ret != 0) {
            free(instructions);
            delete_assembler_buffer(buffer);

            return interpret_handler;
        }

        if (timelimit) {
            sig_ret = sigaction(SIGVTALRM, &act_vtalarm, &old_vtalarm);
            if (sig_ret != 0) {
                free(instructions);
                delete_assembler_buffer(buffer);

                return interpret_handler;
            }

            struct itimerval timer;
            memset(&timer, 0, sizeof(timer));
            timer.it_value = *timelimit;

            int timer_ret = setitimer(ITIMER_VIRTUAL, &timer, NULL);
            if (timer_ret != 0) {
                free(instructions);
                delete_assembler_buffer(buffer);

                return interpret_handler;
            }
        }

        /* Dive in */
        entry_point();
    }

    /* Restore handlers */
    sigaction(SIGSEGV, &old_sigsegv, NULL);
    if (timelimit) {
        sigaction(SIGVTALRM, &old_vtalarm, NULL);
    }

    /* This should cleanup the labels. */
    delete_assembler_buffer(buffer);

    /* Cleanup tape */
    if (munmap(tape, allocated) != 0) {
        /* Unable to unmap pages */
        ret = interpret_munmap_error;
    }

    return ret;
}
