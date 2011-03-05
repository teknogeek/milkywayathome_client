/* Copyright 2010 Matthew Arsenault, Travis Desell, Boleslaw
Szymanski, Heidi Newberg, Carlos Varela, Malik Magdon-Ismail and
Rensselaer Polytechnic Institute.

This file is part of Milkway@Home.

Milkyway@Home is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Milkyway@Home is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Milkyway@Home.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _MILKYWAY_EXTRA_H_
#define _MILKYWAY_EXTRA_H_

/* FIXME: OpenCL bool size */
typedef short int mwbool;

#ifndef TRUE
  #define TRUE  1
  #define FALSE 0
#endif

#ifdef _MSC_VER
  #define strdup _strdup
  #define isnan _isnan
  #define isfinite _finite
  #define copysign _copysign
  #define access _access
  #define snprintf _snprintf
  #define getcwd _getcwd
  #define stat _stat
  #define strncasecmp(a, b, n) _strnicmp(a, b, n)
  #define strcasecmp(a, b) _stricmp(a, b)
#endif /* _MSC_VER */

/* Horrible workaround for lack of C99 in MSVCRT and it being
   impossible to print size_t correctly and standardly */
#ifdef _WIN32
  #define ZU "%Iu"
  #define LLU "%I64u"
#else
  #define ZU "%zu"

  /* FIXME: Should correctly check sizes */
  #ifdef __APPLE__
    #define LLU "%llu"
  #else
    #define LLU "%lu"
  #endif
#endif /* _WIN32 */


  #ifdef __MINGW32__
    #include <stdlib.h>
   /* I don't know why this doesn't work correctly with mingw */
    extern void* _aligned_malloc(size_t size, size_t alignment);
    extern void _aligned_free(void* memblock);
  #endif


#endif /* _MILKYWAY_EXTRA_H_ */

