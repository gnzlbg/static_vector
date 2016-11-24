# embedded_vector 

> A dynamically-resizable vector with fixed capacity and embedded storage (revision -1)

**Document number**: none.

**Date**: none.

**Author**: Gonzalo Brito Gadeschi.

# Introduction

This paper proposes a dynamically-resizable `vector` with fixed capacity and
contiguous embedded storage. That is, the elements of the vector are stored within
the vector object itself. It is based on 
[`boost::container::static_vector<T, Capacity>`][boost_static_vector].

Its API is almost a 1:1 map of `std::vector<T, A>`'s API. It is a contiguous sequence
random-access container (with contiguous storage), `O(1)` insertion and removal of
elements at the end, and `O(size())` insertion and removal otherwise. Like
`std::vector`, the elements are initialized on insertion and destroyed on
removal. It models `ContiguousContainer` and its iterators model
the `ContiguousIterator` concept.

# Motivation

This container is useful when:

- memory allocation is not possible, e.g., embedded environments without a free store, 
  where only a stack and the static memory segment are available,
- memory allocation imposes an unacceptable performance penalty, e.g., with respect to latency, 
- allocation of objects with complex lifetimes in the _static_-memory segment is required,
- non-default constructible objects must be stored such that `std::array` is not an option,
- a dynamic resizable array is required within `constexpr` functions, 
- full control over the storage location of the vector elements is required.

# Design

In this section Frequently Asked Questions are answered, an overview of existing implementations is given, and the proposed design is provided.

## FAQ

### Can we reuse `std::vector` with a custom allocator? 

Yes, in practice we can, but neither in a portable way, nor in a way that results
in a zero-cost abstraction, mainly due to the following limitations in the `Allocator`
interface. 

1. The `Allocator::allocate(N)` member function either succeeds, returning a
pointer to the storage for `N` elements, or it fails. The current interface 
allows returning storage for `M` elements where `M > N`, but it doesn't provide
a way to communicate to the container that this happened, so the container cannot
make use of it.

2. The growth mechanism of `std::vector` is implementation defined. With the current
`Allocator` interface, a stateful allocator with embedded storage needs to allocate
memory for `growth_factor() * (Capacity - 1) * sizeof(T)`, since the vector might try
to growh when inserting the last element. The current `Allocator` 
interface does not provide a way to communicate the growing behavior to the `Allocator`,
so an `Allocator` with embedded storage needs to either be coupled to an implementation 
(resulting in code with different behavior on different platforms), or needs to make a 
non-portable over-estimation of its capacity. Trying to solve this problem leads to
other problems, like adding support for `realloc`.

3. An `Allocator` with embedded storage for `Capacity * sizeof(T)` elements makes
storing data members for the `data` pointer and the `capacity` unnecessary in 
`std::vector` implementation. In the current `Allocator` interface the is no mechanism
to communicate `std::vector` that storing these data members is unnecessary. 

4. The `Allocator` interface does not specify whether containers should propagate `noexcept`ness
off `Allocator` member functions into their interfaces. An `Allocator` with embedded storage for
`Capacity` elements never throws on allocating memory. Whether trying to allocate more memory
should result in a length / out-of-bounds / bad-alloc / logic error, or precondition violation
is discussed below, but the current `Allocator` interface has no way to communicate this.

Improving the `Allocator` interface to solve issue 1. is enough to make an implementation
based on `std::vector` with an user-defined `Allocator` be portable (this should probably be
pursuded in a different proposal) since the `std::vector`constructors could 
`Allocator::allocate(0)` on construction and directly get memory for`Capacity` elements. 
However, in order to make this a zero-cost abstraction one would need
to solve issues 3 and 4 as well. Whether solving these issues is worth pursuing or not is
still an open problem.

The author of this proposal does not think that it will be possible to solve issues 3 and 4 in
the near future. Still, nothing prevents library implementors to reuse their `std::vector` 
implementations when implementing `embedded_vector` if they are able to do so. 

### Can we reuse `small_vector`?

Yes, we can, but no, it does not result in a zero-cost abstraction.

The paper
[PR0274: Clump – A Vector-like Contiguous Sequence Container with Embedded Storage][clump]
proposes a new type, `small_vector<T, N, Allocator>`, which is essentially a
`std::vector<T, Allocator>` that performs a Small Vector Optimization for up to
`N` elements, and then, depending on the `Allocator`, might fall-back to heap allocations,
or do something else (like `throw`, `assert`, `terminate`, introduce undefined behavior...). 

This small vector type is part of [Boost][boostsmallvector], [LLVM][llvmsmallvector], 
[EASTL][eastl], and [Folly][folly]. Most of these libraries special case `small_vector`
for the case in which _only_ embedded storage is desired. This result in a type with
slightly different but noticeable semantics (in the spirit of `vector<bool>`). The
only library that offers it as a completely different type is Boost.Container.

The main difference between `small_vector` and `embedded_vector` is:

- `small_vector` uses embedded storage up-to `N` elements, and then falls back to
  `Allocator`, while `embedded_vector` _only_ provides embedded storage.

As a consequence, for the use cases of `embedded_vector`:

