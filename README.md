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

- memory allocation is not possible (e.g. in the absence of a memory
  heap/free-store) or when it imposes an unacceptable performance penalty (e.g.
  when the latency introduced by the initial heap allocation of a vector is too
  big, or when the performance is dominated by a lot of memory allocations)
- static allocation of objects with complex lifetimes is required,
- storing a number of non-default constructible objects (`std::array` requires
  default constructible by design),
- the user needs full control over where the memory of the vector and its
  elements is stored.

Consider a dynamically-resizable vector of polygons (with a varying number of
points) implemented using `std::vector`:

```c++
using point_t = std::array<float, 3>;
using polygon_t = std::vector<point_t>;
using polygons_t = std::vector<polygon_t>;
polygons_t polygon;
```

This data-structure is a vector of vectors. It has, among others, the following
problems (i'll keep this short):

- two access a point of a polygon we need to dereference two pointers,
- to append a new polygon to it at least one memory allocation is requred,
- the memory layout of the points will be, without extra effort, non-contiguous.

If we had a bound on the maximum number of points a polygon can have, we could
significantly improve our data-structure by using a `std::array`:

```c++
using improved_polygon_t = std::pair<std::size_t, std::array<point_t, max_no_points>>;
```

However to handle a varying number of points, we need to keep track of the size
of these arays (among other things). At this point we have reimplemented a poor
`inline_vector<T, Capacity>`. That is, a dynamically-resizable vector of
compile-time bounded capacity with inline storage.

Using this proposal we can just transform our original polygon implementation to:

```c++
using polygon_t = inline_vector<point_t, max_no_points>;
```

and keep using the original interface. We can then construct polygons on the
stack of a function, and push them back into our vector of polygons without any
memory allocations. Whether the polygon and its points are stored on the stack,
the heap, or in static memory, is left up to the user of the `inline_vector<T, C>`
template.


# Design considerations

## Previous work

This proposal is strongly inspired by
[`boost::container::static_vector` (1.59)][boost_static_vector], which was
introduced in 2013 into the Boost.Container library .


A prototype implementation of this proposal is provided here:
[`http://github.com/gnzlbg/inline_vector`][inline_vector]

## Naming (bikeshedding)

The name `inline_vector<T, Capacity>` denotes that the elements are stored
"inline" with the object itself.

Alternative names are, among others:

- `stack_vector`: which is a lie since the elements won't always be on the stack.
- `static_vector`: Boost.Container's name for it due to its ability to allocate its elements in static memory.
- `fixed_capacity_vector/fixed_vector`: because its capacity is fixed.
- `internal_vector`: because the elements are stored in the object.
- `small_vector`: becuase its capacity _should_ be small.

## General requirements

The general requirements of `inline_vector<T,C>` are:

  1. dynamically-resizable contiguous random-access sequence container with O(1)
  insert/erase at the end, and O(N) insert/erase otherwise,
  2. its elements are stored internally within the object,
  3. it's API should be as similar to `vector<T,A>` API as possible,
  4. differences with `vector<T, A>`s API should be as explicit as possible,
  5. it should be a zero-cost abstraction.

## Failed approach: Reusing std::vector

Reusing a `std::vector` with a new allocator does not fullfill most of the
requirements.

  1. The growth factor and mechanism of `std::vector` is implementation
     dependent. Hence the size of the allocator cannot be fixed in a portable,
     reliable, and performant way. There is no way to fix the capacity of a
     vector. If an implementation decides to use a dynamic growth factor for a
     vector, this would be impossible.

  2. To make point 1 at least safe, some kind of fall-back to the heap would be
     required. Since unavailable heap usage / unacceptable heap usage performance
     is one of the main reasons to use an `inline_vector<T,C>`, this is not acceptable.

  3. One would need a wrapper type, that stores the vector and its elements.
     This wrapper type would be at least two words of memory too big (the unused
     capacity of `std::vector`, and an extra data pointer). 

Note: proposal proposes only an `inline_vector<T,C>` without heap-fallback, but
another proposal should pursue a `small_vector` with heap-fallback. These types
cannot be efficiently build on top of `std::vector` with custom allocators. They
_can_ to some extent be built, but the result is not a zero-cost abstraction.

## Introducing a new container

The main draw-back of introducing a `inline_vector<T, Capacity>` type is code
bloat. For each for each type `T`, and for each `Capacity`, a signficiant amount
of code will be generated.

Since type-erasing the capacity would hinder performance, the only solution to
this problems is that users that don't need this type don't use it. Placing this
container in a new header file `<experimental/inline_vector>` is the best that
can be done.

### Concrete requirements

Zero-cost abstraction:

  1. zero-size if `Capacity == 0`, in which case `data() == begin() == end() ==
     unspecified unique value` (`nullptr` is intended), and `swap` is
     `noexcept(true)`.

  2. fully usable within constexpr functions for literal types.


Safety:

  3. Same exception safety as `std::vector` (with minor differences for throwing
     copy/move constructors). Discussed in sub-section Exception Safety
 
Usability:

  4. API as close to `std::vector` as possible

  5. Explicit instatiation support (for Copy and MoveConstructible types).

  6. Open question: interoperability of `inline_vector<T,C>`s of different capacities.
     (probably not, different capacities require `swap` to be allowed to throw).


### Exception Safety

he only operations that can actually fail within `inline_vector<T,C>` are:

  1. `T` constructors, assignment, destructor, and swap,
  2. insertion exceeding the capacity (`push_back`, `insert`, `emplace`, ..),
  3. out-of-bounds access (`front/back/pop_back` when empty, unchecked
     random-access).

Like for `std::vector`, if (1) fails, the basic guarantee is provided and the
exception is rethrown.

Point (2) is an open question. The capacity of the vector is fixed at
compile-time. Exceeding the capacity can be considered:

  - a) undefined behavior and be asserted?
  - b) a logic error, and should throw a `logic_error` exception?
  - c) an out-of-memory error, since the vector cannot grow, and should be throw a
    `bad_alloc` exception?

