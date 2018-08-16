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
 * This file is a supplement to simpleTests.cc that is primarily used for
 * testing parallel builds.
 */

#include <string>

#ifdef CPP17NANOLOG
#include "NanoLogCpp17.h"
#else
#include "NanoLog.h"
#endif

#include "Cycles.h"

#include "folder/Sample.h"

class SimpleTest {
    int number;

public:
    SimpleTest(int number)
        : number(number)
    {}

    void logSomething();
    void wholeBunchOfLogStatements();
    inline void logStatementsInHeader() {
        /// These should be assigned different ids due to line number
        NANO_LOG(NOTICE, "In the header, I am %d", number);
        NANO_LOG(NOTICE, "In the header, I am %d x2", number);
    }
};