- `small_vector` cannot provide the same exception safety guarantees than
  `embedded_vector` because it sometimes allocates memory, which `embedded_vector`
  never does.

- `small_vector` cannot provide the same reliability than `embedded_vector`,
  because the algorithmic complexity of `small_vector` operations like move
  construction/assignment depends on whether the elements are allocated on the
  heap (swap pointer and size) or embedded within the vector object itself (must 
  always copy the elements).

- `small_vector` cannot be as efficient as `embedded_vector`. It must discriminate 
  between the different storage location of its elements (embedded within the vector
  object or owned by the   allocator). A part of its methods must, at run-time, branch 
  to a different code-path depending on the active storage scheme.

The only way to fix `small_vector` would be to special case it for
`Allocator::max_size() == 0` (which isn't constexpr), and to provide an API that
has different complexity and exception-safety guarantees than `small_vector`'s
with `Allocator::max_size() > 0`.

That is, to make `small_vector` competitive in those situations in which 
`embedded_vector` is required is to special case `small_vector` for a particular
allocator type and in that case provide an `embedded_vector` implementation (with
slightly different semantics). 

### Should we special case `small_vector` for embedded-storage-only like EASTL and Folly do?

The types `embedded_vector` and `small_vector` have different algorithmic
complexity and exception-safety guarantees. They solve different problems and
should be different types. 

## Existing practice

There are at least 3 widely used implementations of `embedded_vector`.

This proposal is strongly inspired by Boost.Container, which offers
[`boost::container::static_vector<T, Capacity>` (1.59)][boost_static_vector],
and, as a different type, also offers `boost::container::small_vector<T, N, Allocator>`
as well.

The other two libraries that implement `embedded_vector` are [Folly][folly] and
[EASTL][eastl]. Both of these libraries implement it as a special case of
`small_vector` (which in both of these libraries has 4 template parameters).

EASTL `small_vector` is called `fixed_vector<T, N,
hasAllocator, OverflowAllocator>` and uses a boolean template parameter to
indicate if the only storage mode available is embedded storage. The
[design documents of EASTL][eastldesign] seem to predate this special casing
since they actually argue against it:

>- special casing complicates the implementation of `small_vector`,
>- (without proof) the resulting code size increase would be larger than the "4
>  bytes" that can be saved in storage per vector for the special case.

The current implementation does, however, special case `small_vector` for embedded
storage. No rationale for this decision is given in the design documentation.

Folly implements `small_vector<T, N, NoHeap, size_type>`, where the tag type
`NoHeap` is used to switch off heap-fall back. Folly allows customizing the 
`size_type` of `small_vector`. No rationale for this design decision is available
in the documentation. A different `size_type` can potentially be used to reduce 
`embedded_vector`s memory requirements.

## Proposed design and rationale

The current design follows that of `boost::container::static_vector<T,
Capacity>` closely.

It introduces a new type `std::experimental::embedded_vector<T, Capacity>` in the
`<experimental/embedded_vector>` header. It is a pure library extension to the C++ 
standard library.

> `embedded_vector` is a dynamically-resizable contiguous random-access sequence
> container with `O(1)` insert/erase at the end, and `O(size())` insert/erase otherwise.
> Its elements are stored within the vector object itself. It models `ContiguousContainer`
> and its iterators model the `ContiguousIterator` concept.

A prototype implementation of this proposal is provided for standardization
purposes: [`http://github.com/gnzlbg/embedded_vector`][embedded_vector].

The main drawback of introducing a new type is, as the
[design document of the EASTL][eastldesign] points out, increased code size.
Since this container type is opt-in, only those users that need it will pay
this cost. Common techniques to reduce code size where explored in the 
prototype implementation (e.g. implementing `Capacity`/`value_type` agnostic 
functionality in a base class) without success, but implementations are 
encouraged to consider code-size as a quality of implementation issue.

### Preconditions

This container requires that `T` models `Destructible`. If `T`'s destructor
throws the behavior is undefined.

### Storage/Memory Layout

Specializations of `embedded_vector<T, Capacity>` model the `ContiguousContainer` concept.

The elements of the vector are properly aligned to an `alignof(T)` memory address.

The `embedded_vector<T, Capacity>::size_type` is the smallest unsigned integer type
that can represent `Capacity`.

If the container is not empty, the member function `data()` returns a pointer such that
`[data(), data() + size())` is a valid range and `data() == addressof(front()) == addressof(*begin())`.
Otherwise, the result of `data()` is unspecified.
  
**Note**: `embedded_vector<T, Capacity>` cannot be an aggregate since it provides
user-defined constructors.

#### Zero-sized

It is required that `is_empty<embedded_vector<T, 0>>::value == true` and
that `swap(embedded_vector<T, 0>&, embedded_vector<T, 0>&)` is `noexcept(true)`.

### Move semantics

The move semantics of `embedded_vector<T, Capacity>` are equal to those of 
`std::array<T, Size>`. That is, after

```c++
embedded_vector a(10);
embedded_vector b(std::move(a));
```

the elements of `a` have been moved element-wise into `b`, the elements of `a`
are left in an initialized but unspecified state (have been moved from state), 
the size of `a` is not altered, and `a.size() == b.size()`.

Note that this behavior differs from `std::vector<T, Allocator>`, in particular
for the similar case in which `std::propagate_on_container_move_assignment<Allocator>{}`
is `false`. In this situation the state of `std::vector` is initialized but unspecified,
which prevents users from portably relying on `size() == 0` or `size() == N`, and raises
questions like "Should users call `clear` after moving from a `std::vector`?" (whose 
answer is _yes_, in particular if `propagate_on_container_move_assignment<Allocator>` 
is `false`).

### Constexpr-support

The whole API of `embedded_vector<T, Capacity>` is `constexpr` if `is_trivial<T>`
is true.

Implementations can achieve this by using a C array for `embedded_vector`'s storage
without altering the guarantees of `embedded_vector`'s methods in an observable way.

For example, `embedded_vector(N)` constructor guarantees "exactly `N` calls to 
`T`'s default constructor". Strictly speaking, `embedded_vector`'s constructor
for trivial types will construct a C array of `Capacity` length. However, because
`is_trivial<T>` is true, the number of constructor or destructor calls is not
observable.

This introduces an additional implementation cost which the author believes is
worth it because similarly to `std::array`, `embedded_vector`s of trivial types 
are incredibly common. 

Note: this type of `constexpr` support does not require any core language changes.
This design could, however, be both simplified and extended if 1) placement `new`,
2) explicit destructor calls, and 3) `reinterpret_cast`, would be `constexpr`. This
paper does not propose any of these changes.

