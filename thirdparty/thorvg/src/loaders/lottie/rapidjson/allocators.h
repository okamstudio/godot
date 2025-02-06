// Tencent is pleased to support the open source community by making RapidJSON available.
// 
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed 
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR 
// CONDITIONS OF ANY KIND, either express or implied. See the License for the 
// specific language governing permissions and limitations under the License.

#ifndef RAPIDJSON_ALLOCATORS_H_
#define RAPIDJSON_ALLOCATORS_H_

#include "rapidjson.h"
#include "internal/meta.h"

#include <memory>
#include <limits>

#if RAPIDJSON_HAS_CXX11
#include <type_traits>
#endif

RAPIDJSON_NAMESPACE_BEGIN

///////////////////////////////////////////////////////////////////////////////
// Allocator

/*! \class rapidjson::Allocator
    \brief Concept for allocating, resizing and freeing memory block.
    
    Note that Malloc() and Realloc() are non-static but Free() is static.
    
    So if an allocator need to support Free(), it needs to put its pointer in 
    the header of memory block.

\code
concept Allocator {
    static const bool kNeedFree;    //!< Whether this allocator needs to call Free().

    // Allocate a memory block.
    // \param size of the memory block in bytes.
    // \returns pointer to the memory block.
    void* Malloc(size_t size);

    // Resize a memory block.
    // \param originalPtr The pointer to current memory block. Null pointer is permitted.
    // \param originalSize The current size in bytes. (Design issue: since some allocator may not book-keep this, explicitly pass to it can save memory.)
    // \param newSize the new size in bytes.
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize);

    // Free a memory block.
    // \param pointer to the memory block. Null pointer is permitted.
    static void Free(void *ptr);
};
\endcode
*/


/*! \def RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY
    \ingroup RAPIDJSON_CONFIG
    \brief User-defined kDefaultChunkCapacity definition.

    User can define this as any \c size that is a power of 2.
*/

#ifndef RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY
#define RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY (64 * 1024)
#endif


///////////////////////////////////////////////////////////////////////////////
// CrtAllocator

//! C-runtime library allocator.
/*! This class is just wrapper for standard C library memory routines.
    \note implements Allocator concept
*/
class CrtAllocator {
public:
    static const bool kNeedFree = true;
    void* Malloc(size_t size) { 
        if (size) //  behavior of malloc(0) is implementation defined.
            return RAPIDJSON_MALLOC(size);
        else
            return NULL; // standardize to returning NULL.
    }
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) {
        (void)originalSize;
        if (newSize == 0) {
            RAPIDJSON_FREE(originalPtr);
            return NULL;
        }
        return RAPIDJSON_REALLOC(originalPtr, newSize);
    }
    static void Free(void *ptr) RAPIDJSON_NOEXCEPT { RAPIDJSON_FREE(ptr); }

    bool operator==(const CrtAllocator&) const RAPIDJSON_NOEXCEPT {
        return true;
    }
    bool operator!=(const CrtAllocator&) const RAPIDJSON_NOEXCEPT {
        return false;
    }
};

///////////////////////////////////////////////////////////////////////////////
// MemoryPoolAllocator

//! Default memory allocator used by the parser and DOM.
/*! This allocator allocate memory blocks from pre-allocated memory chunks. 

    It does not free memory blocks. And Realloc() only allocate new memory.

    The memory chunks are allocated by BaseAllocator, which is CrtAllocator by default.

    User may also supply a buffer as the first chunk.

    If the user-buffer is full then additional chunks are allocated by BaseAllocator.

    The user-buffer is not deallocated by this allocator.

    \tparam BaseAllocator the allocator type for allocating memory chunks. Default is CrtAllocator.
    \note implements Allocator concept
*/
template <typename BaseAllocator = CrtAllocator>
class MemoryPoolAllocator {
    //! Chunk header for perpending to each chunk.
    /*! Chunks are stored as a singly linked list.
    */
    struct ChunkHeader {
        size_t capacity;    //!< Capacity of the chunk in bytes (excluding the header itself).
        size_t size;        //!< Current size of allocated memory in bytes.
        ChunkHeader *next;  //!< Next chunk in the linked list.
    };

