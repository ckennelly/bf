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

#ifndef __BF__INTERPRETER_H__
#define __BF__INTERPRETER_H__

#include <string.h>

typedef int (*getchar_t)(void);
typedef int (*putchar_t)(int);

typedef enum interpret_error {
    interpret_ok                = 0,
    interpret_guard_error       = 1,
    interpret_handler           = 2,
    interpret_malloc_error      = 3,
    interpret_mmap_error        = 4,
    interpret_munmap_error      = 5,
    interpret_no_memory         = 6,
    interpret_page_size         = 7,
    interpret_tape_exceeded     = 8,
    interpret_tape_underflow    = 9,
    interpret_time_exceeded     = 10,
    interpret_unbalanced        = 11
} interpret_error_t;

/* Forward declaration. */
struct timeval;

int interpret(const char * program, size_t program_size, size_t max_data_size,
    const struct timeval * timelimit, getchar_t gcfp, putchar_t pcfp);
const char * get_interpret_error_string(int return_code);

#endif // __BF__INTERPRETER_H__