### Explicit instantiatiability

The class `embedded_vector<T, Capacity>` can be explicitly instantiated for
all combinations of `T` and `Capacity` if `T` satisfied the container preconditions 
(i.e. `T` models `Destructible<T>`).

### Exception Safety

#### What could possibly go wrong?

The only operations that can actually fail within `embedded_vector<T, Capacity>` are:

  1. `T` special member functions and swap can only fail due
     to throwing constructors/assignment/destructors/swap of `T`. 

  2. Mutating operations exceeding the capacity (`push_back`, `insert`, `emplace`, 
     `pop_back` when `empty()`, `embedded_vector(T, size)`, `embedded_vector(begin, end)`...).

  2. Out-of-bounds unchecked access:
     2.1 `front/back/pop_back` when empty, operator[] (unchecked random-access). 
     2.2  `at` (checked random-access) which can throw `out_of_range` exception.

#### Rationale

Three points influence the design of `embedded_vector` with respect to its exception-safety guarantees:

1. Making it a zero-cost abstraction.
2. Making it safe to use.
3. Making it easy to learn and use. 

The best way to make `embedded_vector` easy to learn is to make it as similar to `std::vector` 
as possible. However,`std::vector` allocates memory using an `Allocator`, whose allocation 
functions can throw, e.g., a `std::bad_alloc` exception, e.g., on Out Of Memory. 

However, `embedded_vector` never allocates memory since its `Capacity` is fixed at compile-time.

The main question then becomes, what should `embedded_vector` do when its `Capacity` is exceeded?

Two main choices were identified:

1. Make it throw an exception. 
2. Make not exceeding the `Capacity` of an `embedded_vector` a precondition on its mutating method (and thus exceeding it undefined-behavior).

While throwing an exception makes the interface more similar to that of `std::vector` and safer to use, it does introduces a performance cost since it means that all the mutating methods must check for this condition. It also raises the question: which exception? It cannot be `std::bad_alloc`, because nothing is being allocated.
It should probably be either `std::out_of_bounds` or `std::logic_error`, but if exceeding the capacity is a logic error, why don't we make it a precondition instead?

Making exceeding the capacity a precondition has some advantages:

- It llows implementations to trivially provide a run-time diagnostic on debug builds by, e.g., means of an assertion. 

- It allows the methods to be conditionally marked `noexcept(true)` when `T` is `std::is_nothrow_default_constructible/copy_assignable>...`

- It makes `embedded_vector` a zero-cost abstraction by allowing the user to avoid unnecessary checks (e.g. hoisting checks out of a loop).

And this advantages come at the expense of safety. It is possible to have both by making the methods checked by default, but offering `unchecked_xxx` alternatives that omit the checks which increases the API surface.

Given this design space, this proposal opts for making not exceeding the `Capacity` of an `embedded_vector` a precondition. It still allows some safety by allowing implementations to make the operations checked in the debug builds of their standard libraries, while providing very strong exception safety guarantees (and conditional `noexcept(true)`), which makes `embedded_vector` a true zero-cost abstraction.

The final question to be answered is if we should mark the mutating methods to be conditionally `noexcept(true)` or not when it is safe to do so. The current proposal does so since it is easier to remove `noexcept(...)` than to add it, and since this should allow the compiler to generate better code, which is relevant for some fields in which `embedded_vector` is very useful, like in embedded systems programming. 

#### Precondition on `T` modelling `Destructible`

If `T` models `Destructible` (that is, if `T` destructor never throws), 
`embedded_vector<T, Capacity>` provides at least the basic-exception guarantee.
If `T` does not model `Destructible`, the behavior of `embedded_vector` is undefined. 
Implementations are encouraged to rely on `T` modeling `Destructible` even
if `T`'s destructor is `noexcept(false)`. 

#### Exception-safety guarantees of special member functions and swap