    struct SharedData {
        ChunkHeader *chunkHead;  //!< Head of the chunk linked-list. Only the head chunk serves allocation.
        BaseAllocator* ownBaseAllocator; //!< base allocator created by this object.
        size_t refcount;
        bool ownBuffer;
    };

    static const size_t SIZEOF_SHARED_DATA = RAPIDJSON_ALIGN(sizeof(SharedData));
    static const size_t SIZEOF_CHUNK_HEADER = RAPIDJSON_ALIGN(sizeof(ChunkHeader));

    static inline ChunkHeader *GetChunkHead(SharedData *shared)
    {
        return reinterpret_cast<ChunkHeader*>(reinterpret_cast<uint8_t*>(shared) + SIZEOF_SHARED_DATA);
    }
    static inline uint8_t *GetChunkBuffer(SharedData *shared)
    {
        return reinterpret_cast<uint8_t*>(shared->chunkHead) + SIZEOF_CHUNK_HEADER;
    }

    static const size_t kDefaultChunkCapacity = RAPIDJSON_ALLOCATOR_DEFAULT_CHUNK_CAPACITY; //!< Default chunk capacity.

public:
    static const bool kNeedFree = false;    //!< Tell users that no need to call Free() with this allocator. (concept Allocator)
    static const bool kRefCounted = true;   //!< Tell users that this allocator is reference counted on copy

    //! Constructor with chunkSize.
    /*! \param chunkSize The size of memory chunk. The default is kDefaultChunkSize.
        \param baseAllocator The allocator for allocating memory chunks.
    */
    explicit
    MemoryPoolAllocator(size_t chunkSize = kDefaultChunkCapacity, BaseAllocator* baseAllocator = 0) : 
        chunk_capacity_(chunkSize),
        baseAllocator_(baseAllocator ? baseAllocator : RAPIDJSON_NEW(BaseAllocator)()),
        shared_(static_cast<SharedData*>(baseAllocator_ ? baseAllocator_->Malloc(SIZEOF_SHARED_DATA + SIZEOF_CHUNK_HEADER) : 0))
    {
        RAPIDJSON_ASSERT(baseAllocator_ != 0);
        RAPIDJSON_ASSERT(shared_ != 0);
        if (baseAllocator) {
            shared_->ownBaseAllocator = 0;
        }
        else {
            shared_->ownBaseAllocator = baseAllocator_;
        }
        shared_->chunkHead = GetChunkHead(shared_);
        shared_->chunkHead->capacity = 0;
        shared_->chunkHead->size = 0;
        shared_->chunkHead->next = 0;
        shared_->ownBuffer = true;
        shared_->refcount = 1;
    }

    //! Constructor with user-supplied buffer.
    /*! The user buffer will be used firstly. When it is full, memory pool allocates new chunk with chunk size.

        The user buffer will not be deallocated when this allocator is destructed.

        \param buffer User supplied buffer.
        \param size Size of the buffer in bytes. It must at least larger than sizeof(ChunkHeader).
        \param chunkSize The size of memory chunk. The default is kDefaultChunkSize.
        \param baseAllocator The allocator for allocating memory chunks.
    */
    MemoryPoolAllocator(void *buffer, size_t size, size_t chunkSize = kDefaultChunkCapacity, BaseAllocator* baseAllocator = 0) :
        chunk_capacity_(chunkSize),
        baseAllocator_(baseAllocator),
        shared_(static_cast<SharedData*>(AlignBuffer(buffer, size)))
    {
        RAPIDJSON_ASSERT(size >= SIZEOF_SHARED_DATA + SIZEOF_CHUNK_HEADER);
        shared_->chunkHead = GetChunkHead(shared_);
        shared_->chunkHead->capacity = size - SIZEOF_SHARED_DATA - SIZEOF_CHUNK_HEADER;
        shared_->chunkHead->size = 0;
        shared_->chunkHead->next = 0;
        shared_->ownBaseAllocator = 0;
        shared_->ownBuffer = false;
        shared_->refcount = 1;
    }