I lean towards a) undefined behavior and asserted (run-time diagnostic
required). Throwing an exception would require checking the capacity on every
call to `push_back`, but the user might have done that before using `push_back`
in a loop. Making this undefined behavior allows an implementation to emit a
diagnostic in debug builds.

An alternative would be to preserve the exception semantics of `std::vector`
(i.e. throw on out-of-memory/logic-error in `push_back`), but offer `_unchecked`
methods that might provide a run-time diagnostic:

  - checked (throws `bad_alloc` like `std::vector`): `push_back`,
    `emplace_back`, `emplace`, `insert`, ...
  - unchecked (undefined behavior, assertion encouraged): `push_back_unchecked`,
  `emplace_back_unchecked`, `emplace_unchecked`, `insert_unchecked`,
  `resize_unchecked`

Note: an alternative to `_unchecked`-named functions would be to use tag
dispatching with, e.g., `std::unchecked_t`.

### (Optional) Default initialization

Default initialization of elements could be provided using tag dispatching or
static member functions:

```c++
static constexpr inline_vector default_initialized(size_t n);
constexpr void resize_default_initialized(size_type sz);
constexpr void resize_unchecked_default_initialized(size_type sz);
```

In this proposal the elements of the vector are always value initialized, since
default-initialization extensions should be considered for all containers (e.g.
`std::vector`)

### Iterators

The iterator invalidation rules are very different than those for `std::vector`,
since:

- moving a vector into another vector invalidates all iterators,
- swapping two vectors invalidates all iterators, but 
- inserting elements does not invalidate iterators.

The following functions can potentially invalidate the iterators of the vector:

- `resize(n)`, `resize(n, v)`, `resize_default_initialized(n)`
- `resize_unchecked(n)`, `resize_unchecked(n, v)`, `resize_default_initialized_unchecked(n)`
- `pop_back`
- `erase`
- `swap`

Since `std::reverse_iterator` is not constexpr friendly, reverse iterators
cannot be constexpr. Since making a function constexpr is a breaking change,
making `reverse_iterator` constexpr should be studied in a different proposal.

## Storage/Memory Layout

- The elements of the vector are aligned to an `alignof(T)` memory address
  (properly aligned).

- The size parameter is to be picked as the smalles integer type in which
  `Capacity` fits.
  
- Aggregate initialization of `inline_vector` is not required.

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