If `T`'s special member functions and/or swap are `noexcept(true)`, so are the respective
special member functions and/or swap operations of `embedded_vector` which then provide the
strong-exception guarantee. They provide the basic-exception guarantee otherwise. 

#### Exception-safety guarantees of algorithms that perform insertions

The capacity of `embedded_vector` (a fixed-capacity vector) is statically known at 
compile-time, that is, exceeding it is a logic error.

As a consequence, inserting elements beyond the `Capacity` of an `embedded_vector` results
in _undefined behavior_. While providing a run-time diagnostic in debug builds (e.g. via an 
`assertion`) is encouraged, this is a Quality of Implementation issue.

The algorithms that perform insertions are the constructors `embedded_vector(T, size)` and 
`embedded_vector(begin, end)`, and the member functions `push_back`, `emplace_back`, `insert`, 
and `resize`.

These algorithms provide strong-exception safety guarantee, and if `T`'s special member functions or
`swap` can throw are `noexcept(false)`, and `noexcept(true)` otherwise.

#### Exception-safety guarantees of unchecked access

Out-of-bounds unchecked access (`front>back>pop_back` when empty, `operator[]`) is undefined behavior
and a run-time diagnostic is encouraged but left as a Quality of Implementation issue.

These functions provide the strong-exception safety guarantee and are `noexcept(true)`.

#### Exception-safety guarantees of checked access

Checked access via `at` provides the strong-exception safety guarantee and it throws the `std::out_of_range`
exception on out-of-bounds. It is `noexcept(false)`.

### Iterators 

The iterators of `embedded_vector<T, Capacity>` model the `ContiguousIterator` concept.

#### Iterator invalidation

The iterator invalidation rules are different than those for `std::vector`,
since:

- moving an `embedded_vector` invalidates all iterators,
- swapping two `embedded_vector`s invalidates all iterators, and 
- inserting elements into an `embedded_vector` never invalidates iterators.

The following functions can potentially invalidate the iterators of `embedded_vector`s: 
`resize(n)`, `resize(n, v)`, `pop_back`, `erase`, and `swap`.

The following functions from the "possible future extensions" can potentially
invalidate the iterators of `embedded_vector`s: `resize_default_initialized(n)`,
`resize_unchecked(n)`, `resize_unchecked(n, v)`, and
`resize_default_initialized_unchecked(n)`.

### Naming

Following names have been considered: 

- `embedded_vector<T, Capacity>`: since the elements are "embedded" within the vector object itself. 
   Sadly, the name `embedded` is overloaded, e.g., embedded systems, and while in this domain this container
   is very useful, it is not the only domain in which it is useful. 

- `fixed_capacity_vector`: a vector with fixed capacity, long name, but clearly indicates what this is.
- `static_vector` (Boost.Container): due to "static" / compile-time allocation of the elements. The term 
   `static` is, however, overloaded in C++ (e.g. `static` memory?).
- `inline_vector`: the elements are stored "inline" within the vector object itself. The term `inline` is,
   however, already overloaded in C++ (e.g. `inline` functions => ODR, inlining, `inline` variables).
- `stack_vector`: to denote that the elements can be stored on the stack, which is confusing since the
  elements can be on the stack, the heap, or the static memory segment. It also has a resemblance with 
  `std::stack`.

### Impact on the standard

`embedded_vector<T, Capacity>` is a pure library extension to the C++ standard library and can be implemented by any C++11 compiler in a separate header `<embedded_vector>`. 

### Future extensions 

#### Interoperability of embedded vectors with different capacities

A possible backwards-compatible future extension that is not pursued further
in this paper is providing interoperability of `embedded_vector`s of different
capacities (e.g. copy construction/assignment/comparison>/swap). Currently,
other standard containers like `std::array` do not pursue this, and it would
complicate the exception-safety specification of, e.g., `swap`, significantly. 

#### Default initialization

The size-modifying operations of the `embedded_vector` that do not require a value
also have the following analogous counterparts that perform default
initialization instead of value initialization:

```c++
struct default_initialized_t {};
inline constexpr default_initialized_t default_initialized{};

template <typename Value, std::size_t Capacity>
struct embedded_vector {
    // ...
    constexpr embedded_vector(default_initialized_t, size_type n);
    constexpr void resize(default_initialized_t, size_type sz);
    constexpr void resize_unchecked(default_initialized_t, size_type sz);
};
```

#### Unchecked mutating operations

In the current proposal exceeding the capacity on the mutating operations is considered a logic-error and results in undefined behavior, which allows implementations to cheaply provide an assertion in debug builds without introducing checks in release builds. If a future revision of this paper changes this to an alternative solution that has an associated cost for checking the invariant, it might be worth it to consider adding support to unchecked mutating operations like `resize_unchecked`,`push_back_unchecked`, `assign_unchecked`, `emplace`, and `insert`.


#### `with_size` / `with_capacity` constructors

Consider:

```c++
using vec_t = embedded_vector<std::size_t, N>;
vec_t v0(2);  // two-elements: 0, 0
vec_t v1{2};  // one-element: 2
vec_t v2(2, 1);  // two-elements: 1, 1
vec_t v3{2, 1};  // two-elements: 2, 1
```

