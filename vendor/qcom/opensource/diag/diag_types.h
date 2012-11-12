/*--------------------------------------------------------------------------
Copyright (c) 2010, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Code Aurora nor
      the names of its contributors may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--------------------------------------------------------------------------*/

#ifndef DIAG_TYPES_H
#define DIAG_TYPES_H

/*===========================================================================

                   S T A N D A R D    D E C L A R A T I O N S

DESCRIPTION
  This header file contains general types and macros that are of use
  to all modules.  The values or definitions are dependent on the specified
  target.

DEFINED TYPES

       Name      Definition
       -------   --------------------------------------------------------
       byte      8  bit unsigned value
       word      16 bit unsigned value
       dword     32 bit unsigned value

       uint1     byte
       uint2     word
       uint4     dword

       uint8     8  bit unsigned value
       uint16    16 bit unsigned value
       uint32    32 bit unsigned value
       uint64    64 bit unsigned value

       boolean   8 bit boolean value

DEFINED CONSTANTS

       Name      Definition
       -------   --------------------------------------------------------
       TRUE      Asserted boolean condition (Logical 1)
       FALSE     Deasserted boolean condition (Logical 0)

       ON        Asserted condition
       OFF       Deasserted condition

       NULL      Pointer to nothing

       PACKED    Used to indicate structures which should use packed
                 alignment
===========================================================================*/

#ifdef TRUE
#undef TRUE
#endif

#ifdef FALSE
#undef FALSE
#endif

#define TRUE   1   /* Boolean true value. */
#define FALSE  0   /* Boolean false value. */

#ifndef NULL
  #define NULL  0
#endif

/* -----------------------------------------------------------------------
** Standard Types
** ----------------------------------------------------------------------- */

typedef  unsigned char      boolean;     /* Boolean value type. */

typedef  unsigned long int  uint32;      /* Unsigned 32 bit value */
typedef  unsigned short     uint16;      /* Unsigned 16 bit value */
typedef  unsigned char      uint8;       /* Unsigned 8  bit value */

typedef  signed long int    int32;       /* Signed 32 bit value */
typedef  signed short       int16;       /* Signed 16 bit value */
typedef  signed char        int8;        /* Signed 8  bit value */

/* This group are the deprecated types.  Their use should be
** discontinued and new code should use the types above
*/
typedef  unsigned char     byte;         /* Unsigned 8  bit value type. */
typedef  unsigned short    word;         /* Unsinged 16 bit value type. */
typedef  unsigned long     dword;        /* Unsigned 32 bit value type. */

/* ---------------------------------------------------------------------
** Compiler Keyword Macros
** --------------------------------------------------------------------- */
/************************** The PACK() macro *****************************

  This block sets up the semantics for PACK() macro based upon

  upon the target compiler.
  PACK() is necessary to ensure portability of C variable/struct/union

  packing across many platforms.  For example, ARM compilers require the

  following:

    typedef __packed struct { ... } foo_t;

  But GCC requires this to achieve the same effect:

    typedef struct __attribute((__packed__)) struct { ... } foo_t;

  And, of course, Microsoft VC++ requires an alignment #pragma prologue

  and epilogue.

  To satisfy all three, the following form is recommended:

    #ifdef _WIN32

    #pragma pack(push,1) // Save previous, and turn on 1 byte alignment

    #endif

    typedef PACK(struct)

    {

      ...

    } such_and_such_t;

    typedef PACK(struct)

    {

      ...

    } this_and_that_t;

    typedef PACK(struct)

    {

      PACK(struct)

      {

        ...

      } hdr;

      PACK(union)

      {

        such_and_such_t sas;

        this_and_that_t tat;

      } payload;

    } cmd_t;

    #ifdef _WIN32

    #pragma pack(pop) // Revert alignment to what it was previously

    #endif

*********************** BEGIN PACK() Definition ***************************/

#if defined __GNUC__

  #define PACK(x)       x __attribute__((__packed__))

#elif defined __arm

  #define PACK(x)       __packed x

#elif defined _WIN32

  #define PACK(x)       x /* Microsoft uses #pragma pack() prologue/epilogue */

#else

  #error No PACK() macro defined for this compiler

#endif

/********************** END PACK() Definition *****************************/

#if defined(__ARMCC_VERSION)
  #define PACKED __packed
#else  /* __GNUC__ */
  #define PACKED
#endif /* defined (__GNUC__) */

/* ----------------------------------------------------------------------
**                          STANDARD MACROS
** ---------------------------------------------------------------------- */

/*===========================================================================

MACRO FPOS

DESCRIPTION
  This macro computes the offset, in bytes, of a specified field
  of a specified structure or union type.

PARAMETERS
  type          type of the structure or union
  field         field in the structure or union to get the offset of

DEPENDENCIES
  None

RETURN VALUE
  The byte offset of the 'field' in the structure or union of type 'type'.

===========================================================================*/

#define FPOS( type, field ) \
    ( (dword) &(( type *) 0)-> field )

/*===========================================================================

MACRO FSIZ

DESCRIPTION
  This macro computes the size, in bytes, of a specified field
  of a specified structure or union type.

PARAMETERS
  type          type of the structure or union
  field         field in the structure or union to get the size of

DEPENDENCIES
  None

RETURN VALUE
  size in bytes of the 'field' in a structure or union of type 'type'

SIDE EFFECTS
  None

===========================================================================*/

#define FSIZ( type, field ) sizeof( ((type *) 0)->field )

/*===========================================================================

MACRO MAX
MACRO MIN

DESCRIPTION
  Evaluate the maximum/minimum of 2 specified arguments.

PARAMETERS
  x     parameter to compare to 'y'
  y     parameter to compare to 'x'

DEPENDENCIES
  'x' and 'y' are referenced multiple times, and should remain the same
  value each time they are evaluated.

RETURN VALUE
  MAX   greater of 'x' and 'y'
  MIN   lesser of 'x' and 'y'

SIDE EFFECTS
  None

===========================================================================*/
#ifndef MIN
   #define  MIN( x, y ) ( ((x) < (y)) ? (x) : (y) )
#endif

#endif  /* DIAG_TYPES_H */
