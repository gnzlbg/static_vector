# inline_vector [![Travis build status][travis-shield]][travis] [![Coveralls.io code coverage][coveralls-shield]][coveralls] [![Docs][docs-shield]][docs]


> A dynamically-resizable vector with fixed capacity and inline storage (revision -1)

**Document number**: none.

**Date**: none.

**Author**: Gonzalo Brito Gadeschi.

# Introduction

This paper proposes a dynamically-resizable `vector` with fixed capacity and
contiguous inline storage. That is, the elements of the vector are stored within
the vector object itself. It is based on 
[`boost::container::static_vector<T, Capacity>`][boost_static_vector].

Its API is almost a 1:1 map of `std::vector<T, A>`s API. It is a sequence
random-access container with contiguous storage, O(1) insertion and removal of
elements at the end, and O(N) insertion and removal otherwise. Like
`std::vector`, the elements are initialized on insertion and destroyed on
removal.

The name `inline_vector` is used throughout this proposal to refer to this container. 
While I'd rather avoid bikeshedding at this point (revision -1), if that happens to
be your favourite sport there is a section below evaluating different names. Feedback
is always greatly appreciated.

# Motivation

This container is useful when:

- memory allocation is not possible, e.g., embedded environments without a free store, 
  where only a stack and the static memory segment are available,
- memory allocation imposes an unacceptable performance penalty, e.g., with respect to latency, 
- allocation of objects with complex lifetimes in the _static_-memory segment is required,
- non-default constructible objects must be stored such that `std::array` is not an option,
- full control over the storage location of the vector elements is required.

# Design considerations

## Can we reuse `std::vector` with a custom allocator? 

Yes, we can, but no, it does not result in a zero-cost abstraction. Two main
reasons:

1. The growth mechanism of `std::vector` makes it impossible for a custom allocator
to allocate the optimal amount of storage for a given capacity.

2. The resulting vector would be at least two words too big. The capacity of
`inline_vector` is part of its type and does not need to be stored. The custom
allocator stores the elements internally, storing a `data` pointer inside vector
is unnecessary since a pointer to the first element can be obtained for free. Even 
if the growth mechanism of `std::vector` was known by the allocator implementation,
the allocator would need to assume the worst possible allocation pattern and reserve
up to 2x the required memory to remain safe to use.

## Can we reuse `small_vector`?

Yes, we can, but no, it does not result in a zero-cost abstraction.

The paper
[PR0274: Clump – A Vector-like Contiguous Sequence Container with Embedded Storage][clump]
proposes a new type, `small_vector<T, N, Allocator>`, which is essentially a
`std::vector<T, Allocator>` that performs a Small Vector Optimization for up to
`N` elements, and then, depending on the `Allocator`, might fall-back to heap allocations,
or do something else. 

This small vector type is part of [Boost][boostsmallvector], [LLVM][llvmsmallvector], 
[EASTL][eastl], and [Folly][folly]. Most of these libraries special case `small_vector`
for the case in which _only_ inline storage is desired. This result in a type with
slightly different but noticeable semantics (in the spirit of `vector<bool>`). The
only library that offers it as a completely different type is Boost.Container.

The main difference between `small_vector` and `inline_vector` is:

- `small_vector` uses inline storage up-to `N` elements, and then falls back to
  `Allocator`, while `inline_vector` _only_ provides inline storage.

As a consequence, for the use cases of `inline_vector`:

- `small_vector` cannot provide the same exception safety guarantees than
  `inline_vector` because it sometimes allocates memory, which `inline_vector`
  never does.

- `small_vector` cannot provide the same reliability than `inline_vector`,
  because the algorithmic complexity of `small_vector` operations like move
  construction/assignment depends on whether the elements are allocated on the
  heap (swap pointer and size) or inline (must always copy the elements).