A way to avoid this problem introduced by initializer list and braced initialization, present in the interface of `embedded_vector` and `std::vector`, would be to use a tagged-constructor of the form `embedded_vector(with_size_t, std::size_t N, T const& t = T())` to indicate that constructing a vector with `N` elements is inteded. For `std::vector`,
a similar constructor using a `with_capacity_t` and maybe combinations thereof might make sense. This proposal 
does not propose any of these, but this is a problem that should definetely be solved in STL2, and if it solved,
it should be solved for `embedded_vector` as well. 

# Technical specification

This enhancement is a pure header-only addition to the C++ standard library as the `<experimental/embedded_vector>` header. 

```c++
template<typename T, std::size_t C /* Capacity */>
struct embedded_vector {

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
constexpr embedded_vector() noexcept;
constexpr explicit embedded_vector(size_type n);
constexpr embedded_vector(size_type n, const value_type& value);
template<class InputIterator>
constexpr embedded_vector(InputIterator first, InputIterator last);
constexpr embedded_vector(embedded_vector const& other)
  noexcept(is_nothrow_copy_constructible<value_type>{});
constexpr embedded_vector(embedded_vector && other)
  noexcept(is_nothrow_move_constructible<value_type>{});
constexpr embedded_vector(initializer_list<value_type> il);

/* constexpr ~embedded_vector(); */ // implicitly generated

constexpr embedded_vector& operator=(embedded_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
constexpr embedded_vector& operator=(embedded_vector && other);
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

constexpr void swap(embedded_vector&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));

friend constexpr bool operator==(const embedded_vector& a, const embedded_vector& b);
friend constexpr bool operator!=(const embedded_vector& a, const embedded_vector& b);
friend constexpr bool operator<(const embedded_vector& a, const embedded_vector& b);
friend constexpr bool operator<=(const embedded_vector& a, const embedded_vector& b);
friend constexpr bool operator>(const embedded_vector& a, const embedded_vector& b);
friend constexpr bool operator>=(const embedded_vector& a, const embedded_vector& b);
};

template <typename T, std::size_t Capacity>
constexpr void swap(embedded_vector<T, Capacity>&, embedded_vector<T, Capacity>&)
  noexcept(is_nothrow_swappable<T>{});
```

## Construction

```c++
constexpr embedded_vector() noexcept;
```

> Constructs an empty `embedded_vector`.
>
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_:
>   - time: O(1),
>   - space: O(1).
>
> - _Exception safety_: never throws.
>
> - _Constexpr_: always.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: none.
> 
> - _Post-condition_: `size() == 0`.


```c++
constexpr explicit embedded_vector(size_type n);
```

> Constructs an `embedded_vector` containing `n` value-initialized elements.
>
> - _Requirements_: `value_type` shall be `DefaultInsertable` into `*this`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `n` calls to `value_type`'s default constructor,
>   - space: O(1).
>
> - _Exception safety_:
>   - strong guarantee: all constructed elements shall be destroyed on failure,
>   - re-throws if `value_type`'s default constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly `n` calls to `value_type`s default constructor.
>
> - _Pre-condition_: `n <= Capacity`.
> - _Post-condition_: `size() == n`.


```c++
constexpr embedded_vector(size_type n, const value_type& value);
```

> Constructs an `embedded_vector` containing `n` copies of `value`.
>
> - _Requirements_: `value_type` shall be `CopyInsertable` into `*this`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `n` calls to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee: all constructed elements shall be destroyed on failure,
>   - re-throws if `value_type`'s copy constructor throws,
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly `n` calls to `value_type`s copy constructor.
>
> - _Pre-condition_: `n <= Capacity`.
> - _Post-condition_: `size() == n`.


```c++ 
template<class InputIterator>
constexpr embedded_vector(InputIterator first, InputIterator last);
```

> Constructs an `embedded_vector` containing a copy of the elements in the range `[first, last)`.
>
> - _Requirements_: `value_type` shall be either:
> - `CopyInsertable` into `*this` _if_ the reference type of `InputIterator`
>    is an lvalue reference, or
> - `MoveInsertable` into `*this` _if_ the reference type of `InputIterator`
>    is an rvalue reference.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `last - first` calls to `value_type`'s copy or move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee: all constructed elements shall be destroyed on failure,
>   - re-throws if `value_type`'s copy or move constructors throws,
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly `last - first` calls to `value_type`s copy or move constructor.
>
> - _Pre-condition_: `last - first <= Capacity`.
> - _Post-condition_: `size() == last - first`.


```c++
constexpr embedded_vector(embedded_vector const& other);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

> Constructs a `embedded_vector` whose elements are copied from `other`.
>
> - _Requirements_: `value_type` shall be `CopyInsertable` into `*this`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `other.size()` calls to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee: all constructed elements shall be destroyed on failure,
>   - re-throws if `value_type`'s copy constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly \p `other.size()` calls to `value_type`s copy constructor.
>
> - _Pre-condition_: none.
> - _Post-condition_: `size() == other.size()`.


```c++
constexpr embedded_vector(embedded_vector&& other)
  noexcept(is_nothrow_move_constructible<value_type>{});
