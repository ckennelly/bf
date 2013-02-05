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

#ifndef __BF__COMMON_H__
#define __BF__COMMON_H__

/**
 * Architecture detection.
 */
#if defined(_M_X64) || defined(__x86_64__)
#define HOST_ARCH_X64 1
#elif defined(_M_IX86) || defined(__i386__)
#define HOST_ARCH_IA32 1
#else
#error Architecture was not supported.
#endif

#endif // __BF__COMMON_H__