- `small_vector` cannot be as efficient as `inline_vector`. It must discriminate 
  between the different storage locations of its elements (inline or with the 
  allocator). A part of its methods must, at run-time, branch to a different 
  code-path depending on the active storage scheme.

The only way to fix `small_vector` would be to special case it for
`Allocator::max_size() == 0` (which isn't constexpr), and to provide an API that
has different complexity and exception-safety guarantees than `small_vector`s
with `Allocator::max_size() > 0`.

That is, to make `small_vector` competitive in those situations in which 
`inline_vector` is required is to special case `small_vector` for a particular
allocator type and in that case provide an `inline_vector` implementation (with
slightly different semantics). 

## Should we special case `small_vector` for inline-storage-only like EASTL and Folly do?

The types `inline_vector` and `small_vector` have different algorithmic
complexity and exception-safety guarantees. They solve different problems and
should be different types. 

## Existing practice

There are at least 3 widely used implementations of `inline_vector`.

This proposal is strongly inspired by Boost.Container, which offers
[`boost::container::static_vector<T, Capacity>` (1.59)][boost_static_vector],
and, as a different type, also offers `boost::container::small_vector<T, N, Allocator>`
as well.

The other two libraries that implement `inline_vector` are [Folly][folly] and
[EASTL][eastl]. Both of these libraries implement it as a special case of
`small_vector` (which in both of these libraries has 4 template parameters).

EASTL `small_vector` is called `fixed_vector<T, N,
hasAllocator,OverflowAllocator>` and uses a boolean template parameter to
indicate if the only storage mode available is inline storage. The
[design documents of EASTL][eastldesign] seem to predate this special casing
since they actually argue against it:

>- special casing complicates the implementation of `small_vector`,
>- (without proof) the resulting code size increase would be larger than the "4
>  bytes" that can be saved in storage per vector for the special case.

The current implementation does, however, special case `small_vector` for inline
storage. No rationale for this decision is given in the design documentation.

Folly implements `small_vector<T, N, NoHeap, size_type>`, where the tag type
`NoHeap` is used to switch off heap-fall back. Folly allows customizing the 
`size_type` of `small_vector`. No rationale for this design decision is available
in the documentation. A different `size_type` can potentially be used to reduce 
`inline_vector`s memory requirements.

## Current design

The current design follows that of `boost::container::static_vector<T,
Capacity>` closely.

It introduces a new type `std::experimental::inline_vector<T, Capacity>` in the
`<experimental/inline_vector>` header.

> `inline_vector` is a dynamically-resizable contiguous random-access sequence
> container with O(1) insert/erase at the end, and O(N) insert/erase otherwise.
> Its elements are stored within the vector object itself.

A prototype implementation of this proposal is provided for standardization
purposes: [`http://github.com/gnzlbg/inline_vector`][inline_vector].

The main drawback of introducing a new type is, as the
[design document of the EASTL][eastldesign] points out, increased code size.
Since this container type is opt-in, only those users that need it will pay
this cost. Common techniques to reduce code size where explored in the 
prototype implementation (e.g. implementing `Capacity`/`value_type` agnostic 
functionality in a base class) without success, but implementations are 
encouraged to consider code-size as a quality of implementation issue.

## Storage/Memory Layout

The elements of the vector are properly aligned to an `alignof(T)` memory address.

The `inline_vector<T, Capacity>::size_type` is the smallest unsigned integer type
that can represent `Capacity`.
  
**Note**: `inline_vector<T, Capacity>` cannot be an aggregate since it provides
user-defined constructors.

### Zero-sized

It is required that `is_empty<inline_vector<T, 0>>::value == true`, 
in which case  `data() == begin() == end() == unspecified unique value`
(`nullptr` is intended), and `swap` is `noexcept`.

### Constexpr-support

The whole API of `inline_vector<T, Capacity>` is `constexpr` if `is_trivial<T>`
is true.

### Interoperability of inline vectors with different capacities (possible future extension)

A possible backwards-compatible future extension that is not pursued further
in this paper is providing interoperability of `inline_vector`s of different
capacities (e.g. copy construction/assignment/comparison/swap). Currently,
other standard containers like `std::array` do not pursue this, and it would
complicate the exception-safety specification of, e.g., `swap`, significantly. 

### Explicit instantiatiability

The class `inline_vector<T, Capacity>` can be explicitly instantiated for
all combinations of `T` and `Capacity`.

### Exception Safety

The only operations that can actually fail within `inline_vector<T, Capacity>` are:

  1. `T` constructors, assignment, destructor, and swap. These can fail only due
     to throwing constructors/assignment/destructors/swap of `T`. If these
     operations are `noexcept` for `T`, they are `noexcept` for
     `inline_vector`. Since the storage is inline only the basic guarantee can
     be provided by these if `noexcept(false)`.

  2. Out-of-bounds unchecked access (`front/back/pop_back` when empty, unchecked
     random-access). These are undefined behavior (an `assertion` is encouraged
     as a quality of implementation issue).

  3. Out-of-bounds checked access, `at`, which throws `out_of_range` exception.

  4. Insertion exceeding the capacity (`push_back`, `insert`, `emplace`, ..).
     These are undefined behavior (an `assertion` is encouraged as a quality of
     implementation issue). This is still an open question:

This last point is controversial. First, the capacity of the vector is fixed at 
compile-time. Is exceeding the capacity, on, e.g., `push_back`: a `bad_alloc`, 
`out_of_range`, or even a `logic_error`? The author considers that exceeding
the capacity of a fixed-capacity vector is actually a `logic_error`.

Second, for API-similarity with `std::vector`, throwing an exception is desired.
On the other hand, the capacity is fixed, no memory allocations can occur, and
as a consequence `_unchecked` versions of these operations are desired in some
use cases (see below).

Open questions (feedback required):

- What kind of error is exceeding the capacity of a fixed-capacity vector?
- Should we:
  - provide the same API as `std::vector` and just throw `bad_alloc`?
  - provide `_unchecked` versions of these operations?
  - make all of these operations unchecked?

The current revision (-1) of this proposal considers these logic errors, makes all 
of these operations unchecked, violating the preconditions is undefined behavior, 
and recommends an assertion

Note: providing `_unchecked` versions of these operations significantly increases
the API of `inline_vector`, but doing so, and preserving `std::vector` semantics
for the checked operations, might be a good tradeoff:

  - checked (throws `bad_alloc` like `std::vector`): `push_back`,
    `emplace_back`, `emplace`, `insert`, ...
  - unchecked (undefined behavior, assertion encouraged): `push_back_unchecked`,
  `emplace_back_unchecked`, `emplace_unchecked`, `insert_unchecked`,
  `resize_unchecked`

An alternative to `_unchecked`-named functions would be to use tag dispatching 
with, e.g., a `std::unchecked_t` tag type.

### Default initialization (possible future Extension)

The size-modifying operations of the `inline_vector` that do not require a value
also have the following analogous counterparts that perform default
initialization instead of value initialization:

```c++
struct default_initialized_t {};
inline constexpr default_initialized_t default_initialized{};

template <typename Value, std::size_t Capacity>
struct inline_vector {
// ...
    constexpr inline_vector(default_initialized_t, size_type n);
    constexpr void resize(default_initialized_t, size_type sz);
    constexpr void resize_unchecked(default_initialized_t, size_type sz);
};
```

### Iterators

The iterator invalidation rules are different than those for `std::vector`,
since:

- moving an `inline_vector` invalidates all iterators,
- swapping two `inline_vector`s invalidates all iterators, and 
- inserting elements into an `inline_vector` never invalidates iterators.

The following functions can potentially invalidate the iterators of `inline_vector`s: 
`resize(n)`, `resize(n, v)`, `pop_back`, `erase`, and `swap`.

The following functions from the "possible future extensions" can potentially
invalidate the iterators of `inline_vector`s: `resize_default_initialized(n)`,
`resize_unchecked(n)`, `resize_unchecked(n, v)`, and
`resize_default_initialized_unchecked(n)`.

### Naming (feedback required)

Following names have been considered: The name `inline_vector<T, Capacity>` 

Some names that have been considered:
denotes that the elements are stored
"inline" with the object itself.

- `fixed_capacity_vector`: a vector with fixed capacity, long name, but clearly indicates what this is.
- `static_vector` (Boost.Container): due to "static" allocation of the elements. Might be confusing
  because the vector (and the elements) will sometimes be allocated in static memory (but not always). 
- `inline_vector`: denotes that the elements are stored "inline" with the object itself.
  Might be confusing because the term `inline` is overloaded in C++. In C++17 the term `inline` is also
  used to refer to data (in the context of `inline` variables), but there it means something else. 
- `stack_vector`: to denote that the elements can be stored on the stack, which is confusing since the
  elements can be on the stack, the heap, or the static memory segment. It also has a resemblance with `std::stack`.
- `embedded_vector`: since the elements are "embedded" within the vector object itself.

### Summary of possible future extensions

None of these are proposed in this revision (revision -1) of this proposal.

1. Support default initialization of elements.
2. Support comparison/construction/assignment/swap between `inline_vector`s of
   different capacities.
3. Support uncheked mutating operations: `resize_unchecked`,
   `push_back_unchecked`, `assign_unchecked`, `emplace`, `insert`, ...

## Proposed API

This enhancement is a pure addition to the C++ standard.

```c++
template<typename T, std::size_t C /* Capacity */>
struct inline_vector {

// types:
typedef value_type& reference;
typedef value_type const& const_reference;
typedef implementation-defined iterator;
typedef implementation-defined const_iterator;
typedef /*smallest unsigned integer type that is able to represent Capacity */ size_type;
typedef ptrdiff_t difference_type;
typedef T value_type;
typedef T* pointer;
typedef T const* const_pointer;
typedef reverse_iterator<iterator> reverse_iterator;
typedef reverse_iterator<const_iterator> const_reverse_iterator;

// construct/copy/move/destroy:
constexpr inline_vector() noexcept;
constexpr explicit inline_vector(size_type n);
constexpr inline_vector(size_type n, const value_type& value);
template<class InputIterator>
constexpr inline_vector(InputIterator first, InputIterator last);
constexpr inline_vector(inline_vector const& other)
  noexcept(is_nothrow_copy_constructible<value_type>{});
constexpr inline_vector(inline_vector && other)
  noexcept(is_nothrow_move_constructible<value_type>{});
constexpr inline_vector(initializer_list<value_type> il);

/* constexpr ~inline_vector(); */  // implicitly generated

constexpr inline_vector& operator=(inline_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
constexpr inline_vector& operator=(inline_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});

template<class InputIterator>
constexpr void assign(InputIterator first, InputIterator last);
constexpr void assign(size_type n, const value_type& u);
constexpr void assign(initializer_list<value_type> il);


// iterators:
constexpr iterator               begin()         noexcept;
constexpr const_iterator         begin()   const noexcept;
constexpr iterator               end()           noexcept;
constexpr const_iterator         end()     const noexcept;

constexpr reverse_iterator       rbegin()        noexcept;
constexpr const_reverse_iterator rbegin()  const noexcept;
constexpr reverse_iterator       rend()          noexcept;
constexpr const_reverse_iterator rend()    const noexcept;

constexpr const_iterator         cbegin()        noexcept;
constexpr const_iterator         cend()    const noexcept;
constexpr const_reverse_iterator crbegin()       noexcept;
constexpr const_reverse_iterator crend()   const noexcept;


// size/capacity:
constexpr size_type size()     const noexcept;
static constexpr size_type capacity() noexcept;
static constexpr size_type max_size() noexcept;
constexpr void resize(size_type sz);
constexpr void resize(size_type sz, const value_type& c)
constexpr bool empty() const noexcept;

void reserve(size_type n) = delete;
void shrink_to_fit() = delete; 

// element access:
constexpr reference       operator[](size_type n) noexcept; 
constexpr const_reference operator[](size_type n) const noexcept;
constexpr const_reference at(size_type n) const;
constexpr reference       at(size_type n);
constexpr reference       front() noexcept;
constexpr const_reference front() const noexcept;
constexpr reference       back() noexcept;
constexpr const_reference back() const noexcept;

// data access:
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;

// modifiers:
template<class... Args>
  constexpr void emplace_back(Args&&... args);
constexpr void push_back(const value_type& x);
constexpr void push_back(value_type&& x);
constexpr void pop_back();

template<class... Args>
  constexpr iterator emplace(const_iterator position, Args&&...args)
constexpr iterator insert(const_iterator position, const value_type& x);
constexpr iterator insert(const_iterator position, value_type&& x);
constexpr iterator insert(const_iterator position, size_type n, const value_type& x);
template<class InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);

constexpr iterator insert(const_iterator position, initializer_list<value_type> il);

constexpr iterator erase(const_iterator position)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator erase(const_iterator first, const_iterator last)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr void clear() noexcept(is_nothrow_destructible<value_type>{});

constexpr void swap(inline_vector&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));

friend constexpr bool operator==(const inline_vector& a, const inline_vector& b);
friend constexpr bool operator!=(const inline_vector& a, const inline_vector& b);
friend constexpr bool operator<(const inline_vector& a, const inline_vector& b);
friend constexpr bool operator<=(const inline_vector& a, const inline_vector& b);
friend constexpr bool operator>(const inline_vector& a, const inline_vector& b);
friend constexpr bool operator>=(const inline_vector& a, const inline_vector& b);
};
```

### Construction

```c++
/// Constructs an empty inline_vector.
///
/// Requirements: none.
///
/// Enabled: always.
///
/// Complexity:
/// - time: O(1),
/// - space: O(1).
///
/// Exception safety: never throws.
///
/// Constexpr: always.
///
/// Iterator invalidation: none.
///
/// Effects: none.
/// - it is guaranteed that no elements will be constructed unless 
/// `is_trivial<value_type>`, in which case this guarantee is 
/// implementation defined.
///
constexpr inline_vector() noexcept;
```

```c++
/// Constructs a inline_vector containing \p n value-initialized elements.
///
/// Requirements: `value_type` shall be `DefaultInsertable` into `*this`.
///
/// Enabled: if requirements are met.
///
/// Complexity:
/// - time: O(N) calls to `value_type`'s default constructor,
/// - space: O(1).
///
/// Exception safety:
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - rethrows if `value_type`'s default constructor throws.
///
/// Constexpr: if `is_trivial<value_type>`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p n calls to `value_type`s default constructor.
///
constexpr explicit inline_vector(size_type n);
```

```c++
/// Constructs a inline_vector containing \p n copies of \p value.
///
/// Requirements: `value_type` shall be `CopyInsertable` into `*this`.
///
/// Enabled: if requirements are met.
///
/// Complexity:
/// - time: O(N) calls to `value_type`'s copy constructor,
/// - space: O(1).
///
/// Exception safety: 
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - rethrows if `value_type`'s copy constructor throws,
/// - throws `bad_alloc` if `\p n > capacity()`.
///
/// Constexpr: if `is_trivial<value_type>`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p n calls to `value_type`s copy constructor.
///
constexpr inline_vector(size_type n, const value_type& value);
```

```c++ 
/// Constructs a inline_vector equal to the range [\p first, \p last).
///
/// Requirements: `value_type` shall be either:
/// - `CopyInsertable` into `*this` _if_ the reference type of `InputIterator`
///    is an lvalue reference, or
/// - `MoveInsertable` into `*this` _if_ the reference type of `InputIterator`
///    is an rvalue reference.
///
/// Enabled: if requirements are met.
///
/// Complexity:
/// - time: O(N) calls to `value_type`'s copy or move constructor,
/// - space: O(1).
///
/// Exception safety: 
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - rethrows if `value_type`'s copy or move constructors throws,
/// - throws `bad_alloc` if `\p n > capacity()`.
///
/// Constexpr: if `is_trivial<value_type>`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `last - first` calls to `value_type`s copy or move constructor.
///
template<class InputIterator>
constexpr inline_vector(InputIterator first, InputIterator last);
```

```c++
/// Constructs a inline_vector whose elements are copied from \p other.
///
/// Requirements: `value_type` shall be `CopyInsertable` into `*this`.
///
/// Enabled: if requirements are met.
///
/// Complexity:
/// - time: O(N) calls to `value_type`'s copy constructor,
/// - space: O(1).
///
/// Exception safety: 
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - rethrows if `value_type`'s copy constructor throws.
///
/// Constexpr: if `is_trivial<value_type>`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `other.size()` calls to `value_type`s copy constructor.
///
constexpr inline_vector(inline_vector const&);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

```c++
/// Constructs a inline_vector whose elements are moved from \p other.
///
/// Requirements: `value_type` shall be `MoveInsertable` into `*this`.
///
/// Enabled: if requirements are met.
///
/// Complexity:
/// - time: O(N) calls to `value_type`'s move constructor,
/// - space: O(1).
///
/// Exception safety: 
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - rethrows if `value_type`'s move constructor throws.
///
/// Constexpr: if `is_trivial<value_type>`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `other.size()` calls to `value_type`s move constructor.
///
constexpr inline_vector(inline_vector&&)
  noexcept(is_nothrow_move_constructible<value_type>{});
```

```c++
/// Effects: Equivalent to `inline_vector(il.begin(), il.end())`.
constexpr inline_vector(initializer_list<value_type> il);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

### Assignment

Move assignment operations invalidate iterators.

```c++
constexpr inline_vector& operator=(inline_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
```

```c++
constexpr inline_vector& operator=(inline_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
```

```c++
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector& operator=(inline_vector<value_type, M>const& other)
    noexcept(is_nothrow_copy_assignable<value_type>{} and C >= M);
```

```c++
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector& operator=(inline_vector<value_type, M>&& other);
    noexcept(is_nothrow_move_assignable<value_type>{} and C >= M);
```

```c++
template<class InputIterator>
constexpr void assign(InputIterator first, InputIterator last);
```

```c++
constexpr void assign(size_type n, const value_type& u);
```

```c++
constexpr void assign(initializer_list<value_type> il);
```

### Destruction

The destructor should be implicitly generated and it should be constexpr
if `is_trivial<value_type>`.

```c++
constexpr ~inline_vector(); // implicitly generated
```

### Iterators

For all iterator functions:

```c++
constexpr iterator               begin()         noexcept;
constexpr const_iterator         begin()   const noexcept;
constexpr iterator               end()           noexcept;
constexpr const_iterator         end()     const noexcept;

constexpr reverse_iterator       rbegin()        noexcept;
constexpr const_reverse_iterator rbegin()  const noexcept;
constexpr reverse_iterator       rend()          noexcept;
constexpr const_reverse_iterator rend()    const noexcept;

constexpr const_iterator         cbegin()        noexcept;
constexpr const_iterator         cend()    const noexcept;
constexpr const_reverse_iterator crbegin()       noexcept;
constexpr const_reverse_iterator crend()   const noexcept;
```

the following holds:

- Requirements: none.
- Enabled: always.
- Complexity: constant time and space.
- Exception safety: never throw.
- Constexpr: always.
- Effects: none.

There are also some guarantees between the results of `data` and the iterator
functions that are explained in the section "Element / data access" below.

### Size / capacity

For the following size / capacity functions: 

```c++
constexpr size_type size()     const noexcept;
static constexpr size_type capacity() noexcept;
static constexpr size_type max_size() noexcept;
constexpr bool empty() const noexcept;
```
the following holds:

- Requirements: none
- Enabled: always.
- Complexity: constant time and space.
- Exception safety: never throw.
- Constexpr: always.
- Effects: none.

Note:
  - if `capacity() == 0`, then `sizeof(inline_vector) == 0`,
  - if `sizeof(T) == 0 and capacity() > 0`, then `sizeof(inline_vector) == sizeof(unsigned char)`.

For the checked resize functions:

```c++
constexpr void resize(size_type sz);
constexpr void resize(size_type sz, const value_type& c);
```
the following holds:

- Requirements: DefaultInsertable/CopyInsertable.
- Enabled: if requirements satisfied.
- Complexity: O(N) time, O(1) space.
- Exception safety:
   - basic guarantee: all constructed elements shall be destroyed on failure,
   - rethrows if `value_type`'s default or copy constructors throws,
   - throws `bad_alloc` if `new_size > capacity()`.
- Constexpr: if `is_trivial<value_type>`.
- Effects:
  - if `new_size > size` exactly `new_size - size` elements default/copy constructed.
  - if `new_size < size`:
      - exactly `size - new_size` elements destroyed.
      - all iterators pointing to elements at position > `new_size` are invalidated.

### Element /data access

For the unchecked element access functions:

```c++
constexpr reference       operator[](size_type n) noexcept; 
constexpr const_reference operator[](size_type n) const noexcept;
constexpr reference       front() noexcept;
constexpr const_reference front() const noexcept;
constexpr reference       back() noexcept;
constexpr const_reference back() const noexcept;
```

the following holds:

- Requirements: none.
- Enabled: always.
- Complexity: O(1) in time and space.
- Exception safety: never throws.
- Constexpr: if `is_trivial<value_type>`.
- Effects: none.


For the checked element access functions:

```c++
constexpr const_reference at(size_type n) const;
constexpr reference       at(size_type n);
```

the following holds:

- Requirements: none.
- Enabled: always.
- Complexity: O(1) in time and space.
- Exception safety:
  - throws `out_of_range` if `n >= size()`.
- Constexpr: if `is_trivial<value_type>`.
- Effects: none.

For the data access:

```c++
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;
```

the same as for the unchecked element access holds. But furthermore:

- if the container is empty, `data() == begin() == end()`
- if the container has zero-capacity, `data() == begin() == end() == implementation-defined`

### Modifiers

For the modifiers:

```c++
template<class... Args>
constexpr void emplace_back(Args&&... args);
```

```c++
constexpr void push_back(const value_type& x);
constexpr void push_back(value_type&& x);
```

```c++
constexpr void pop_back();
```

```c++
template<class... Args>
  constexpr void emplace_back(Args&&... args);
```

```c++
constexpr iterator insert(const_iterator position, const value_type& x);
constexpr iterator insert(const_iterator position, value_type&& x);
constexpr iterator insert(const_iterator position, size_type n, const value_type& x);
template<class InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);
constexpr iterator insert(const_iterator position, initializer_list<value_type> il);
```

```c++
constexpr iterator erase(const_iterator position)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator erase(const_iterator first, const_iterator last)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
```


```c++
constexpr void clear() noexcept(is_nothrow_destructible<value_type>{});
```

```c++
constexpr void swap(inline_vector&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));
```

the following holds:

- Requirements: `Default/Copy/MoveInsertable<value_type>` for default, copy, and move insertion operations, respectively.
- Enabled: always.
- Complexity: O(N) where N is the number of elements being constructed, inserted, or destroyed.
- Exception safety: Throw only if default/copy/move construction/assignment or destruction of T can throw.
- Constexpr: if `is_trivial<value_type>`.
- Effects: the size of the container increases/decreases by the number of elements being inserted/destroyed.

### Comparison operators

The following operators are `noexcept` if the operations required to compute them are all `noexcept`:

```c++
constexpr bool operator==(const inline_vector& a, const inline_vector& b);
constexpr bool operator!=(const inline_vector& a, const inline_vector& b);
constexpr bool operator<(const inline_vector& a, const inline_vector& b);
constexpr bool operator<=(const inline_vector& a, const inline_vector& b);
constexpr bool operator>(const inline_vector& a, const inline_vector& b);
constexpr bool operator>=(const inline_vector& a, const inline_vector& b);
```
The following holds for the comparison operators:

- Enabled/Requirements: only enabled if `value_type` supports the corresponding operations.
- Complexity: for two vectors of sizes N and M, the complexity is O(1) if N != M, otherwise, the comparison operator
  of `value_type` is invoked at most N times.
- Exception safety: `noexcept` if the comparison operator of `value_type` is `noexcept`, otherwise can only throw if the comparison operator can throw.

# Acknowledgments

The authors of Boost.Container's `boost::container::static_vector` (Adam
Wulkiewicz, Andrew Hundt, and Ion Gaztanaga). Howard Hinnant for libc++
`<algorithm>` and `<vector>` headers, and in particular, for the `<vector>` test
suite which was extremely useful while prototyping an implementation. Andrzej 
Krzemieński for providing an example that shows that using tags is better than
using static member functions for "special constructors" (like the default initialized 
constructor).