```

> Constructs an `embedded_vector` whose elements are moved from `other`.
>
> - _Requirements_: `value_type` shall be `MoveInsertable` into `*this`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `other.size()` calls to `value_type`'s move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee if `std::nothrow_move_assignable<T>` is true, basic
>     guarantee otherwise: all moved elements shall be destroyed on failure.
>   - re-throws if `value_type`'s move constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly `other.size()` calls to `value_type`s move constructor.
>
> - _Pre-condition_: none.
> - _Post-condition_: `size() == other.size()`.
> - _Invariant_: `other.size()` does not change.


```c++
/// Equivalent to `embedded_vector(il.begin(), il.end())`.
constexpr embedded_vector(initializer_list<value_type> il);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

## Assignment

Move assignment operations invalidate iterators.

```c++
constexpr embedded_vector& operator=(embedded_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
```

```c++
constexpr embedded_vector& operator=(embedded_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
```

```c++
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr embedded_vector& operator=(embedded_vector<value_type, M>const& other)
    noexcept(is_nothrow_copy_assignable<value_type>{} and C >= M);
```

```c++
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr embedded_vector& operator=(embedded_vector<value_type, M>&& other);
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

## Destruction

The destructor should be implicitly generated and it should be constexpr
if `is_trivial<value_type>`.

```c++
/* constexpr ~embedded_vector(); */ // implicitly generated
```

## Iterators

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

- _Requirements_: none.
- _Enabled_: always.
- _Complexity_: constant time and space.
- _Exception safety_: never throw.
- _Constexpr_: always.
- _Effects_: none.

The `iterator` and `const_iterator` types are implementation defined and model
the `ContiguousIterator` concept. 

There are also some guarantees between the results of `data` and the iterator
functions that are explained in the section "Element / data access" below.

## Size / capacity


```c++
constexpr size_type size()     const noexcept;
```

> - _Returns_: the number of elements that the vector currently holds.
> 
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_: constant time and space.
>
> - _Exception safety_: never throws.
>
> - _Constexpr_: always.
>
> - _Effects_: none.


```c++
static constexpr size_type capacity() noexcept;
```

> - _Returns_: the total number of elements that the vector can hold.
> 
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_: constant time and space.
>
> - _Exception safety_: never throws.
>
> - _Constexpr_: always.
>
> - _Effects_: none.
>
> - _Note_:  
>   - if `capacity() == 0`, then `sizeof(embedded_vector) == 0`,
>   - if `sizeof(T) == 0 and capacity() > 0`, then `sizeof(embedded_vector) == sizeof(unsigned char)`.


```c++
static constexpr size_type max_size() noexcept;
```

> - _Returns_: the total number of elements that the vector can hold.
> 
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_: constant time and space.
>
> - _Exception safety_: never throws.
>
> - _Constexpr_: always.
>
> - _Effects_: none.


```c++
constexpr bool empty() const noexcept;
```

> - _Returns_: `true` if the total number of elements is zero, `false` otherwise.
> 
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_: constant time and space.
>
> - _Exception safety_: never throws.
>
> - _Constexpr_: always.
>
> - _Effects_: none.


For the checked resize functions:

```c++
constexpr void resize(size_type new_size);
constexpr void resize(size_type new_size, const value_type& c);
```
the following holds:

- _Requirements_: `T` models `DefaultInsertable`/`CopyInsertable`.
- _Enabled_: if requirements satisfied.
- _Complexity_: O(size()) time, O(1) space.
- _Exception safety_:
   - basic guarantee: all constructed elements shall be destroyed on failure,
   - rethrows if `value_type`'s default or copy constructors throws,
   - throws `bad_alloc` if `new_size > capacity()`.
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_:
  - if `new_size > size` exactly `new_size - size` elements default>copy constructed.
  - if `new_size < size`:
      - exactly `size - new_size` elements destroyed.
      - all iterators pointing to elements with `position > new_size` are invalidated.

## Element / data access

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

- _Requirements_: none.
- _Enabled_: always.
- _Complexity_: O(1) in time and space.
- _Exception safety_: never throws.
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_: none.
- Pre-conditions: `size() > n` for `operator[]`, `size() > 0` for `front` and `back`.


For the checked element access functions:

```c++
constexpr const_reference at(size_type n) const;
constexpr reference       at(size_type n);
```

the following holds:

- _Requirements_: none.
- _Enabled_: always.
- _Complexity_: O(1) in time and space.
- _Exception safety_:
  - throws `out_of_range` if `n >= size()`.
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_: none.
- _Pre-conditions_: none.

For the data access:

```c++
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;
```

the following holds: 

- _Requirements_: none.
- _Enabled_: always.
- _Complexity_: O(1) in time and space.
- _Exception safety_: never throws.
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_: none.
- _Pre-conditions_: none.
- _Returns_: if the container is empty the return value is unspecified. If the container 
  is not empty, `[data(), data() + size())` is a valid range, and `data() == addressof(front())`.

## Modifiers

For the modifiers:

```c++
template<class... Args>
constexpr void emplace_back(Args&&... args);
```

> Construct a new element at the end of the vector in place using `args...`.
>
> - _Requirements_: `Constructible<value_type, Args...>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(1), exactly one call to `T`'s constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee: no side-effects if `value_type`'s constructor throws. 
>   - re-throws if `value_type`'s constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `T`'s constructor, the `size()` of the vector 
> is incremented by one.
>
> - _Pre-condition_: `size() < Capacity`.
> - _Post-condition_: `size() == size_before + 1`.


