/* Copyright (c) 2016 Stanford University
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

 /**
  * Helps tests the following components of the NanoLog system:
  *   1) Detecting NANO_LOG redefinitions via #define (should handled
  *      by C preprocessor) in a header file
  *   2) Consistent assignment of format identifiers to a log statement
  *      in the header file #include-d by multiple C++ files.
  */

#include <iostream>
#include <string>
#include <stdio.h>
#include <stdint.h>

#ifndef PREPROCESSOR_NANOLOG
#include "NanoLogCpp17.h"
#else
#include "NanoLog.h"
#endif
using namespace NanoLog::LogLevels;

#ifndef __Sample__h__
#define __Sample__h__

// Tests whether the system can detect re #define's
#define LOG NANO_LOG


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

// Tests whether header functions get parsed as well.
static void
hiddenInHeaderFilePrint()
{
    NANO_LOG(NOTICE,
      "Messages in the Header"
        " File"
        );
}

#pragma GCC diagnostic pop

#endif // __Sample__h__