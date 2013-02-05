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

#include <assert.h>
#include "interpreter.h"
#include <setjmp.h>
#include <stdio.h>
#include "test.h"

static jmp_buf      test_jmpbuf;

static const char * test_input;
static size_t       test_input_size;
static size_t       test_input_offset;

static const char * test_output;
static size_t       test_output_size;
static size_t       test_output_offset;

typedef enum test_error {
    test_okay               = 0,
    test_incorrect_write    = 1,
    test_insufficient_write = 2,
    test_excess_write       = 3,
    test_invalid_test       = 4
} test_error;

static int test_getchar(void) {
    if (test_input_offset >= test_input_size) {
        /* Reading past the end of the input. */
        return EOF;
    }

    int ret = test_input[test_input_offset];
    test_input_offset++;
    return ret;
}

static int test_putchar(int ch) {
    assert(test_output_offset <= test_output_size);
    if (test_output_offset >= test_output_size) {
        /* Writing past end of expected output */
        longjmp(test_jmpbuf, test_excess_write);

        /* We do not expect to return */
        return EOF;
    }

    if (ch != test_output[test_output_offset]) {
        printf("diff '%d' '%d' %zu\n", ch, test_output[test_output_offset], test_output_offset);
        /* Invalid character */
        longjmp(test_jmpbuf, test_incorrect_write);

        /* We do not expect to return */
        return EOF;
    }

    test_output_offset++;
    return ch;
}

int test_interpreter(const char * program, size_t program_size,
        size_t max_data_size, int return_code, const char * input,
        size_t input_size, const char * output, size_t output_size) {
    /* Store parameters in global buffer as we do not have closures to
     * help us. */
    if (input && input_size > 0) {
        test_input          = input;
        test_input_size     = input_size - 1u;
    } else {
        test_input          = NULL;
        test_input_size     = 0u;
    }

    test_input_offset   = 0u;

    if (output && output_size > 0) {
        test_output         = output;
        test_output_size    = output_size - 1u;
    } else {
        test_output         = NULL;
        test_output_size    = 0u;
    }
    test_output_offset  = 0u;

    /* Store state. */
    int jmpret = setjmp(test_jmpbuf);
    if (jmpret != 0) {
        return jmpret;
    }

    int ret = interpret(program, program_size, max_data_size, NULL,
        test_getchar, test_putchar);
    if (ret != return_code) {
        return -ret;
    }

    /* Check output */
    if (test_output_offset != test_output_size) {
        return test_insufficient_write;
    }

    return test_okay;
}


