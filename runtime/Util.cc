/* Copyright (c) 2012 Stanford University
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

#include <sstream>

#include "Util.h"
#include <stdio.h>
#include <stdarg.h>
#include <string>

#include "Portability.h"

using std::string;

namespace NanoLogInternal {
namespace Util {
string format(NANOLOG_PRINTF_FORMAT const char* format, ...)
    NANOLOG_PRINTF_FORMAT_ATTR(1, 2);

string vformat(NANOLOG_PRINTF_FORMAT const char* format, va_list ap)
    NANOLOG_PRINTF_FORMAT_ATTR(1, 0);

/// A safe version of sprintf.
std::string
format(const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    string s = vformat(format, ap);
    va_end(ap);
    return s;
}

/// A safe version of vprintf.
string
vformat(const char* format, va_list ap)
{
    string s;

    // We're not really sure how big of a buffer will be necessary.
    // Try 1K, if not the return value will tell us how much is necessary.
    int bufSize = 1024;
    while (true) {
        char buf[bufSize];
        // vsnprintf trashes the va_list, so copy it first
        va_list aq;
        __va_copy(aq, ap);
        int r = vsnprintf(buf, bufSize, format, aq);
        assert(r >= 0); // old glibc versions returned -1
        if (r < bufSize) {
            s = buf;
            break;
        }
        bufSize = r + 1;
    }

    return s;
}

/**
 * Return (potentially multi-line) string hex dump of a binary buffer in
 * 'hexdump -C' style.
 * Note that this exceeds 80 characters due to 64-bit offsets.
 */
std::string
hexDump(const void *buf, uint64_t bytes)
{
    const unsigned char *cbuf = reinterpret_cast<const unsigned char *>(buf);
    uint64_t i, j;

    std::ostringstream output;
    for (i = 0; i < bytes; i += 16) {
        char offset[17];
        char hex[16][3];
        char ascii[17];

        snprintf(offset, sizeof(offset), "%016lx", i);
        offset[sizeof(offset) - 1] = '\0';

        for (j = 0; j < 16; j++) {
            if ((i + j) >= bytes) {
                snprintf(hex[j], sizeof(hex[0]), "  ");
                ascii[j] = '\0';
            } else {
                snprintf(hex[j], sizeof(hex[0]), "%02x",
                    cbuf[i + j]);
                hex[j][sizeof(hex[0]) - 1] = '\0';
                if (isprint(static_cast<int>(cbuf[i + j])))
                    ascii[j] = cbuf[i + j];
                else
                    ascii[j] = '.';
            }
        }
        ascii[sizeof(ascii) - 1] = '\0';

        output <<
            format("%s  %s %s %s %s %s %s %s %s  %s %s %s %s %s %s %s %s  "
                   "|%s|\n", offset, hex[0], hex[1], hex[2], hex[3], hex[4],
                   hex[5], hex[6], hex[7], hex[8], hex[9], hex[10], hex[11],
                   hex[12], hex[13], hex[14], hex[15], ascii);
    }
    return output.str();
}


} // namespace Util
} // namespace NanoLogInternal
