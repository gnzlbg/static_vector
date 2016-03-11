# inline_vector [![Travis build status][travis-shield]][travis] [![Coveralls.io code coverage][coveralls-shield]][coveralls] [![Docs][docs-shield]][docs]

> A dynamically-resizable vector with fixed capacity and inline storage (revision -1)


# Introduction

This paper proposes a dynamically-resizable `vector` with fixed capacity and
contiguous inline storage. That is, the elements of the vector are stored within
the vector object itself.

Its API is almost a 1:1 map of `std::vector<T, A>`s API. It is a sequence
random-access container with contiguous storage, O(1) insertion and removal of
elements at the end, and O(N) insertion and removal otherwise. Like
`std::vector`, the elements are initialized on insertion and destroyed on
removal.

# Motivation

This container is useful when:

- memory allocation is not possible, e.g., most embedded enviroments only
  provide stack and static memory but no heap,
- memory allocation imposes an unacceptable performance penalty, e.g., the
  latency introduced by a memory allocation,
- allocation of objects with complex lifetimes in the _static_-memory segment is required,
- non-default constructible objects must be stored such that `std::array` is not an option,
- full control over the storage location of the vector elements is required.

# Design considerations

## Can we reuse `std::vector` with a custom allocator? 

Yes, we can, but no, it does not result in a zero-cost abstraction. Two main
reasons:

The growth mechanism of `std::vector` makes it impossible for a custom allocator
to allocate the optimal amount of storage for a given capacity.

Furthermore, the resulting vector would be at least one word too big, since
storing the `capacity` as a data member is not necessary for `inline_vector`
because it is known at compile-time.

## Can we reuse `small_vector`?

Yes, we can, but no, it does not result in a zero-cost abstraction.

The paper "(PR0274: Clump – A Vector-like Contiguous Sequence Container with
Embedded
Storage)[http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0274r0.pdf]"
proposes a new type, `small_vector<T, N, Allocator>`, which is essentially a
`std::vector<T, Allocator>` that performs a Small Vector Optimization for up to
`N` elements. This small vector type is implemented in Boost, LLVM, EASTL, and
Folly.

However, most of these libraries special case small vector in the spirit of
`vector<bool>` for the case in which only inline storage is desired. The only
library that offers it as a completely different type is Boost.Container.

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
  heap (swap a point) or on the stack (must always copy).

- `small_vector` cannot be as efficient as `inline_vector`. It must store at
  least a bit to discriminate between inline and allocator storage, and most of
  its API must, at run-time, choose a different code-path depending on the active
  storage scheme.