    MemoryPoolAllocator(const MemoryPoolAllocator& rhs) RAPIDJSON_NOEXCEPT :
        chunk_capacity_(rhs.chunk_capacity_),
        baseAllocator_(rhs.baseAllocator_),
        shared_(rhs.shared_)
    {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        ++shared_->refcount;
    }
    MemoryPoolAllocator& operator=(const MemoryPoolAllocator& rhs) RAPIDJSON_NOEXCEPT
    {
        RAPIDJSON_NOEXCEPT_ASSERT(rhs.shared_->refcount > 0);
        ++rhs.shared_->refcount;
        this->~MemoryPoolAllocator();
        baseAllocator_ = rhs.baseAllocator_;
        chunk_capacity_ = rhs.chunk_capacity_;
        shared_ = rhs.shared_;
        return *this;
    }

#if RAPIDJSON_HAS_CXX11_RVALUE_REFS
    MemoryPoolAllocator(MemoryPoolAllocator&& rhs) RAPIDJSON_NOEXCEPT :
        chunk_capacity_(rhs.chunk_capacity_),
        baseAllocator_(rhs.baseAllocator_),
        shared_(rhs.shared_)
    {
        RAPIDJSON_NOEXCEPT_ASSERT(rhs.shared_->refcount > 0);
        rhs.shared_ = 0;
    }
    MemoryPoolAllocator& operator=(MemoryPoolAllocator&& rhs) RAPIDJSON_NOEXCEPT
    {
        RAPIDJSON_NOEXCEPT_ASSERT(rhs.shared_->refcount > 0);
        this->~MemoryPoolAllocator();
        baseAllocator_ = rhs.baseAllocator_;
        chunk_capacity_ = rhs.chunk_capacity_;
        shared_ = rhs.shared_;
        rhs.shared_ = 0;
        return *this;
    }
#endif

    //! Destructor.
    /*! This deallocates all memory chunks, excluding the user-supplied buffer.
    */
    ~MemoryPoolAllocator() RAPIDJSON_NOEXCEPT {
        if (!shared_) {
            // do nothing if moved
            return;
        }
        if (shared_->refcount > 1) {
            --shared_->refcount;
            return;
        }
        Clear();
        BaseAllocator *a = shared_->ownBaseAllocator;
        if (shared_->ownBuffer) {
            baseAllocator_->Free(shared_);
        }
        RAPIDJSON_DELETE(a);
    }

    //! Deallocates all memory chunks, excluding the first/user one.
    void Clear() RAPIDJSON_NOEXCEPT {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        for (;;) {
            ChunkHeader* c = shared_->chunkHead;
            if (!c->next) {
                break;
            }
            shared_->chunkHead = c->next;
            baseAllocator_->Free(c);
        }
        shared_->chunkHead->size = 0;
    }

    //! Computes the total capacity of allocated memory chunks.
    /*! \return total capacity in bytes.
    */
    size_t Capacity() const RAPIDJSON_NOEXCEPT {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        size_t capacity = 0;
        for (ChunkHeader* c = shared_->chunkHead; c != 0; c = c->next)
            capacity += c->capacity;
        return capacity;
    }

    //! Computes the memory blocks allocated.
    /*! \return total used bytes.
    */
    size_t Size() const RAPIDJSON_NOEXCEPT {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        size_t size = 0;
        for (ChunkHeader* c = shared_->chunkHead; c != 0; c = c->next)
            size += c->size;
        return size;
    }

    //! Whether the allocator is shared.
    /*! \return true or false.
    */
    bool Shared() const RAPIDJSON_NOEXCEPT {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        return shared_->refcount > 1;
    }

