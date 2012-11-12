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

#ifndef DIAG_OS_H
#define DIAG_OS_H

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================

                   Diag Mapping Layer DLL declarations

DESCRIPTION
  Function declarations for Diag Service Mapping Layer
===========================================================================*/
#include <diag_types.h>

#define MSG_LEGACY_HIGH     (0x00000004)
#define MSG_SSID_ANDROID_ADB    58
#define MSG_BUILD_MASK_MSG_SSID_ANDROID_ADB (MSG_LVL_HIGH) /* Created for Android messages drained from logcat */
#define MSG_LVL_HIGH    MSG_LEGACY_HIGH

/*---------------------------------------------------------------------------
  This is the macro for sprintf messages with 1 parameters. msg_sprintf()
  uses var arg list supported by the compiler.This Macro is used when xx_fmt
  is passed as a literal.
---------------------------------------------------------------------------*/
#define MSG_SPRINTF_1(xx_ss_id, xx_ss_mask, xx_fmt, xx_arg1) \
  do { \
    if (xx_ss_mask & (MSG_BUILD_MASK_ ## xx_ss_id)) { \
      XX_MSG_CONST (xx_ss_id, xx_ss_mask, xx_fmt); \
      msg_sprintf (&xx_msg_const,  (uint32)(xx_arg1)); \
    } \
  } while (0) \

/*---------------------------------------------------------------------------
  The purpose of this macro is to define the constant part of the message
  that can be initialized at compile time and stored in ROM. This will
  define and initialize a msg_const_type for each call of a message macro.
  The "static" limits the scope to the file the macro is called from while
  using the macro in a do{}while() guarantees the uniqueness of the name.
---------------------------------------------------------------------------*/
#define XX_MSG_CONST(xx_ss_id, xx_ss_mask, xx_fmt) \
    static const msg_const_type xx_msg_const = { \
      {__LINE__, (xx_ss_id), (xx_ss_mask)}, (xx_fmt), msg_file}

/* If MSG_FILE is defined, use that as the filename, and allocate a single
   character string to contain it.  Note that this string is shared with the
   Error Services, to conserve ROM and RAM.

   With ADS1.1 and later, multiple uses of __FILE__ or __MODULE__ within the
   same file do not cause multiple literal strings to be stored in ROM. So in
   builds that use the more recent versions of ADS, it is not necessary to
   define the static variable msg_file. Note that __MODULE__ gets replaced
   with the filename portion of the full pathname of the file.
---------------------------------------------------------------------------*/
#define msg_file __FILE__

/*---------------------------------------------------------------------------
  This structure is stored in ROM and is copied blindly by the phone.
  The values for the fields of this structure are known at compile time.
  So this is to be defined as a "static const" in the MACRO, so it ends up
  being defined and initialized at compile time for each and every message
  in the software. This minimizes the amount of work to do during run time.

  So this structure is to be used in the "caller's" context. "Caller" is the
  client of the Message Services.
---------------------------------------------------------------------------*/
typedef struct
{
  uint16 line;			/* Line number in source file */
  uint16 ss_id;			/* Subsystem ID               */
  uint32 ss_mask;		/* Subsystem Mask             */
}
msg_desc_type;

/*---------------------------------------------------------------------------
  All constant information stored for a message.

  The values for the fields of this structure are known at compile time.
  So this is to be defined as a "static " in the MACRO, so it ends up
  being defined and initialized at compile time for each and every message
  in the software. This minimizes the amount of work to do during run time.

  So this structure is to be used in the "caller's" context. "Caller" is the
  client of the Message Services.
---------------------------------------------------------------------------*/
typedef struct
{
  msg_desc_type desc;       /* ss_mask, line, ss_id */
  const char *fmt;      /* Printf style format string */
  const char *fname;        /* Pointer to source file name */
}
msg_const_type;
/*===========================================================================

FUNCTION MSG_SPRINTF

DESCRIPTION
  This will build a message sprintf diagnostic Message with var # (1 to 6)
  of parameters.
  Do not call directly; use macro MSG_SPRINTF_* defined in msg.h

  Send a message through diag output services.

DEPENDENCIES
  msg_init() must have been called previously.  A free buffer must
  be available or the message will be ignored (never buffered).

===========================================================================*/
  void msg_sprintf(const msg_const_type* const_blk,...);

/*===========================================================================
FUNCTION   Diag_LSM_Init

DESCRIPTION
  Initializes the Diag Legacy Mapping Layer. This should be called
  only once per process.

DEPENDENCIES
  Successful initialization requires Diag CS component files to be present
  and accessible in the file system.

RETURN VALUE
  FALSE = failure, else TRUE

SIDE EFFECTS
  None

===========================================================================*/
boolean Diag_LSM_Init (byte* pIEnv);

/*===========================================================================

FUNCTION    Diag_LSM_DeInit

DESCRIPTION
  De-Initialize the Diag service.

DEPENDENCIES
  None.

RETURN VALUE
  FALSE = failure, else TRUE
  Currently all the internal boolean return functions called by
  this function just returns TRUE w/o doing anything.

SIDE EFFECTS
  None

===========================================================================*/
boolean Diag_LSM_DeInit(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAG_OS_H */