The only way to fix `small_vector` would be to special case it for
`Allocator::max_size() == 0` (which isn't constexpr yet), and to provide an API
that has different complexity and exception-safety guarantees than
`small_vector`s with `Allocator::max_size() > 0`.


The author of this proposal considers that special casing should not be done.
`inline_vector` is a different type, with different algorithmic and
exception-safety guarantees. Let's not repeat `vector<bool>` all over again.

However, even if the standardization process decideds to special case
`small_vector` for inline-storage-only, having a full-fleshed proposal for that
special case will still be immensely useful at that point.
    
## Existing practice

There are at least 3 widely used implementations of `inline_vector`.

This proposal is strongly inspired by Boost.Container, which offers
 [`boost::container::static_vector<T, Capacity>` (1.59)][boost_static_vector].
 Boost.Container also offers `boost::container::small_vector<T, N, Allocator>`
 as a different type.

The other two libraries that implement `inline_vector` are [Folly][folly] and
[EASTL][eastl]. Both of these libraries implement it as a special case of
`small_vector` (which in both of these libraries has 4 template parameters).

EASTL `small_vector` is called `fixed_vector<T, N,
hasAllocator,OverflowAllocator>` and uses a boolean template parameter to
indicate if the only storage mode available is inline storage. The
[design documents of EASTL][eastldesign] seem to predate this special casing
since they actually argue against it. The two main arguments are:

- special casing complicates the implementation of `small_vector`,
- (without proof) the resulting code size increase would be larger than the "4
  bytes" that can be saved in storage per vector for the special case.

The current implementation does, however, special case `small_vector` for inline
storage. No rationale for this decision is given in the design documentation.

Folly implements `small_vector<T, N, NoHeap, size_type>`, where if the tag
`NoHeap` is present a non-customizable allocator is used. Interestingly, folly
allows customizing the `size_type` of the `small_vector`. The rationale for this
decision is not described, but a reason could be that since the maximum number
of elements for the `NoHeap` case is known at compile-time, a `size_type` can be
chosen to reduce `inline_vector`s memory requirements.


## Current design

The current design follows that of `boost::container::static_vector<T,
Capacity>` closely.

It introduces a new type `std::experimental::inline_vector<T, Capacity>` in the
`<experimental/inline_vector>` header.

> `inline_vector` is a dynamically-resizable contiguous random-access sequence
> container with O(1) insert/erase at the end, and O(N) insert/erase otherwise.
> Its elements are stored within the container ojbect itself.

A prototype implementation of this proposal is provided here for standardization
purposes: [`http://github.com/gnzlbg/inline_vector`][inline_vector].

The main drawback of introducing a new type is, as the
[design document of the EASTL][eastldesign] points out, increased code size.
This is opt-in, those who don't want to pay for it are free not to include the
header file and not to use the container. For those who want to use it,
type-erasing the capacity is not an option, but reducing code-size by defining
an `inline_vector_base<T>` to implement `Capacity`-agnostic functionality is
left as a quality of implementation issue.

## Storage/Memory Layout

The elements of the vector are aligned to an `alignof(T)` memory address
(properly aligned).

The `inline_vector<T,Capacity>::size_type` is the smallest unsigned integer type
that can store the representation of `Capacity` without loosing information.
  
**Note**: `inline_vector` cannot be an aggregate since it provides user-defined
constructors.

### Zero-sized

The `sizeof(inline_vector<T, Capacity>)` is required to be zero if:

- `sizeof(T) == 0`, or

- `Capacity == 0`.

In both cases `data() == begin() == end() == unspecified unique value`
(`nullptr` is intended), and `swap` is `noexcept(true)`.

### Constexpr-support

The whole API of `inline_vector<T, Capacity>` is `constexpr` if `LiteralType<T>`
is true.

Since `std::reverse_iterator` is not constexpr friendly in C++14, the only
exception for this is the member functions returning a reverse iterator.

### Interoperability of inline vectors with different capacities

This is not pursued in this proposal since other containers do not provide it
(e.g. like `std::array`), and it would complicate the exception-safety
specification of, e.g., `swap`, significantly. This can be pursued as a
backwards-compatible extension in the future is there is desire to do so.


### Explicit instantiatiability

`inline_vector<T, Capacity>` should be able to be explicitly instantiated for
all combinations of `T` and `Capacity`.

### Exception Safety

The only operations that can actually fail within `inline_vector<T,C>` are:

  1. `T` constructors, assignment, destructor, and swap. These can fail only due
     to throwing constructors/assignment/destructors/swap of `T`. If these
     operations are `noexcept(true)` for `T`, they are `noexcept(true)` for
     `inline_vector`. Since the storage is inline only the basic guarantee can
     be provided by these if `noexcept(false)`.

  2. Out-of-bounds unchecked access (`front/back/pop_back` when empty, unchecked
     random-access). Defined as undefined behavior.

  3. Out-of-bounds checked access, `at`, which throws `out_of_range` exception,
     and provides the strong-guarantee.

  4. Insertion exceeding the capacity (`push_back`, `insert`, `emplace`, ..).
     **Open question**: what to do here.

The capacity of the vector is fixed at compile-time. Is exceeding the capacity,
on, e.g., `push_back`: a `bad_alloc`, `out_of_range`, or even a `logic_error`?

For API-similarity with `std::vector` throwing an exception is desired, but
should it be considered to define these as undefined behavior and encourage
implementations to provide an `assertion` as a quality of implementation issue?

Since `push_back` for `inline_vector` can be much cheaper than for `std::vector`
(since memory will never be reallocated), should we provide `_unchecked`
variants of these operations in which exceeding the capacity is defined as
undefined behavior?

Should we make all of these operations unchecked?

The current implementation actually makes all of these operations unchecked, and
provides an assertion. But a good balance that significantly increases the API
could be:

  - checked (throws `bad_alloc` like `std::vector`): `push_back`,
    `emplace_back`, `emplace`, `insert`, ...
  - unchecked (undefined behavior, assertion encouraged): `push_back_unchecked`,
  `emplace_back_unchecked`, `emplace_unchecked`, `insert_unchecked`,
  `resize_unchecked`

    Note: an alternative to `_unchecked`-named functions would be to use tag
dispatching with, e.g., `std::unchecked_t`.

### Default initialization

The size-modifying operations of the `inline_vector` that do not require a value
also have the following analogous member functions that perform default
initialization instead of value initialization:

```c++
static constexpr inline_vector default_initialized(size_t n);
constexpr void resize_default_initialized(size_type sz);
constexpr void resize_unchecked_default_initialized(size_type sz);
```

Note 0: an alternative would be to use tag dispatching for selecting default
initialization of the vector elements.

Note 1: default initialization should be probably considered as a feature for
all the standard library containers that can support it.

### Iterators

The iterator invalidation rules are different than those for `std::vector`,
since:

- moving a vector into another vector invalidates all iterators,
- swapping two vectors invalidates all iterators, and 
- inserting elements never invalidates iterators.

The following functions can potentially invalidate the iterators of the vector:

- `resize(n)`, `resize(n, v)`, `resize_default_initialized(n)`
- `resize_unchecked(n)`, `resize_unchecked(n, v)`, `resize_default_initialized_unchecked(n)`
- `pop_back`
- `erase`
- `swap`

### Naming (bikeshedding)

The name `inline_vector<T, Capacity>` denotes that the elements are stored
"inline" with the object itself.

Alternative names are, among others:

- `stack_vector`: which is a lie since the elements won't always be on the stack.
- `static_vector`: Boost.Container's name for it due to its ability to allocate its elements in static memory.
- `fixed_capacity_vector/fixed_vector`: because its capacity is fixed.


## Proposed API

```c++
template<typename T, std::size_t C>
struct inline_vector<T, Size> {

// types:
typedef value_type& reference;
typedef value_type const& const_reference;
typedef implementation-defined iterator;
typedef implementation-defined const_iterator;
typedef size_t size_type;
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
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector(inline_vector<value_type, M> const& other);
    noexcept(is_nothrow_copy_constructible<value_type>{} and C >= M);
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector(inline_vector<value_type, M> && other)
    noexcept(is_nothrow_move_constructible<value_type>{} and C >= M);
constexpr inline_vector(inline_vector const& other);
  noexcept(is_nothrow_copy_constructible<value_type>{});
constexpr inline_vector(inline_vector&& other)
  noexcept(is_nothrow_move_constructible<value_type>{});
constexpr inline_vector(initializer_list<value_type> il);

/* constexpr ~inline_vector(); */  // implicitly generated

constexpr inline_vector<value_type, C>& operator=(inline_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
constexpr inline_vector<value_type, C>& operator=(inline_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector<value_type, C>& operator=(inline_vector<value_type, M>const& other)
    noexcept(is_nothrow_copy_assignable<value_type>{} and C >= M);
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector<value_type, C>& operator=(inline_vector<value_type, M>&& other);
    noexcept(is_nothrow_move_assignable<value_type>{} and C >= M);

template<class InputIterator>
constexpr void assign(InputIterator first, InputIterator last);
constexpr void assign(size_type n, const value_type& u);
constexpr void assign(initializer_list<value_type> il);

// iterators:
constexpr iterator               begin()         noexcept;
constexpr const_iterator         begin()   const noexcept;
constexpr iterator               end()           noexcept;
constexpr const_iterator         end()     const noexcept;

          reverse_iterator       rbegin()        noexcept;
          const_reverse_iterator rbegin()  const noexcept;
          reverse_iterator       rend()          noexcept;
          const_reverse_iterator rend()    const noexcept;

constexpr const_iterator         cbegin()        noexcept;
constexpr const_iterator         cend()    const noexcept;
          const_reverse_iterator crbegin()       noexcept;
          const_reverse_iterator crend()   const noexcept;


// size/capacity:
constexpr size_type size()     const noexcept;
static constexpr size_type capacity() noexcept;
static constexpr size_type max_size() noexcept;
constexpr void resize(size_type sz);
constexpr void resize(size_type sz, const value_type& c);
constexpr bool empty() const noexcept;
void reserve(size_type n) /* QoI */ = deleted;
void shrink_to_fit() /* QoI */ = deleted; 

constexpr void resize_default_initialized(size_type sz);

constexpr void resize_unchecked(size_type sz)
  noexcept(is_nothrow_default_constructible<T>{} and is_nothrow_destructible<T>{});
constexpr void resize_unchecked(size_type sz, const value_type& c)
  noexcept(is_nothrow_copy_constructible<T>{} and is_nothrow_destructible<T>{});
constexpr void resize_unchecked_default_initialized(size_type sz)
  noexcept(is_nothrow_default_constructible<T>{} and is_nothrow_destructible<T>{});


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
  constexpr iterator emplace(const_iterator position, Args&&...args);
constexpr iterator insert(const_iterator position, const value_type& x);

constexpr iterator insert(const_iterator position, value_type&& x);
constexpr iterator insert(const_iterator position, size_type n, const value_type& x);
template<class InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);

constexpr iterator insert(const_iterator position, initializer_list<value_type> il);


template<class... Args>
  constexpr void emplace_back_unchecked(Args&&... args)
    noexcept(NothrowConstructible<value_type, Args...>{});  // TODO
constexpr void push_back_unchecked(const value_type& x)
  noexcept(is_nothrow_copy_constructible<value_type>{});
constexpr void push_back_unchecked(value_type&& x)
  noexcept(is_nothrow_move_constructible<value_type>{});

template<class... Args>
  constexpr iterator emplace_unchecked(const_iterator position, Args&&...args)
    noexcept(NothrowConstructible<value_type, Args...>{} and is_nothrow_swappable<value_type>{});  // TODO

constexpr iterator insert_unchecked(const_iterator position, const value_type& x)
  noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr iterator insert_unchecked(const_iterator position, value_type&& x)
  noexcept(is_nothrow_move_constructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator insert_unchecked(const_iterator position, size_type n, const value_type& x)
    noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});
template<class InputIterator>
  constexpr iterator insert_unchecked(const_iterator position, InputIterator first, InputIterator last)
    noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr iterator insert_unchecked(const_iterator position, initializer_list<value_type> il)
  noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr iterator erase(const_iterator position)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator erase(const_iterator first, const_iterator last)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr void clear() noexcept(is_nothrow_destructible<value_type>{});

constexpr void swap(inline_vector<value_type, C>&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));
};

// TODO: noexcept specification missing
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator==(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator!=(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<=(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>=(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b);

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
/// - it is guaranteed that no elements will be constructed unless `value_type`
/// models `TrivialType`, in which case this guarantee is implementation defined.
///
constexpr inline_vector() noexcept;
```

```c++
/// Constructs a inline_vector containing \p n default-inserted elements.
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
/// - rethrows if `value_type`'s default constructor throws,
/// - throws `bad_alloc` if `\p n > capacity()`.
///
/// Constexpr: if `value_type` models `TrivialType`.
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
/// Constexpr: if `value_type` models `TrivialType`.
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
/// Constexpr: if `value_type` models `TrivialType`.
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
/// Constexpr: if `value_type` models `TrivialType`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `other.size()` calls to `value_type`s copy constructor.
///
constexpr inline_vector(inline_vector const&);
  noexcept(is_nothrow_copy_constructible<value_type>{});
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
/// - rethrows if `value_type`'s copy constructor throws,
/// - if `C<M`: throws `bad_alloc` if \p `other.size() > capacity()`.
///
/// Constexpr: if `value_type` models `TrivialType`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `other.size()` calls to `value_type`s copy constructor.
///
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector(inline_vector<value_type, M> const& other);
    noexcept(is_nothrow_copy_constructible<value_type>{} and C >= M);
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
/// Constexpr: if `value_type` models `TrivialType`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `other.size()` calls to `value_type`s move constructor.
///
constexpr inline_vector(inline_vector&&)
  noexcept(is_nothrow_move_constructible<value_type>{});
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
/// - rethrows if `value_type`'s move constructor throws,
/// - if `C<M`: throws `bad_alloc` if \p `other.size() > capacity()`.
///
/// Constexpr: if `value_type` models `TrivialType`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `other.size()` calls to `value_type`s move constructor.
///
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector(inline_vector<value_type, M> &&)
    noexcept(is_nothrow_move_constructible<value_type>{} and C >= M);
```


```c++
/// Constructs a inline_vector with elements copied from [il.begin(), il.end()).
///
/// Requirements: `value_type` shall be `CopyInsertable` into `*this`
///
/// Enabled: if requirements are met.
///
/// Complexity:
/// - time: O(N) calls to `value_type`'s copy constructor,
/// - space: O(1).
///
/// Exception safety: 
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - rethrows if `value_type`'s copy or move constructors throws,
/// - throws `bad_alloc` if `\p il.size() > capacity()`.
///
/// Constexpr: if `value_type` models `TrivialType`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p `il.size()` calls to `value_type`s copy constructor.
///
constexpr inline_vector(initializer_list<value_type> il);
```

```c++
/// Returns a inline_vector containing \p n default-initialied elements.
///
/// Requirements: none.
///
/// Enabled: always.
///
/// Complexity:
/// - time: O(N).
/// - space: O(1).
///
/// Exception safety:
/// - basic guarantee: all constructed elements shall be destroyed on failure,
/// - throws `bad_alloc` if `\p n > capacity()`.
///
/// Constexpr: if `value_type` models `TrivialType`.
///
/// Iterator invalidation: none.
///
/// Effects: exactly \p n default initializations of `value_type`.
/// - it is guaranteed that the element's will not be value-initialized.
///
static constexpr inline_vector default_initialized(size_t n);
```

### Assignment

```c++
constexpr inline_vector<value_type, C>& operator=(inline_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
```

```c++
constexpr inline_vector<value_type, C>& operator=(inline_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
```

```c++
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector<value_type, C>& operator=(inline_vector<value_type, M>const& other)
    noexcept(is_nothrow_copy_assignable<value_type>{} and C >= M);
```

```c++
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr inline_vector<value_type, C>& operator=(inline_vector<value_type, M>&& other);
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
if `value_type` models `TrivialType`. TODO: noexcept-ness?

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

          reverse_iterator       rbegin()        noexcept;
          const_reverse_iterator rbegin()  const noexcept;
          reverse_iterator       rend()          noexcept;
          const_reverse_iterator rend()    const noexcept;

constexpr const_iterator         cbegin()        noexcept;
constexpr const_iterator         cend()    const noexcept;
          const_reverse_iterator crbegin()       noexcept;
          const_reverse_iterator crend()   const noexcept;
```

the following holds:

- Requirements: none.
- Enabled: always.
- Complexity: constant time and space.
- Exception safety: never throw.
- Constexpr: always, except:
  - for the `reverse_iterator` functions, since `std::reverse_iterator<Iterator>`
  will not be `constexpr` even for types like `T*`.
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

- Requirements: none.
- Enabled: always.
- Complexity: constant time and space.
- Exception safety: never throw.
- Constexpr: always.
- Effects: none.


For the checked resize functions:

```c++
constexpr void resize(size_type sz);
constexpr void resize(size_type sz, const value_type& c);
constexpr void resize_default_initialized(size_type sz);
```
the following holds:

- Requirements: DefaultInsertable/CopyInsertable.
- Enabled: if requirements satisfied.
- Complexity: O(N) time, O(1) space.
- Exception safety:
   - basic guarantee: all constructed elements shall be destroyed on failure,
   - rethrows if `value_type`'s default or copy constructors throws,
   - throws `bad_alloc` if `new_size > capacity()`.
- Constexpr: if type models TrivialType.
- Effects:
  - if `new_size > size` exactly `new_size - size` elements default/copy constructed.
  - if `new_size < size`:
      - exactly `size - new_size` elements destroyed.
      - all iterators pointing to elements at position > `new_size` are invalidated.

For the unchecked resize functions:

```c++
constexpr void resize_unchecked(size_type sz)
  noexcept(is_nothrow_default_constructible<T>{} and is_nothrow_destructible<T>{});
constexpr void resize_unchecked(size_type sz, const value_type& c)
  noexcept(is_nothrow_copy_constructible<T>{} and is_nothrow_destructible<T>{});
constexpr void resize_unchecked_default_initialized(size_type sz)
  noexcept(is_nothrow_default_constructible<T>{} and is_nothrow_destructible<T>{});
```

the following holds:

- Requirements: DefaultInsertable/CopyInsertable.
- Enabled: if requirements satisfied.
- Complexity: O(N) time, O(1) space.
- Exception safety:
  - basic guarantee if default/copy construction throws
  - undefined behavior if `new_size > capacity()`
- Constexpr: if type models TrivialType.
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
- Constexpr: if type models TrivialType.
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
- Constexpr: if type models TrivialType.
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


```c++
template<class... Args>
constexpr void emplace_back(Args&&... args);
```

``` c++
template<class... Args>
  constexpr void emplace_back_unchecked(Args&&... args)
    noexcept(NothrowConstructible<value_type, Args...>{});
```

```c++
constexpr void push_back(const value_type& x);
constexpr void push_back(value_type&& x);
```
```c++
constexpr void push_back_unchecked(const value_type& x)
  noexcept(is_nothrow_copy_constructible<value_type>{});
constexpr void push_back_unchecked(value_type&& x)
  noexcept(is_nothrow_move_constructible<value_type>{});
```

```c++
constexpr void pop_back();
```

```c++
template<class... Args>
  constexpr void emplace_back(Args&&... args);
```

```c++
template<class... Args>
  constexpr iterator emplace_unchecked(const_iterator position, Args&&...args)
    noexcept(NothrowConstructible<value_type, Args...>{} and is_nothrow_swappable<value_type>{});  // TODO
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
constexpr iterator insert_unchecked(const_iterator position, const value_type& x)
  noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr iterator insert_unchecked(const_iterator position, value_type&& x)
  noexcept(is_nothrow_move_constructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator insert_unchecked(const_iterator position, size_type n, const value_type& x)
    noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});
template<class InputIterator>
  constexpr iterator insert_unchecked(const_iterator position, InputIterator first, InputIterator last)
    noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr iterator insert_unchecked(const_iterator position, initializer_list<value_type> il)
  noexcept(is_nothrow_copy_constructible<value_type>{} and is_nothrow_swappable<value_type>{});
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
constexpr void swap(inline_vector<value_type, C>&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));
```

### Comparision operators

Here `noexcept(auto)` is used to denote `noexcept(true)` if the operations
required by the function are all `noexcept(true)` (I can't type these noexcept
specifications anymore).

```c++
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator==(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b) noexcept(auto);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator!=(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b) noexcept(auto);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b) noexcept(auto);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<=(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b) noexcept(auto);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b) noexcept(auto);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>=(const inline_vector<value_type, C0>& a, const inline_vector<value_type, C1>& b) noexcept(auto);
```


# Acknowledgments

The authors of Boost.Container's `boost::container::static_vector` (Adam
Wulkiewicz, Andrew Hundt, and Ion Gaztanaga). Howard Hinnant for libc++
`<algorithm>` and `<vector>` headers, and in particular, for the `<vector>` test
suite.