```c++
constexpr void push_back(const value_type& x);
```

> Copy construct an element at the end of the vector from `x`.
>
> - _Requirements_: `CopyConstructible<value_type>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(1), exactly one call to `T`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee: no side-effects if `value_type`'s copy constructor throws. 
>   - re-throws if `value_type`'s constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `T`'s copy constructor, the `size()` of the vector is 
> incremented by one.
>
> - _Pre-condition_: `size() < Capacity`.
> - _Post-condition_: `size() == size_before + 1`.


```c++
constexpr void push_back(value_type&& x);
```

> Move construct an element at the end of the vector from `x`.
>
> - _Requirements_: `MoveConstructible<value_type>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(1), exactly one call to `T`'s move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee: no side-effects if `value_type`'s move constructor throws. 
>   - re-throws if `value_type`'s constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `T`'s move constructor, the `size()` of the vector is 
> incremented by one.
>
> - _Pre-condition_: `size() < Capacity`.
> - _Post-condition_: `size() == size_before + 1`.


```c++
constexpr void pop_back();
```

> Removes the last element from the vector.
>
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_:
>   - time: O(1), exactly one call to `T`'s destructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee (note: `embedded_vector` requires `Destructible<T>`). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `T`'s destructor, the `size()` of the vector is 
> decremented by one.
>
> - _Pre-condition_: `size() > 0`.
> - _Post-condition_: `size() == size_before - 1`.


```c++
constexpr iterator insert(const_iterator position, const value_type& x);
```

> Stable inserts `x` at `position` within the vector (preserving the relative order of the elements in the vector).
>
> - _Requirements_: `CopyConstructible<value_type>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(size() + 1), exactly `end() - position` swaps, one call to `T`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `T`s copy constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position` swaps, one call to `T`'s copy constructor, the `size()` 
> of the vector is incremented by one.
>
> - _Pre-condition_: `size() + 1 <= Capacity`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before + 1`.
> - _Invariant_: the relative order of the elements before and after \p position is preserved.


```c++
constexpr iterator insert(const_iterator position, value_type&& x);
```

> Stable inserts `x` at `position` (preserving the relative order of the elements in the vector).
>
> - _Requirements_: `MoveConstructible<value_type>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(size() + 1), exactly `end() - position` swaps, one call to `T`'s move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `T`s move constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position` swaps, one call to `T`'s move constructor, the `size()` 
> of the vector is incremented by one.
>
> - _Pre-condition_: `size() + 1 <= Capacity`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before - 1`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.


```c++
constexpr iterator insert(const_iterator position, size_type n, const value_type& x);
```

> Stable inserts `n` copies of `x` at `position` (preserving the relative order of the elements in the vector).
>
> - _Requirements_: `CopyConstructible<value_type>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(size() + n), exactly `end() - position + n - 1` swaps, `n` calls to `T`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `T`s copy constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position + n - 1` swaps, `n` calls to `T`'s copy constructor, the `size()` 
> of the vector is incremented by `n`.
>
> - _Pre-condition_: `size() + n <= Capacity`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before + n`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.


```c++
template <typename InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);
```

> Stable inserts the elements of the range `[first, last)` at `position` (preserving the relative order of the elements in the vector).
>
> - _Requirements_: `Constructible<value_type, iterator_traits<InputIt>::value_type>`, `InputIterator<InputIt>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(size() + distance(first, last)), exactly `end() - position + distance(first, last) - 1` swaps, `n` calls to `T`'s copy constructor (note: independently of `InputIt`'s iterator category),
>   - space: O(1).
>
> - _Exception safety_: 
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `T`s copy constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position + distance(first, last) - 1` swaps, `n` calls to `T`'s copy constructor, the `size()` 
> of the vector is incremented by `n`.
>
> - _Pre-condition_: `size() + distance(first, last) <= Capacity`, `position` is in range `[begin(), end())`, `[first, last)` is not a sub-range of `[position, end())`.
> - _Post-condition_: `size() == size_before + distance(first, last)`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.

  
```c++
/// Equivalent to `insert(position, begin(il), end(il))`.
constexpr iterator insert(const_iterator position, initializer_list<value_type> il);
```

```c++
constexpr iterator erase(const_iterator position)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
```

> Stable erases the element at `position` (preserving the relative order of the elements in the vector).
>
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_:
> - time: O(size()), exactly `end() - position - 1` swaps, 1 call to `T`'s destructor. 
> - space: O(1).
>
> - _Exception safety_: 
> - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position - 1` swaps, 1 call to `T`'s destructor, the `size()` 
> of the vector is decremented by 1.
>
> - _Pre-condition_: `size() - 1 >= 0`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before - 1`, `size() >= 0`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.


```c++
constexpr iterator erase(const_iterator first, const_iterator last)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
```

