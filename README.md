# stack_vector [![Travis build status][travis-shield]][travis] [![Coveralls.io code coverage][coveralls-shield]][coveralls] [![Docs][docs-shield]][docs]

> A dynamically-resizable vector with fixed capacity (revision -1)


**Design issues**

- broken `initializer_list` support. The current proposal has no initializer
  list constructor, but a variadic constructor that allows
  "initializer-list"-like usage and constexpr. Since this constructor is too
  gready, it is disabled for lists of < 2 elements... A decision has to be
  made here: either drop constexpr support and support initializer lists
  properly, or maybe don't support initializer lists at all.

- interoperability between vectors of different capacity: to which extent is it
  worth it to support this. What exception-safety guarantees should be provided.


# Introduction

This paper proposes a dynamically-resizable `vector` with fixed capacity and
contiguous internal storage, that is, the elements are stored within the vector
object itself. 

Its API is almost a 1:1 map of `std::vector<T, A>`s API. It is a sequence
random-access container with contiguous storage, O(1) insertion and removal of
elements at the end, and O(N) insertion and removal otherwise. Like
`std::vector`, the elements are initialized on insertion and destroyed on
removal.

This container is useful when one would otherwise use a `std::vector`, but:

- memory allocation is not desired _and_ the maximum capacity is bounded at
  compile-time,
- static allocation of objects with complex lifetimes is required (TODO: find
  example?),
