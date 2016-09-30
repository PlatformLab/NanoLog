
///**
// * Prefetch the cache lines containing [object, object + numBytes) into the
// * processor's caches.
// * The best docs for this are in the Intel instruction set reference under
// * PREFETCH.
// * \param object
// *      The start of the region of memory to prefetch.
// * \param numBytes
// *      The size of the region of memory to prefetch.
// */
//static inline void
//prefetch(const void* object, uint64_t numBytes)
//{
//    uint64_t offset = reinterpret_cast<uint64_t>(object) & 0x3fUL;
//    const char* p = reinterpret_cast<const char*>(object) - offset;
//    for (uint64_t i = 0; i < offset + numBytes; i += 64)
//        _mm_prefetch(p + i, _MM_HINT_T0);
//}
//
///**
// * Prefetch the cache lines containing the given object into the
// * processor's caches.
// * The best docs for this are in the Intel instruction set reference under
// * PREFETCHh.
// * \param object
// *      A pointer to the object in memory to prefetch.
// */
//template<typename T>
//static inline void
//prefetch(const T* object)
//{
//    prefetch(object, sizeof(*object));
//}

// Unfortunately, unit tests based on gtest can't access private members
// of classes.  If the following uppercase versions of "private" and
// "protected" are used instead, it works around the problem:  when
// compiling unit test files (anything that includes TestUtil.h)
// everything becomes public.

#ifdef EXPOSE_PRIVATES
#define PRIVATE public
#define PROTECTED public
#define PUBLIC public
#else
#define PRIVATE private
#define PROTECTED protected
#define PUBLIC public
#endif