    //! Allocates a memory block. (concept Allocator)
    void* Malloc(size_t size) {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        if (!size)
            return NULL;

        size = RAPIDJSON_ALIGN(size);
        if (RAPIDJSON_UNLIKELY(shared_->chunkHead->size + size > shared_->chunkHead->capacity))
            if (!AddChunk(chunk_capacity_ > size ? chunk_capacity_ : size))
                return NULL;

        void *buffer = GetChunkBuffer(shared_) + shared_->chunkHead->size;
        shared_->chunkHead->size += size;
        return buffer;
    }

    //! Resizes a memory block (concept Allocator)
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize) {
        if (originalPtr == 0)
            return Malloc(newSize);

        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        if (newSize == 0)
            return NULL;

        originalSize = RAPIDJSON_ALIGN(originalSize);
        newSize = RAPIDJSON_ALIGN(newSize);

        // Do not shrink if new size is smaller than original
        if (originalSize >= newSize)
            return originalPtr;

        // Simply expand it if it is the last allocation and there is sufficient space
        if (originalPtr == GetChunkBuffer(shared_) + shared_->chunkHead->size - originalSize) {
            size_t increment = static_cast<size_t>(newSize - originalSize);
            if (shared_->chunkHead->size + increment <= shared_->chunkHead->capacity) {
                shared_->chunkHead->size += increment;
                return originalPtr;
            }
        }

        // Realloc process: allocate and copy memory, do not free original buffer.
        if (void* newBuffer = Malloc(newSize)) {
            if (originalSize)
                std::memcpy(newBuffer, originalPtr, originalSize);
            return newBuffer;
        }
        else
            return NULL;
    }

    //! Frees a memory block (concept Allocator)
    static void Free(void *ptr) RAPIDJSON_NOEXCEPT { (void)ptr; } // Do nothing

    //! Compare (equality) with another MemoryPoolAllocator
    bool operator==(const MemoryPoolAllocator& rhs) const RAPIDJSON_NOEXCEPT {
        RAPIDJSON_NOEXCEPT_ASSERT(shared_->refcount > 0);
        RAPIDJSON_NOEXCEPT_ASSERT(rhs.shared_->refcount > 0);
        return shared_ == rhs.shared_;
    }
    //! Compare (inequality) with another MemoryPoolAllocator
    bool operator!=(const MemoryPoolAllocator& rhs) const RAPIDJSON_NOEXCEPT {
        return !operator==(rhs);
    }

private:
    //! Creates a new chunk.
    /*! \param capacity Capacity of the chunk in bytes.
        \return true if success.
    */
    bool AddChunk(size_t capacity) {
        if (!baseAllocator_)
            shared_->ownBaseAllocator = baseAllocator_ = RAPIDJSON_NEW(BaseAllocator)();
        if (ChunkHeader* chunk = static_cast<ChunkHeader*>(baseAllocator_->Malloc(SIZEOF_CHUNK_HEADER + capacity))) {
            chunk->capacity = capacity;
            chunk->size = 0;
            chunk->next = shared_->chunkHead;
            shared_->chunkHead = chunk;
            return true;
        }
        else
            return false;
    }

    static inline void* AlignBuffer(void* buf, size_t &size)
    {
        RAPIDJSON_NOEXCEPT_ASSERT(buf != 0);
        const uintptr_t mask = sizeof(void*) - 1;
        const uintptr_t ubuf = reinterpret_cast<uintptr_t>(buf);
        if (RAPIDJSON_UNLIKELY(ubuf & mask)) {
            const uintptr_t abuf = (ubuf + mask) & ~mask;
            RAPIDJSON_ASSERT(size >= abuf - ubuf);
            buf = reinterpret_cast<void*>(abuf);
            size -= abuf - ubuf;
        }
        return buf;
    }

    size_t chunk_capacity_;     //!< The minimum capacity of chunk when they are allocated.
    BaseAllocator* baseAllocator_;  //!< base allocator for allocating memory chunks.
    SharedData *shared_;        //!< The shared data of the allocator
};

namespace internal {
    template<typename, typename = void>
    struct IsRefCounted :
        public FalseType
    { };
    template<typename T>
    struct IsRefCounted<T, typename internal::EnableIfCond<T::kRefCounted>::Type> :
        public TrueType
    { };
}