- storing a bounded number of non-default constructible objects (`std::array`
  won't work here).

This paper addresses the utility of this container, explores the design space,
the existing trade-offs, and for a particular choice of trade-offs proposes an
API with standard-like wording and provides an implementation thereof. 

##### Bikeshedding

In this paper, the proposed container is named `stack_vector`. This name is a
bit of a lie since in the following the `stack_vector` elements will be on the
heap:

```c++
std::vector<stack_vector<float, 10>> where_is_my_mind;
```

Just mentally replace the name `stack_vector` with any of the following names:

- `static_vector` (Boost.Container's name for it)
- `fixed_capacity_vector`
- `fixed_vector`
- `non_allocating_vector`
- `internal_vector`
- `small_vector`

or a name of your preference.

# Motivation

## Split square with line in two dimensions

Lets consider splitting a two-dimensional square with a line into two polygons.
It is known a priori that the result of this operation are two polygons, which
might have either 3, 4, or 5 points. Using `stack_vector`:

```c++
// We can easily define a polygon with N points:
template<size_t N>
using polygon2d = stack_vector<point<2>, N>;

// And the signature of our split function:
pair<polygon2d<5>, polygon2d<5>> split(polygon<4> square, polygon<2> line);
```

Whether these polygons are to be stored on the stack or on the heap is
irrelevant. We can easily put them on the heap if needed:

```c++
std::vector<polygon2d<5>> heap_polygons;
for (auto&& p : zip(squares, surfaces)) {
  heap_polygons.push_back(split(p.first, p.second).first);
}
```

but if we just want to compute their volume or center of gravity we don't need
to put them on the heap at all. Using a `std::vector` to store the points of the
polygons would then inquire unnecessary memory allocations. Using a `std::array`
we wouldn't have information about the number of points in the polygon.

## Leaf neighbor search in an octree

A similar problem appears when one needs to find all the leaf neighbors of a
leaf node within an octree. The maximum number of neighbors that a node can have
is known for a 2:1 balanced octree, such that:

```c++
stack_vector<node_iterator, max_number_of_leaf_neighbors>
leaf_neighbors(octree const&, node_iterator);
```

# Previous work and implementation

This proposal is strongly inspired by
[`boost::container::static_vector` (1.59)][boost_static_vector]. A new
implementation is provided for standardization purposes here:
[http://github.com/gnzlbg/stack_vector][stack_vector]

# Design space

The requirements of `stack_vector` are:

  0. dynamically-resizable contiguous random-access sequence container with O(1)
     insert/erase at the end, and O(N) insert/erase otherwise, whose elements
     are stored internally within the object,

  1. it's API should be as similar to `vector<T,A>` API as possible,
  2. differences with `vector<T, A>`s API should be as explicit as possible, and
  3. it should be as efficient as possible.
  
Given these requirements, the following approaches cannot work:

### Failed approach 1: stack allocator for `std::vector`

Trying to reuse `std::vector` with a stack-only allocator (e.g. like a modified
Howard Hinnant's [`stack_alloc`][stack_alloc]) doesn't work because:

  1. The growth factor of `std::vector` is implementation dependent. Hence the
     storage required by the stack-allocator is implementation dependent, and
     one cannot write neither performant nor portable code.

  2. The size of the resulting type is one word too large (`std::vector` stores
     its capacity even though for a stack-only allocator the capacity is fixed
     at compile-time).

### Failed approach 2: stack allocator with heap-fallback for `std::vector`

Reusing Howard Hinnant's [`stack_alloc`][stack_alloc] one can implement a
`stack_vector` that works but has the following problems:

  1. Library-dependent performance due to the implementation-defined growth
     factor of `std::vector`.

  2. The resulting type is one word of memory too large.

On the other hand, `std::vector` with `stack_alloc` is perfect when a fallback
to the heap is desired, so `stack_vector` should not try to accommodate such an
use case.


### Failed approach 3: `std::vector::growth_factor()` + stack allocator

The main problem with the two failed approaches below has nothing to do with the
`Allocator` used to customize `std::vector`, but rather with the
implementation-defined growth factor, which cannot be worked around externally
in a reliable way since there is no way to query it portably at compile-time.

In this approach `std::vector`s API is extended to provide a `static constexpr
double growth_factor() noexcept(true)` member function that allows querying its
growth factor at compile-time.

Since the allocator does not know anything about the container, an adaptor must
be written. Since this adaptor will store a `std::vector` inside, it is
necessarily going to contain an unnecessary word of storage for its capacity.

This adaptor can store an arena that can hold enough elements for a desired
capacity, however, this arena will be unnecessary big, since in the worst case
the vector will trigger a reallocation after (Capacity - 1) elements, so the
arena must be able to store `GrowthFactor * (Capacity - 1)` elements to deal with
the worst case.

TODO: Can a std::vector implementation use a dynamic growth factor? (If that is
the case this approach cannot possibly ever work).


## A new sequence container

The problems with the three failed approaches above are intrinsic with
`std::vector` implementation. For this reason, a new container, `stack_vector<T,
Capacity>` is proposed in this section.

The main draw-backs of introducing a `stack_vector<T, Capacity>` type is:

  1. Code bloat: for each type `T`, and for each `Capacity`, a significant
     amount of code will be generated.

I don't have a good solution to this problem. Type-erasing the capacity would
hinder performance.

  - TODO: does Boost.Container static vector deal with this somehow?

The main advantages of introducing a new type are that we can get:

  1. portability,
  2. reliable performance across implementations, and
  3. a type that is as small as possible.

Now follows a list of the concrete requirements for an implementation (those
discussed later are marked as such):

  1. if `Capacity == 0`:
     - it should have zero-size so that it can be inherited from and the empty
       base optimization will trigger.
     - `data() == begin() == end() == unspecified unique value` (`nullptr` is
     intended) (like `std::array`).
     - `swap` is `noexcept(true)` (like `std::array`).
     - `front` and `back` are _disabled_.

  2. Exception safety (discussed in sub-section Exception Safety):
     - same as `std::vector`: throwing copy/move constructors retain only the
       basic guarantee.
 
  3. Constexpr (discussed in subsection Constexpr):
     - whole API is constexpr for types with a trivial destructor.

  4. Explicit instantiation support (discussed in subsection Explicit instantiation):
     - it is possible to explicitly instantiate `stack_vector` for types that
       are both `CopyConstructible` and `MoveConstructible`.

  5. Interoperability between `stack_vector`s of different capacities (discussed
     in subsection Interoperability between capacities):
     - comparison operators support, 
     - throwing constructors/assignment support,
     - throwing swap support.

### Exception Safety

Since `stack_vector<T, Capacity>` does not allocate memory, the only operations
that can actually fail are:

  1. `T` constructors, assignment, destructor, and swap,
  2. insertion exceeding the capacity (`push_back`, `insert`, `emplace`, ..),
  3. out-of-bounds access (`front/back/pop_back` when empty, unchecked
     random-access).

Like for `std::vector`, if (1) fails, the basic guarantee is provided and the
exception is rethrown.

Point (2) is controversal. The capacity of the vector is fixed at compile-time.
Is exceeding the capacity:

  - a) a logic error, and should be undefined behavior and asserted?
  - b) a logic error, and should throw a `logic_error` exception?
  - c) an out-of-memory error, since the vector cannot grow, and should be throw a
    `bad_alloc` exception?

Furthermore, since this class is very useful in embedded enviroments, should it
use exceptions at all?

Finally, since the capacity is known at compile-time, checked `push_back` incurs
O(N) checks, but the calle might know the exact number of elements that are
going to be inserted and might want to extract this check from the loop.

Addressing all these issues and concerns is arguably not easy. A good compromise
is provided by checked `std::vector`-like methods, and unchecked methods (that
can trigger an assertion on undefined behavior):

  - checked (throws `bad_alloc` like `std::vector`): `push_back`,
    `emplace_back`, `emplace`, `insert`, ...
  - unchecked (undefined behavior, assertion encouraged): `push_back_unchecked`,
  `emplace_back_unchecked`, `emplace_unchecked`, `insert_unchecked`,
  `resize_unchecked`

Non-conforming implementations could in practice replace exceptions with
assertions, calls to exit or terminate, or even a callback, but issues with C++
dialects are not for this proposal to solve and will not be discussed here
further.

Foot-note: an alternative to `_unchecked` would be to use a tag `unchecked_t`.

### Constexpr

The whole API of `stack_vector<T, Capacity>` is `constexpr` if `T` is trivial. 

### Explicit instantiation

It is technically possible to allow explicit instantiations of `stack_vector`
for types that are not both `MoveConstructible` and `CopyConstructible` but
doing so is so painful that it is left at first as a Quality of Implementation
issue. Implementations are encouraged to do so.

### Interoperability between capacities

Even though `stack_vector`s of different capacity have different types, it is
useful to be able to compare them and convert them. Since the capacities are
different, and the number of elements is not known till run-time, the
conversions and swap must be allowed to throw.

### Default initialization

Default initialization of elements is provided by:

```c++
static constexpr stack_vector default_initialized(size_t n);
constexpr void resize_default_initialized(size_type sz);
constexpr void resize_unchecked_default_initialized(size_type sz);
```

An alternative would be to provide a tag like `std::default_initialized_t` and
use it to overload the `resize` method and the sized constructor. The effect is
the same.

### Proposed API

```c++
template<typename T, std::size_t C>
struct stack_vector<T, Size> {

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
constexpr stack_vector() noexcept;
constexpr explicit stack_vector(size_type n);
constexpr stack_vector(size_type n, const value_type& value);
template<class InputIterator>
constexpr stack_vector(InputIterator first, InputIterator last);
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr stack_vector(stack_vector<value_type, M> const& other);
    noexcept(is_nothrow_copy_constructible<value_type>{} and C >= M);
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr stack_vector(stack_vector<value_type, M> && other)
    noexcept(is_nothrow_move_constructible<value_type>{} and C >= M);
constexpr stack_vector(stack_vector const& other);
  noexcept(is_nothrow_copy_constructible<value_type>{});
  constexpr stack_vector(stack_vector&& other)
  noexcept(is_nothrow_move_constructible<value_type>{});
constexpr stack_vector(initializer_list<value_type> il);

static constexpr stack_vector default_initialized(size_t n);

/* constexpr ~stack_vector(); */  // implicitly generated

constexpr stack_vector<value_type, C>& operator=(stack_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
constexpr stack_vector<value_type, C>& operator=(stack_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr stack_vector<value_type, C>& operator=(stack_vector<value_type, M>const& other)
    noexcept(is_nothrow_copy_assignable<value_type>{} and C >= M);
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr stack_vector<value_type, C>& operator=(stack_vector<value_type, M>&& other);
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

constexpr void resize_unchecked(size_type sz);
constexpr void resize_unchecked_default_initialized(size_type sz);
constexpr void resize_unchecked(size_type sz, const value_type& c);


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

constexpr void swap(stack_vector<value_type, C>&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));
};

// TODO: noexcept specification missing
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator==(const stack_vector<value_type, C0>& a, const stack_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator!=(const stack_vector<value_type, C0>& a, const stack_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<(const stack_vector<value_type, C0>& a, const stack_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<=(const stack_vector<value_type, C0>& a, const stack_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>(const stack_vector<value_type, C0>& a, const stack_vector<value_type, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>=(const stack_vector<value_type, C0>& a, const stack_vector<value_type, C1>& b);

```

#### API description

- the template parameters are called `T` and `C` for Type and Capacity,
  respectively.


##### Struct

- struct is chosen as for `std::array`, since `stack_vector` cannot support
  aggregate initialization maybe it would be better to make it a `class` instead
  to differentiate it from `std::array`

##### Types

The list of types is the same as `std::vector`, with the exception of
`allocator_type`.


## Storage / Layout guarantees

The memory layout of `stack_vector` offers the following guarantees:

- the elements are properly aligned ... wording
- the size parameter is as small as possible (e.g. for Capacity < 256 an
  unsigned char is enough).


## Construction/Assignment/Destruction






### Construction

```c++
/// Constructs an empty stack_vector.
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
constexpr stack_vector() noexcept;
```

```c++
/// Constructs a stack_vector containing \p n default-inserted elements.
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
constexpr explicit stack_vector(size_type n);
```

```c++
/// Constructs a stack_vector containing \p n copies of \p value.
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
constexpr stack_vector(size_type n, const value_type& value);
```

```c++ 
/// Constructs a stack_vector equal to the range [\p first, \p last).
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
constexpr stack_vector(InputIterator first, InputIterator last);
```

```c++
/// Constructs a stack_vector whose elements are copied from \p other.
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
constexpr stack_vector(stack_vector const&);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

```c++
/// Constructs a stack_vector whose elements are copied from \p other.
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
  constexpr stack_vector(stack_vector<value_type, M> const& other);
    noexcept(is_nothrow_copy_constructible<value_type>{} and C >= M);
```

```c++
/// Constructs a stack_vector whose elements are moved from \p other.
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
constexpr stack_vector(stack_vector&&)
  noexcept(is_nothrow_move_constructible<value_type>{});
```

```c++
/// Constructs a stack_vector whose elements are moved from \p other.
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
  constexpr stack_vector(stack_vector<value_type, M> &&)
    noexcept(is_nothrow_move_constructible<value_type>{} and C >= M);
```

```c++
/// Constructs a stack_vector whose elements are copied from \p il.
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
constexpr stack_vector(initializer_list<value_type> il);
```

```c++
/// Returns a stack_vector containing \p n default-initialied elements.
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
static constexpr stack_vector default_initialized(size_t n);
```

### Assignment



### Destruction

The destructor should be implicitly generated and it should be constexpr
if `value_type` models `TrivialType`. 

```c++
constexpr ~stack_vector(); */  // implicitly generated
```

### Static methods

- a `static constexpr stack_vector default_initialized(size_t n)` method is
  provided which returns a vector of `n` default initialized elements. An
  alternative would be to provide a tag for this.

## Iterators

- all iteration methods are constexpr, except
  - the reverse iterator methods since that would require
    `std::reverse_iterator` to be constexpr-friendly, and that is not being
    proposed here.

- the time complexity of the iterator functions is O(1)
- the space complexity of the iterator functions is O(1)
- the iterator functions are noexcept

## Size

- the time complexity of the size functions is O(1)
- the space complexity of the size functions is O(1)
- the size functions are noexcept


## Element access

- should operator[] be noexcept(true) ? TODO: Ask Eric if he thinks this might
conflict with proxy references in the future

## Swap

TODO: the paragraph below is wrong, we should support swapping vectors of different capacity
TODO: swap is, as for `std::array`, O(N), which might be controversial.

For simplicity only `stack_vector`s of the same type can be swapped. Allowing
`stack_vector`s of different types to be swapped is possible but that swap
operation can then fail. For example consider:

```c++
stack_vector<int, 5> a(3);
stack_vector<int, 5> b(4);
stack_vector<int, 3> c(2);
a.swap(c); // works
b.swap(c); // always fails
```

## TODO: Iterator invalidation

### List of functions that can potentially invalidate iterators and how


## Other

- `swap`
- `fill`


# Acknowledgements

The authors of Boost.Container's `boost::container::static_vector` (Adam
Wulkiewicz, Andrew Hundt, and Ion Gaztanaga). The authors of libc++ (Howard
Hinnant + ?? TODO: check) `<algorithm>` and `<vector>` and in particular their
`<vector>` test suite which I extensively used in the prototype implementation.
Anybody else who finds errors, and helps improve this proposal and/or the
implementation.

# Appendix A: Standad wording

Not part of this revision.

# Appendix B: Rough edges in C++14


There are some rough edges in C++14 that complicate the implementation of this
proposal. These are listed here "for completeness". This proposal does not
propose anything about them.

  1. `std::reverse_iterator<It>` is never constexpr, even when `It` is a
      raw-pointer (which is constexpr).

  2. `std::array` is not fully `constexpr` (`data`, `begin/end`, `non-const
     operator[]`, `swap`...) and as a consequence `stack_vector`s implementation
     uses a C Array for trivially destructible types. If the types are also
     trivially constructible, then `std::array` would have been a better fit.

  3. the `<algorithms>` are not `constexpr` even though it is trivial to make
     all of them but 3 (the allocating algorithms) `constexpr`. The API of the
     allocating algorithms is broken. Fixing it would make it trivial to make
     them `constexpr` as well. The implementation of `stack_vector` needs to
     reimplement some algorithms because of this.

  4. the generic `begin`, `end`, and `swap` as well as their overloads for
     C-arrays are not `constexpr`.

  5. `std::aligned_storage` is impossible to use in `constexpr` code because one needs to:
     - `reinterpret_cast` to the elements pointer type,
     - use placement new, and
     - call explicit destructors. None of these three things can be used in
     `constexpr` code, and the implementation of `stack_vector` and
     `std::variant` suffers from this.

  6. `std::initializer_list`:
     - doesn't have its size as part of its type,
     - its elements cannot be moved,
     - cannot be converted to a C-array or a `std::array` easily:
     ```c++
     std::initializer_list<int> il{1, 2, 3};
     const int ca[3] = il;  
     ```

     Working around these in the implementation of `stack_vector` proved to be
     impossible. As a consequence `stack_vector` doesn't offer an
     `initializer_list` constructor but uses a variadic constructor instead.
     As a consequence `stack_vector<T, 1> v = {1};` is broken.
 
  7. `<type_traits>` offers the "dangerous" `decay_t` but offers no `uncvref_t`
     (which is the type trait most used in `stack_vector`).

  8.  `stack_vector` doesn't provide a `reserve` member function. It is
     explicitly deleted in its API to convey that this is not an accident, but
     there is no way to actually provide more information to the users (e.g. `=
     delete("message");`).

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
     Working around this issue is _very very painful_.


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
