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

#include "interpreter.h"
#include <stdio.h>
#include "test.h"

int main(int argc, char **argv) {
    (void) argc;
    (void) argv;

    {
        /* "Hello, World!" */
        const char program[]    = "++++++++++[>+++++++>++++++++++>+++>+<<<<-]>++.>+.+++++++..+++.>++.<<+++++++++++++++.>.+++.------.--------.>+.>.";
        const char input[]      = "";
        const char output[]     = "Hello World!\n";

        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 1;
        }
    }

    {
        /* [Tape overflow] */
        const char program[]    = "+[>+]";

        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_tape_exceeded, NULL, 0, NULL, 0);
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 2;
        }
    }

    {
        /* "\0" */
        const char program[] = ".";
        const char output[]  = {0x0, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 3;
        }
    }

    {
        /* "\0" */
        const char program[] = ",.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x0, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 4;
        }
    }

    {
        /* "\1" */
        const char program[] = ",+.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x1, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 5;
        }
    }

    {
        /* "\4" */
        const char program[] = ",++++.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x4, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 6;
        }
    }

    {
        /* "\2" */
        const char program[] = "++>+><<.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x2, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 7;
        }
    }

    {
        /* "\2\1" */
        const char program[] = "++>+><<.>.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x2, 0x1, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 8;
        }
    }

    {
        /* "\2\1" */
        const char program[] = "++>+><<<.>.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x2, 0x1, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 8;
        }
    }

    {
        /* "\6\0" */
        const char program[] = "+++[>++<-].>.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x0, 0x6, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 9;
        }
    }

    {
        /* "\40 */
        const char program[] = "+++++[>++++++++<-]>.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {40, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 10;
        }
    }

    {
        /* "\1\7 */
        const char program[] = "+.++++++.";
        const char input[]   = {0x0, 0x0};
        const char output[]  = {0x1, 0x7, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, input, sizeof(input), output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 11;
        }
    }

    {
        const char program[] = "+-";
        const char output[]  = {0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 11;
        }
    }

    {
        const char program[] = "[";
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_unbalanced, NULL, 0, NULL, 0);
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 12;
        }
    }

    {
        const char program[] = "[]]";
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_unbalanced, NULL, 0, NULL, 0);
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 13;
        }
    }

    {
        const char program[] = "a";
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, NULL, 0);
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 14;
        }
    }

    {
        const char program[] = "-+.";
        const char output[]  = {0x0, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 15;
        }
    }

    {
        const char program[] = "<.";
        const char output[]  = {0x0, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 16;
        }
    }

    {
        const char program[] = ">.";
        const char output[]  = {0x0, 0x0};
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, output, sizeof(output));
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 17;
        }
    }

    {
        const char program[] = ",,";
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, NULL, 0);
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 18;
        }
    }

    {
        const char program[] = "><";
        int ret = test_interpreter(program, sizeof(program), (1u << 19),
            interpret_ok, NULL, 0, NULL, 0);
        if (ret != 0) {
            fprintf(stderr, "test_interpreter failed with %d\n", ret);
            return 18;
        }
    }

    return 0;
}