template<typename T, typename A>
inline T* Realloc(A& a, T* old_p, size_t old_n, size_t new_n)
{
    RAPIDJSON_NOEXCEPT_ASSERT(old_n <= (std::numeric_limits<size_t>::max)() / sizeof(T) && new_n <= (std::numeric_limits<size_t>::max)() / sizeof(T));
    return static_cast<T*>(a.Realloc(old_p, old_n * sizeof(T), new_n * sizeof(T)));
}

template<typename T, typename A>
inline T *Malloc(A& a, size_t n = 1)
{
    return Realloc<T, A>(a, NULL, 0, n);
}

template<typename T, typename A>
inline void Free(A& a, T *p, size_t n = 1)
{
    static_cast<void>(Realloc<T, A>(a, p, n, 0));
}

#ifdef __GNUC__
RAPIDJSON_DIAG_PUSH
RAPIDJSON_DIAG_OFF(effc++) // std::allocator can safely be inherited
#endif

template <typename T, typename BaseAllocator = CrtAllocator>
class StdAllocator :
    public std::allocator<T>
{
    typedef std::allocator<T> allocator_type;
#if RAPIDJSON_HAS_CXX11
    typedef std::allocator_traits<allocator_type> traits_type;
#else
    typedef allocator_type traits_type;
#endif

public:
    typedef BaseAllocator BaseAllocatorType;

    StdAllocator() RAPIDJSON_NOEXCEPT :
        allocator_type(),
        baseAllocator_()
    { }

    StdAllocator(const StdAllocator& rhs) RAPIDJSON_NOEXCEPT :
        allocator_type(rhs),
        baseAllocator_(rhs.baseAllocator_)
    { }

    template<typename U>
    StdAllocator(const StdAllocator<U, BaseAllocator>& rhs) RAPIDJSON_NOEXCEPT :
        allocator_type(rhs),
        baseAllocator_(rhs.baseAllocator_)
    { }

#if RAPIDJSON_HAS_CXX11_RVALUE_REFS
    StdAllocator(StdAllocator&& rhs) RAPIDJSON_NOEXCEPT :
        allocator_type(std::move(rhs)),
        baseAllocator_(std::move(rhs.baseAllocator_))
    { }
#endif
#if RAPIDJSON_HAS_CXX11
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
#endif

    /* implicit */
    StdAllocator(const BaseAllocator& allocator) RAPIDJSON_NOEXCEPT :
        allocator_type(),
        baseAllocator_(allocator)
    { }

    ~StdAllocator() RAPIDJSON_NOEXCEPT
    { }

    template<typename U>
    struct rebind {
        typedef StdAllocator<U, BaseAllocator> other;
    };

    typedef typename traits_type::size_type         size_type;
    typedef typename traits_type::difference_type   difference_type;

    typedef typename traits_type::value_type        value_type;
    typedef typename traits_type::pointer           pointer;
    typedef typename traits_type::const_pointer     const_pointer;

#if RAPIDJSON_HAS_CXX11

    typedef typename std::add_lvalue_reference<value_type>::type &reference;
    typedef typename std::add_lvalue_reference<typename std::add_const<value_type>::type>::type &const_reference;

    pointer address(reference r) const RAPIDJSON_NOEXCEPT
    {
        return std::addressof(r);
    }
    const_pointer address(const_reference r) const RAPIDJSON_NOEXCEPT
    {
        return std::addressof(r);
    }

    size_type max_size() const RAPIDJSON_NOEXCEPT
    {
        return traits_type::max_size(*this);
    }

    template <typename ...Args>
    void construct(pointer p, Args&&... args)
    {
        traits_type::construct(*this, p, std::forward<Args>(args)...);
    }
    void destroy(pointer p)
    {
        traits_type::destroy(*this, p);
    }

#else // !RAPIDJSON_HAS_CXX11

    typedef typename allocator_type::reference       reference;
    typedef typename allocator_type::const_reference const_reference;

    pointer address(reference r) const RAPIDJSON_NOEXCEPT
    {
        return allocator_type::address(r);
    }
    const_pointer address(const_reference r) const RAPIDJSON_NOEXCEPT
    {
        return allocator_type::address(r);
    }

    size_type max_size() const RAPIDJSON_NOEXCEPT
    {
        return allocator_type::max_size();
    }

    void construct(pointer p, const_reference r)
    {
        allocator_type::construct(p, r);
    }
    void destroy(pointer p)
    {
        allocator_type::destroy(p);
    }

#endif // !RAPIDJSON_HAS_CXX11

    template <typename U>
    U* allocate(size_type n = 1, const void* = 0)
    {
        return RAPIDJSON_NAMESPACE::Malloc<U>(baseAllocator_, n);
    }
    template <typename U>
    void deallocate(U* p, size_type n = 1)
    {
        RAPIDJSON_NAMESPACE::Free<U>(baseAllocator_, p, n);
    }

    pointer allocate(size_type n = 1, const void* = 0)
    {
        return allocate<value_type>(n);
    }
    void deallocate(pointer p, size_type n = 1)
    {
        deallocate<value_type>(p, n);
    }

#if RAPIDJSON_HAS_CXX11
    using is_always_equal = std::is_empty<BaseAllocator>;
#endif

    template<typename U>
    bool operator==(const StdAllocator<U, BaseAllocator>& rhs) const RAPIDJSON_NOEXCEPT
    {
        return baseAllocator_ == rhs.baseAllocator_;
    }
    template<typename U>
    bool operator!=(const StdAllocator<U, BaseAllocator>& rhs) const RAPIDJSON_NOEXCEPT
    {
        return !operator==(rhs);
    }

    //! rapidjson Allocator concept
    static const bool kNeedFree = BaseAllocator::kNeedFree;
    static const bool kRefCounted = internal::IsRefCounted<BaseAllocator>::Value;
    void* Malloc(size_t size)
    {
        return baseAllocator_.Malloc(size);
    }
    void* Realloc(void* originalPtr, size_t originalSize, size_t newSize)
    {
        return baseAllocator_.Realloc(originalPtr, originalSize, newSize);
    }
    static void Free(void *ptr) RAPIDJSON_NOEXCEPT
    {
        BaseAllocator::Free(ptr);
    }

private:
    template <typename, typename>
    friend class StdAllocator; // access to StdAllocator<!T>.*

    BaseAllocator baseAllocator_;
};

