/* Copyright (c) 2011-2017 Stanford University
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

#ifndef FENCE_H
#define FENCE_H

namespace NanoLogInternal {
/**
 * This class is used to restrict instruction reordering within a CPU in
 * order to maintain synchronization properties between threads.  Is a thin
 * wrapper around x86 "fence" instructions.  Note: getting fencing correct
 * is extremely tricky!  Whenever possible, use existing solutions that already
 * handle the fencing.
 */
class Fence {
  public:

    /**
     * This method creates a boundary across which load instructions cannot
     * migrate: if a memory read comes from code occurring before (after)
     * invoking this method, the read is guaranteed to complete before (after)
     * the method is invoked.
     */
    static void inline lfence()
    {
        __asm__ __volatile__("lfence" ::: "memory");
    }

    /**
     * This method creates a boundary across which store instructions cannot
     * migrate: if a memory store comes from code occurring before (after)
     * invoking this method, the store is guaranteed to complete before (after)
     * the method is invoked.
     */
    static void inline sfence()
    {
        __asm__ __volatile__("sfence" ::: "memory");
    }

    /**
     * This method provides appropriate fencing for the beginning of a critical
     * section.  Normally this method is invoked immediately after performing
     * synchronization to enter a critical section, such as acquiring a lock.
     * It guarantees the following:
     * - Loads coming from code following this method will see any changes
     *   made to memory by other threads before the method is invoked.
     * - Stores coming from code following this method will be reflected
     *   in memory after the method is invoked.  Note: this property depends
     *   on the existence of branch instructions in the synchronization step
     *   that precedes this method (stores cannot be reflected in memory until
     *   after any preceding branches have been resolved).
     */
    static void inline enter()
    {
        lfence();
    }

    /**
     * This method provides appropriate fencing for the end of a critical
     * section.  Normally this method is invoked immediately prior to releasing
     * the lock for the critical section.  It guarantees the following:
     * - Loads coming from code preceding this method will complete before the
     *   method returns, so they will not see any changes made to memory by other
     *   threads after the method is invoked.
     * - Stores coming from code preceding this method will be reflected
     *   in memory before the method returns, so when the next thread enters
     *   the critical section it is guaranteed to see any changes made in the
     *   current critical section.
     */
    static void inline leave()
    {
        sfence();
        lfence();
    }
};
}; // namespace NanoLogInternal

#endif  // FENCE_H