> Stable erases the elements in range `[first, last)` (preserving the relative order of the elements in the vector).
>
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_:
> - time: O(size()), exactly `end() - first - distance(first, last)` swaps, `distance(first, last)` 
>   calls to `T`'s destructor. 
> - space: O(1).
>
> - _Exception safety_: 
> - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `first` are invalidated.
>
> - _Effects_: exactly `end() - first - distance(first, last)` swaps, 
>   `distance(first, last)` calls to `T`'s destructor, the `size()` 
> of the vector is decremented by `distance(first, last)`.
>
> - _Pre-condition_: `size() - distance(first, last) >= 0`, `[first, last)` is a sub-range of `[begin(), end())`.
> - _Post-condition_: `size() == size_before - distance(first, last)`, `size() >= 0`.
> - _Invariant_: the relative order of the elements before and after `position` remains unchanged.

```c++
/// Equivalent to `erase(begin(), end())`. 
constexpr void clear() noexcept(is_nothrow_destructible<value_type>{});
```

```c++
constexpr void swap(embedded_vector& other)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));
```

> Swaps the elements of two vectors.
>
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_:
>   - time: O(max(size(), other.size())), exactly `max(size(), other.size())` swaps. 
>   - space: O(1).
>
> - _Exception safety_: 
> - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to the elements of both vectors are invalidated.
>
> - _Effects_: exactly `max(size(), other.size())` swaps.
>
> - _Pre-condition_: none.
> - _Post-condition_: `size() == other_size_before`, `other.size() == size_before`.

## Comparison operators

The following operators are `noexcept` if the operations required to compute them are all `noexcept`:

```c++
constexpr bool operator==(const embedded_vector& a, const embedded_vector& b);
constexpr bool operator!=(const embedded_vector& a, const embedded_vector& b);
constexpr bool operator<(const embedded_vector& a, const embedded_vector& b);
constexpr bool operator<=(const embedded_vector& a, const embedded_vector& b);
constexpr bool operator>(const embedded_vector& a, const embedded_vector& b);
constexpr bool operator>=(const embedded_vector& a, const embedded_vector& b);
```
The following holds for the comparison operators:

- _Requirements_: only enabled if `value_type` supports the corresponding operations.
- _Enabled_: if requirements are met.
- _Complexity_: for two vectors of sizes `N` and `M`, the complexity is `O(1)` if `N != M`, and the comparison operator
  of `value_type` is invoked at most `N` times otherwise.
- _Exception safety_: `noexcept` if the comparison operator of `value_type` is `noexcept`, otherwise can only throw if the comparison operator can throw.

# Acknowledgments

The authors of Boost.Container's `boost::container::static_vector` (Adam
Wulkiewicz, Andrew Hundt, and Ion Gaztanaga). Howard Hinnant for libc++
`<algorithm>` and `<vector>` headers, and in particular, for the `<vector>` test
suite which was extremely useful while prototyping an implementation. Andrzej 
Krzemieński for providing an example that shows that using tags is better than
using static member functions for "special constructors" (like the default initialized 
constructor). Casey Carter for his invaluable feedback on this proposal.

# References

- [Boost.Container::static_vector][boost_static_vector].
  - Discussions in the Boost developers mailing list:
    - [Interest in StaticVector - fixed capacity vector](https:>>groups.google.com>d>topic>boost-developers-archive>4n1QuJyKTTk>discussion).
    - [Stack-based vector container](https:>>groups.google.com>d>topic>boost-developers-archive>9BEXjV8ZMeQ>discussion).
    - [static_vector: fixed capacity vector update](https:>>groups.google.com>d>topic>boost-developers-archive>d5_Kp-nmW6c>discussion).
- [Boost.Container::small_vector][boostsmallvector].
- [Howard Hinnant's stack_alloc][stack_alloc].
- [EASTL fixed_vector][eastl] and [design][eastldesign].
- [Folly small_vector][folly].
- [LLVM small_vector][llvmsmallvector].

<!-- Links -->
[stack_alloc]: https://howardhinnant.github.io/stack_alloc.html
[embedded_vector]: http://github.com/gnzlbg/embedded_vector
[boost_static_vector]: http://www.boost.org/doc/libs/1_59_0/doc/html/boost/container/static_vector.html
[travis-shield]: https://img.shields.io/travis/gnzlbg/embedded_vector.svg?style=flat-square
[travis]: https://travis-ci.org/gnzlbg/embedded_vector
[coveralls-shield]: https://img.shields.io/coveralls/gnzlbg/embedded_vector.svg?style=flat-square
[coveralls]: https://coveralls.io/github/gnzlbg/embedded_vector
[docs-shield]: https://img.shields.io/badge/docs-online-blue.svg?style=flat-square
[docs]: https://gnzlbg.github.io/embedded_vector
[folly]: https://github.com/facebook/folly/blob/master/folly/docs/small_vector.md
[eastl]: https://github.com/questor/eastl/blob/master/fixed_vector.h#L71
[eastldesign]: https:github.com/questor/eastl/blob/master/doc%2FEASTL%20Design.html#L284
[clump]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0274r0.pdf
[boostsmallvector]: http://www.boost.org/doc/libs/master/doc/html/boost/container/small_vector.html
[llvmsmallvector]: http://llvm.org/docs/doxygen/html/classllvm_1_1SmallVector.html