# References

- [Boost.Container::static_vector][boost_static_vector].
  - Discussions in the Boost developers mailing list:
    - [Interest in StaticVector - fixed capacity vector](https://groups.google.com/d/topic/boost-developers-archive/4n1QuJyKTTk/discussion).
    - [Stack-based vector container](https://groups.google.com/d/topic/boost-developers-archive/9BEXjV8ZMeQ/discussion).
    - [static_vector: fixed capacity vector update](https://groups.google.com/d/topic/boost-developers-archive/d5_Kp-nmW6c/discussion).
- [Boost.Container::small_vector][boostsmallvector].
- [Howard Hinnant's stack_alloc][stack_alloc].
- [EASTL fixed_vector][eastl] and [design][eastldesign].
- [Folly small_vector][folly].
- [LLVM small_vector][llvmsmallvector].

<!-- Links -->
[stack_alloc]: https://howardhinnant.github.io/stack_alloc.html
[inline_vector]: http://github.com/gnzlbg/inline_vector
[boost_static_vector]: http://www.boost.org/doc/libs/1_59_0/doc/html/boost/container/static_vector.html
[travis-shield]: https://img.shields.io/travis/gnzlbg/inline_vector.svg?style=flat-square
[travis]: https://travis-ci.org/gnzlbg/inline_vector
[coveralls-shield]: https://img.shields.io/coveralls/gnzlbg/inline_vector.svg?style=flat-square
[coveralls]: https://coveralls.io/github/gnzlbg/inline_vector
[docs-shield]: https://img.shields.io/badge/docs-online-blue.svg?style=flat-square
[docs]: https://gnzlbg.github.io/inline_vector
[folly]: https://github.com/facebook/folly/blob/master/folly/docs/small_vector.md
[eastl]: https://github.com/questor/eastl/blob/master/fixed_vector.h#L71
[eastldesign]: https://github.com/questor/eastl/blob/master/doc%2FEASTL%20Design.html#L284
[clump]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0274r0.pdf
[boostsmallvector]: http://www.boost.org/doc/libs/master/doc/html/boost/container/small_vector.html
[llvmsmallvector]: http://llvm.org/docs/doxygen/html/classllvm_1_1SmallVector.html