# References

- [Boost.Container::static_vector][boost_static_vector].
  - Discussions in the Boost developers mailing list:
    - [Interest in StaticVector - fixed capacity vector](https://groups.google.com/d/topic/boost-developers-archive/4n1QuJyKTTk/discussion).
    - [Stack-based vector container](https://groups.google.com/d/topic/boost-developers-archive/9BEXjV8ZMeQ/discussion).
    - [static_vector: fixed capacity vector update](https://groups.google.com/d/topic/boost-developers-archive/d5_Kp-nmW6c/discussion).
- [Howard Hinnant's stack_alloc][stack_alloc].


# Appendix A: Standad wording

Not part of this revision.

# Appendix B: Rough edges in C++14

There are some rough edges in C++14 that complicate the implementation of this
proposal. These are listed here "for completeness". This proposal does not
propose anything about them.

  1. `std::reverse_iterator<It>` is never constexpr, even when `It` is a
      raw-pointer (which is constexpr).

  2. `std::array` is not fully `constexpr` (`data`, `begin/end`, `non-const
     operator[]`, `swap`...) and as a consequence `inline_vector`s implementation
     uses a C Array for trivially destructible types. If the types are also
     trivially constructible, then `std::array` would have been a better fit.

  3. the `<algorithms>` are not `constexpr` even though it is trivial to make
     all of them but 3 (the allocating algorithms) `constexpr`. The API of
     the allocating algorithms is broken. Fixing it would make it trivial to
     make them `constexpr` as well. The implementation of `inline_vector` needs
     to reimplement some algorithms because of this.

  4. the generic `begin`, `end`, and `swap` as well as their overloads for
     C-arrays are not `constexpr`.

  5. `std::aligned_storage` is impossible to use in `constexpr` code because one
     needs to:
     - `reinterpret_cast` to the elements pointer type,
     - use placement new, and
     - call explicit destructors.

     None of these three things can be used in `constexpr` code, and the
     implementation of `inline_vector` and `std::variant` suffers from this
     (`std::variant` needs to be implemented using a recursive union...).

  6. `std::initializer_list`:
     - doesn't have its size as part of its type,
     - its elements cannot be moved,
     - cannot be converted to a C-array or a `std::array` easily:
     ```c++
     std::initializer_list<int> il{1, 2, 3};
     // These all fail:
     const int ca[3] = il;
     const int ca[3](il);
     const int ca[3]{il};
     const int ca[3]{{il}};
     ```
 
  7. `<type_traits>` offers the "dangerous" `decay_t` but offers no `uncvref_t`
     (which is the type trait most used in `inline_vector`, that is,
     `ucvref_t<T> = std::remove_reference_t<std::remove_cv_t<T>>`).

  8.  `inline_vector` doesn't provide a `reserve` member function. It is
     explicitly deleted in its API to convey that this is not an accident, but
     there is no way to actually provide more information to the users (e.g. `=
     delete("message");`). Something like "Hey, you are trying to reserve memory
     with an inline vector but it has fixed capacity so the memory is already
     there." could be probably very helpful.

  9. special member functions cannot be disabled, the following doesn't work:
     ```c++
     #include <type_traits>
     using namespace std;
    
     template <typename T> 
     struct example {
       template <int d = 0, typename = enable_if_t<d == 1 || is_same<T, int>{}>>
       example(example const&) {}
       example(example&&) {}
       example() {}
     };
    
     int main() {
       example<int> a;
       example<int> b(a);  // since T is int, this should work but doesn't
       //example<double> c;
       //example<double> d(c);  // this obviously fail
	   return 0;
     }
         ```

<!-- Links -->
[stack_alloc]: https://howardhinnant.github.io/stack_alloc.html
[stack_vector]: http://github.com/gnzlbg/stack_vector
[boost_static_vector]: http://www.boost.org/doc/libs/1_59_0/doc/html/boost/container/static_vector.html
[travis-shield]: https://img.shields.io/travis/gnzlbg/stack_vector.svg?style=flat-square
[travis]: https://travis-ci.org/gnzlbg/stack_vector
[coveralls-shield]: https://img.shields.io/coveralls/gnzlbg/stack_vector.svg?style=flat-square
[coveralls]: https://coveralls.io/github/gnzlbg/stack_vector
[docs-shield]: https://img.shields.io/badge/docs-online-blue.svg?style=flat-square
[docs]: https://gnzlbg.github.io/stack_vector
[folly]: https://github.com/facebook/folly/blob/master/folly/docs/small_vector.md
[eastl]: https://github.com/questor/eastl/blob/master/fixed_vector.h#L71
[eastldesign]: https://github.com/questor/eastl/blob/master/doc%2FEASTL%20Design.html#L284
