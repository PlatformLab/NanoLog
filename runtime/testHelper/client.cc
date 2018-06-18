/* Copyright (c) 2017 Stanford University
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

#include "NanoLog.h"

using namespace NanoLog::LogLevels;
int main() {
	NANO_LOG(NOTICE, "Simple log message with 0 parameters");
	NANO_LOG(NOTICE, "This is a string %s", "aaa");

	NANO_LOG(DEBUG, "Debug level");
	NANO_LOG(NOTICE, "Notice Level");
	NANO_LOG(WARNING, "Warning Level");
	NANO_LOG(ERROR, "Error Level");

	NANO_LOG(NOTICE, "I have an integer %d", 2);
	NANO_LOG(NOTICE, "I have a uint64_t %lu", 2);
	NANO_LOG(NOTICE, "I have a double %lf", 2.0);
	NANO_LOG(NOTICE, "I have a couple of things %d, %f, %u, %s",
			 			1, 2.0, 3, "s4");
	return 0;
}