#if !RAPIDJSON_HAS_CXX17 // std::allocator<void> deprecated in C++17
template <typename BaseAllocator>
class StdAllocator<void, BaseAllocator> :
    public std::allocator<void>
{
    typedef std::allocator<void> allocator_type;

public:
    typedef BaseAllocator BaseAllocatorType;

    StdAllocator() RAPIDJSON_NOEXCEPT :
        allocator_type(),
        baseAllocator_()
    { }

    StdAllocator(const StdAllocator& rhs) RAPIDJSON_NOEXCEPT :
        allocator_type(rhs),
        baseAllocator_(rhs.baseAllocator_)
    { }

    template<typename U>
    StdAllocator(const StdAllocator<U, BaseAllocator>& rhs) RAPIDJSON_NOEXCEPT :
        allocator_type(rhs),
        baseAllocator_(rhs.baseAllocator_)
    { }

    /* implicit */
    StdAllocator(const BaseAllocator& baseAllocator) RAPIDJSON_NOEXCEPT :
        allocator_type(),
        baseAllocator_(baseAllocator)
    { }

    ~StdAllocator() RAPIDJSON_NOEXCEPT
    { }

    template<typename U>
    struct rebind {
        typedef StdAllocator<U, BaseAllocator> other;
    };

    typedef typename allocator_type::value_type value_type;

private:
    template <typename, typename>
    friend class StdAllocator; // access to StdAllocator<!T>.*

    BaseAllocator baseAllocator_;
};
#endif

#ifdef __GNUC__
RAPIDJSON_DIAG_POP
#endif

RAPIDJSON_NAMESPACE_END

#endif // RAPIDJSON_ENCODINGS_H_
