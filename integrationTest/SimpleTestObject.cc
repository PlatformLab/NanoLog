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

#include "FastLogger.h"
#include "Cycles.h"

#include "SimpleTestObject.h"

void
SimpleTest::logSomething()
{
    static int cnt = 0;
    FAST_LOG("SimpleTest::logSomething: Something = %d", ++cnt);
}

void
SimpleTest::wholeBunchOfLogStatements() {
    FAST_LOG("SimpleTest::wholeBunchOfLogStatements: Here I am");

    for (int i = 0; i < 10; ++i) {
        FAST_LOG("SimpleTest::wholeBunchOfLogStatements: I am in a loop!");
    }

    FAST_LOG("SimpleTest::wholeBunchOfLogStatements: exiting...");
}