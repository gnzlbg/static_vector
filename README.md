# fixed_capacity_vector<T>

> A dynamically-resizable vector with fixed capacity and embedded storage (revision -1)

**Document number**: none.

**Date**: none.

**Author**: Gonzalo Brito Gadeschi.

# Introduction

This paper proposes a dynamically-resizable `vector` with fixed capacity and
contiguous embedded storage. That is, the elements of the vector are stored within
the vector object itself. This container is a modernized version of  
[`boost::container::static_vector<T, Capacity>`][boost_static_vector] that takes 
advantage of C++17.

Its API is almost a 1:1 map of `std::vector<T, A>`'s API. It is a contiguous sequence
random-access container (with contiguous storage), non-amortized `O(1)` insertion and removal of
elements at the end, and worst case `O(size())` insertion and removal otherwise. Like
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

In this section Frequently Asked Questions are answered, an overview of existing implementations is given, and the rationale behind the proposed design is provided.

## FAQ

### Can we reuse `std::vector` with a custom allocator? 

Yes, in practice we can, but neither in a portable way, nor in a way that results
in a zero-cost abstraction, mainly due to the following limitations in the `Allocator`
interface. 

1. The `Allocator::allocate(N)` member function either succeeds, returning a
pointer to the storage for `N` elements, or it fails. The current interface 
allows returning storage for more elements than requested, but it doesn't provide 
`Allocators` a way to communicate this situation to the container, so the 
containers cannot make use of it. 

2. The `Allocator` interface does not provide a way to "extend" the storage in place

3. The growth mechanism of `std::vector` is implementation defined. There is currently
no way for an `Allocator` with embedded storage to know how much memory a vector will
try to allocate for a maximum given number of elements at compile time. This information
is required for allocating a memory buffer large enough to satisfy the growth policy
of `std::vector` in its worst case. Even if this becomes achievable in a portable way,
a stateful allocator with embedded storage would still need to allocate
memory for `growth_factor() * (Capacity - 1)` vector elements to accomodate 
`std::vector`'s growth policy, which is far from optimal (and thus, zero cost).

4. An `Allocator` with embedded storage for `Capacity * sizeof(T)` elements makes
storing data members for the `data` pointer and the `capacity` unnecessary in 
`std::vector` implementation. In the current `Allocator` interface there is no mechanism
to communicate `std::vector` that storing these data members is unnecessary. 

5. The `Allocator` interface does not specify whether containers should propagate `noexcept`ness
off `Allocator` member functions into their interfaces. An `Allocator` with embedded storage for
`Capacity` elements _never throws_ on allocating memory, significantly improving the exception
safety guarantees of multiple `std::vector` operations. 

Improving the `Allocator` interface to solve issue 1. is enough to make an implementation
based on `std::vector` with an user-defined `Allocator` be portable (this should probably be
pursuded in a different proposal) since the `std::vector`constructors could 
`Allocator::allocate(0)` on construction and directly get memory for`Capacity` elements. 
However, in order to make this a zero-cost abstraction one would need
to solve issues 4 and 5 as well. Whether solving these issues is worth pursuing or not is
still an open problem. Solving issue 4 to avoid dupplicated copies of the data and capacity
members of vector could significantly complicate the `Allocator` interface.

The author of this proposal does not think that it will be possible to solve issues 4 and 5 in
the near future. Still, standard library implementors are encouraged to reuse their `std::vector` 
implementations when implementing `fixed_capacity_vector` if they are able to do so. Even if that leads
to a solution to all the issues above, an `fixed_capacity_vector` type is still an useful type to have
in the standard library independently of how it is defined (e.g. as a stand alone type, or as a type
alias of a `std::vector` with a specific allocator). 

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

The main difference between `small_vector` and `fixed_capacity_vector` is:

- `small_vector` uses embedded storage up-to `N` elements, and then falls back to
  `Allocator`, while `fixed_capacity_vector` _only_ provides embedded storage.

As a consequence, for the use cases of `fixed_capacity_vector`:

- `small_vector` cannot provide the same exception safety guarantees than
  `fixed_capacity_vector` because it sometimes allocates memory, which `fixed_capacity_vector`
  never does.

- `small_vector` cannot provide the same reliability than `fixed_capacity_vector`,
  because the algorithmic complexity of `small_vector` operations like move
  construction/assignment depends on whether the elements are allocated on the
  heap (swap pointer and size) or embedded within the vector object itself (must 
  always copy the elements).

- `small_vector` cannot be as efficient as `fixed_capacity_vector`. It must discriminate 
  between the different storage location of its elements (embedded within the vector
  object or owned by the   allocator). A part of its methods must, at run-time, branch 
  to a different code-path depending on the active storage scheme.

The only way to fix `small_vector` would be to special case it for
`Allocator::max_size() == 0` (which isn't constexpr), and to provide an API that
has different complexity and exception-safety guarantees than `small_vector`'s
with `Allocator::max_size() > 0`.

That is, to make `small_vector` competitive in those situations in which 
`fixed_capacity_vector` is required is to special case `small_vector` for a particular
allocator type and in that case provide an `fixed_capacity_vector` implementation (with
slightly different semantics). 

### Should we special case `small_vector` for embedded-storage-only like EASTL and Folly do?

The types `fixed_capacity_vector` and `small_vector` have different algorithmic
complexity and exception-safety guarantees. They solve different problems and
should be different types. 

### Can we reuse [P0494R0][contiguous_container] - `contiguous_container` proposal?

The author has not tried but it might be possible to reuse this proposal to implement 
`fixed_capacity_vector` on top of it by defining a new `Storage` type. Note however, that
the semantics of `contiguous_container` highly depend on the concepts modeled by 
the `Storage` type. It is not even guaranteed that `contiguous_container` models 
`DefaultConstructible` (it only does so conditionally, if `Storage` does). So while
an implementation might be able to reuse `contiguous_container` to implement 
`fixed_capacity_vector`, its interface, or that of its storage, would still need to be 
specified.

## Existing practice

There are at least 3 widely used implementations of `fixed_capacity_vector`.

This proposal is strongly inspired by Boost.Container, which offers
[`boost::container::static_vector<T, Capacity>` (1.59)][boost_static_vector],
and, as a different type, also offers `boost::container::small_vector<T, N, Allocator>`
as well.

The other two libraries that implement `fixed_capacity_vector` are [Folly][folly] and
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
`fixed_capacity_vector`s memory requirements.

## Proposed design and rationale

The current design follows that of `boost::container::static_vector<T,
Capacity>` closely.

It introduces a new type `std::experimental::fixed_capacity_vector<T, Capacity>` in the
`<experimental/fixed_capacity_vector>` header. It is a pure library extension to the C++ 
standard library.

> `fixed_capacity_vector` is a dynamically-resizable contiguous random-access sequence
> container with `O(1)` insert/erase at the end, and `O(size())` insert/erase otherwise.
> Its elements are stored within the vector object itself. It models `ContiguousContainer`
> and its iterators model the `ContiguousIterator` concept.

A prototype implementation of this proposal is provided for standardization
purposes: [`http://github.com/gnzlbg/embedded_vector`][fixed_capacity_vector].

The main drawback of introducing a new type is, as the
[design document of the EASTL][eastldesign] points out, increased code size.
Since this container type is opt-in, only those users that need it will pay
this cost. Common techniques to reduce code size where explored in the 
prototype implementation (e.g. implementing `Capacity`/`value_type` agnostic 
functionality in a base class) without success, but implementations are 
encouraged to consider code-size as a quality of implementation issue.

### Preconditions

This container requires that `value_type` models `Destructible`. If `value_type`'s destructor
throws the behavior is undefined.

### Storage/Memory Layout

Specializations of `fixed_capacity_vector<T, Capacity>` model the `ContiguousContainer` concept.

The elements of the vector are properly aligned to an `alignof(T)` memory address.

The `fixed_capacity_vector<T, Capacity>::size_type` is the smallest unsigned integer type
that can represent `Capacity`.

If the container is not empty, the member function `data()` returns a pointer such that
`[data(), data() + size())` is a valid range and `data() == addressof(front()) == addressof(*begin())`.
Otherwise, the result of `data()` is unspecified.
  
**Note**: `fixed_capacity_vector<T, Capacity>` cannot be an aggregate since it provides
user-defined constructors.

#### Zero-sized

It is required that `is_empty<fixed_capacity_vector<T, 0>>::value == true` and
that `swap(fixed_capacity_vector<T, 0>&, fixed_capacity_vector<T, 0>&)` is `noexcept(true)`.

### Move semantics

The move semantics of `fixed_capacity_vector<T, Capacity>` are equal to those of 
`std::array<T, Size>`. That is, after

```c++
fixed_capacity_vector a(10);
fixed_capacity_vector b(std::move(a));
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

The whole API of `fixed_capacity_vector<T, Capacity>` is `constexpr` if `is_trivial<T>`
is true.

Implementations can achieve this by using a C array for `fixed_capacity_vector`'s storage
without altering the guarantees of `fixed_capacity_vector`'s methods in an observable way.

For example, `fixed_capacity_vector(N)` constructor guarantees "exactly `N` calls to 
`value_type`'s default constructor". Strictly speaking, `fixed_capacity_vector`'s constructor
for trivial types will construct a C array of `Capacity` length. However, because
`is_trivial<T>` is true, the number of constructor or destructor calls is not
observable.

This introduces an additional implementation cost which the author believes is
worth it because similarly to `std::array`, `fixed_capacity_vector`s of trivial types 
are incredibly common. 

Note: this type of `constexpr` support does not require any core language changes.
This design could, however, be both simplified and extended if 1) placement `new`,
2) explicit destructor calls, and 3) `reinterpret_cast`, would be `constexpr`. This
paper does not propose any of these changes.

### Explicit instantiatiability

The class `fixed_capacity_vector<T, Capacity>` can be explicitly instantiated for
all combinations of `value_type` and `Capacity` if `value_type` satisfied the container preconditions 
(i.e. `value_type` models `Destructible<T>`).

### Exception Safety

#### What could possibly go wrong?

The only operations that can actually fail within `fixed_capacity_vector<T, Capacity>` are:

  1. `value_type` special member functions and swap can only fail due
     to throwing constructors/assignment/destructors/swap of `value_type`. 

  2. Mutating operations exceeding the capacity (`push_back`, `insert`, `emplace`, 
     `pop_back` when `empty()`, `fixed_capacity_vector(T, size)`, `fixed_capacity_vector(begin, end)`...).

  2. Out-of-bounds unchecked access:
     2.1 `front`/`back`/`pop_back` when empty, operator[] (unchecked random-access). 
     2.2  `at` (checked random-access) which can throw `out_of_range` exception.

#### Rationale

Three points influence the design of `fixed_capacity_vector` with respect to its exception-safety guarantees:

1. Making it a zero-cost abstraction.
2. Making it safe to use.
3. Making it easy to learn and use. 

The best way to make `fixed_capacity_vector` easy to learn is to make it as similar to `std::vector` 
as possible. However,`std::vector` allocates memory using an `Allocator`, whose allocation 
functions can throw, e.g., a `std::bad_alloc` exception, e.g., on Out Of Memory. 

However, `fixed_capacity_vector` never allocates memory since its `Capacity` is fixed at compile-time.

The main question then becomes, what should `fixed_capacity_vector` do when its `Capacity` is exceeded?

Two main choices were identified:

1. Make it throw an exception. 
2. Make not exceeding the `Capacity` of an `fixed_capacity_vector` a precondition on its mutating method (and thus exceeding it undefined-behavior).

While throwing an exception makes the interface more similar to that of `std::vector` and safer to use, it does introduces a performance cost since it means that all the mutating methods must check for this condition. It also raises the question: which exception? It cannot be `std::bad_alloc`, because nothing is being allocated.
It should probably be either `std::out_of_bounds` or `std::logic_error`, but if exceeding the capacity is a logic error, why don't we make it a precondition instead?

Making exceeding the capacity a precondition has some advantages:

- It llows implementations to trivially provide a run-time diagnostic on debug builds by, e.g., means of an assertion. 

- It allows the methods to be conditionally marked `noexcept(true)` when `value_type` is `std::is_nothrow_default_constructible/copy_assignable>...`

- It makes `fixed_capacity_vector` a zero-cost abstraction by allowing the user to avoid unnecessary checks (e.g. hoisting checks out of a loop).

And this advantages come at the expense of safety. It is possible to have both by making the methods checked by default, but offering `unchecked_xxx` alternatives that omit the checks which increases the API surface.

Given this design space, this proposal opts for making not exceeding the `Capacity` of an `fixed_capacity_vector` a precondition. It still allows some safety by allowing implementations to make the operations checked in the debug builds of their standard libraries, while providing very strong exception safety guarantees (and conditional `noexcept(true)`), which makes `fixed_capacity_vector` a true zero-cost abstraction.

The final question to be answered is if we should mark the mutating methods to be conditionally `noexcept(true)` or not when it is safe to do so. The current proposal does so since it is easier to remove `noexcept(...)` than to add it, and since this should allow the compiler to generate better code, which is relevant for some fields in which `fixed_capacity_vector` is very useful, like in embedded systems programming. 

#### Precondition on `value_type` modelling `Destructible`

If `value_type` models `Destructible` (that is, if `value_type` destructor never throws), 
`fixed_capacity_vector<T, Capacity>` provides at least the basic-exception guarantee.
If `value_type` does not model `Destructible`, the behavior of `fixed_capacity_vector` is undefined. 
Implementations are encouraged to rely on `value_type` modeling `Destructible` even
if `value_type`'s destructor is `noexcept(false)`. 

#### Exception-safety guarantees of special member functions and swap

If `value_type`'s special member functions and/or swap are `noexcept(true)`, so are the respective
special member functions and/or swap operations of `fixed_capacity_vector` which then provide the
strong-exception guarantee. They provide the basic-exception guarantee otherwise. 

#### Exception-safety guarantees of algorithms that perform insertions

The capacity of `fixed_capacity_vector` (a fixed-capacity vector) is statically known at 
compile-time, that is, exceeding it is a logic error.

As a consequence, inserting elements beyond the `Capacity` of an `fixed_capacity_vector` results
in _undefined behavior_. While providing a run-time diagnostic in debug builds (e.g. via an 
`assertion`) is encouraged, this is a Quality of Implementation issue.

The algorithms that perform insertions are the constructors `fixed_capacity_vector(T, size)` and 
 `fixed_capacity_vector(begin, end)`, and the member functions `push_back`, `emplace_back`, `insert`, 
and `resize`.

These algorithms provide strong-exception safety guarantee, and if `value_type`'s special member functions or
`swap` can throw are `noexcept(false)`, and `noexcept(true)` otherwise.

#### Exception-safety guarantees of unchecked access

Out-of-bounds unchecked access (`front`/`back`/`pop_back` when empty, `operator[]`) is undefined behavior
and a run-time diagnostic is encouraged but left as a Quality of Implementation issue.

These functions provide the strong-exception safety guarantee and are `noexcept(true)`.

#### Exception-safety guarantees of checked access

Checked access via `at` provides the strong-exception safety guarantee and it throws the `std::out_of_range`
exception on out-of-bounds. It is `noexcept(false)`.

#### On the general use of noexcept in this proposal

This proposal aims to follow the standard library guidelines for making functions `noexcept`. That is, a function can only be `noexcept` if it has no preconditions (a wide contract). For functions without preconditions, the proposal specifies when `noexcept` applies.

### Iterators 

The iterators of `fixed_capacity_vector<T, Capacity>` model the `ContiguousIterator` concept.

#### Iterator invalidation

The iterator invalidation rules are different than those for `std::vector`,
since:

- moving an `fixed_capacity_vector` invalidates all iterators (see note below),
- swapping two `fixed_capacity_vector`s invalidates all iterators, and 
- inserting elements at the end of an `fixed_capacity_vector` never invalidates iterators.

The following functions can potentially invalidate the iterators of `fixed_capacity_vector`s: 
`resize(n)`, `resize(n, v)`, `pop_back`, `erase`, and `swap`.

The following functions from the "possible future extensions" can potentially
invalidate the iterators of `fixed_capacity_vector`s: `resize_default_initialized(n)`,
`resize_unchecked(n)`, `resize_unchecked(n, v)`, and
`resize_default_initialized_unchecked(n)`.

Note: this proposal specifies that move assignment operations invalidate all
iterators, but this isn't necessarily the case. A `fixed_capacity_vector` is
move-assigned element-wise to another vector by calling the move assignment
operator on its elements. The elements are left in a "valid but unspecified
state", that is, iterators pointing to the elements of the vector that has been
move-assigned from still do point to valid objects. Arguably, for many types,
including those for which move assignment performs a copy, invalidating the
iterators of the original vector might seem extremely restrictive. However, even
if we decide to not invalidate iterators on move assignment, the semantics will
still be different to that of `std::vector`.

### Naming

Following names have been considered: 

- `fixed_capacity_vector`: this clearly indicates what this container is all about. 
- `embedded_vector<T, Capacity>`: since the elements are "embedded" within the vector object itself. 
   Sadly, the name `embedded` is overloaded, e.g., embedded systems, and while in this domain this container
   is very useful, it is not the only domain in which it is useful. 
- `static_vector` (Boost.Container): due to "static" / compile-time allocation of the elements. The term 
   `static` is, however, overloaded in C++ (e.g. `static` memory?).
- `inline_vector`: the elements are stored "inline" within the vector object itself. The term `inline` is,
   however, already overloaded in C++ (e.g. `inline` functions => ODR, inlining, `inline` variables).
- `stack_vector`: to denote that the elements can be stored on the stack, which is confusing since the
  elements can be on the stack, the heap, or the static memory segment. It also has a resemblance with 
  `std::stack`.

### Impact on the standard

`fixed_capacity_vector<T, Capacity>` is a pure library extension to the C++ standard library and can be implemented by any C++11 compiler in a separate header `<fixed_capacity_vector>`. 

### Future extensions 

#### Interoperability of embedded vectors with different capacities

A source of pain when using embedded vectors on API is that the vector `Capacity`
is part of its type. This is a problem worth solving, can be solved independently
of this proposal, and in a backwards compatible way with it.

To the author's best knowledge, the best way to solve this problem would be to 
define an `any_vector_view<T>` and `any_vector<T>` types with reference and value 
semantics respectively that use concept-based run-time polymorphism to erase 
the type of the vector. These types would work with any vector-like type and provide
infinite extensibility. Among the types they could work with are `std::vector`/ `boost::vector`
with different allocators, and `fixed_capacity_vector`s and `small_vector`s of different capacities
(and allocators for `small_vector`). 

A down-side of such an `any_vector_view/any_vector` type is that its efficient 
implementation would use virtual functions internally for type-erasure. Devirtualization
in non-trivial programs (multiple TUs) is still not a solved problem (not even
with the recent advances in LTO in modern compilers).

An `any_fixed_capacity_vector_view<T>` that provides reference
semantics for `fixed_capacity_vector`s can be implemented using the same 
techniques as `array_view<T>` without introducing virtual dispatch. This would
solve the pain points of using `fixed_capacity_vector<T, Capacity>` on APIs.

It is possible to implement and propose those types in a future proposal, but 
doing so is clearly out of this proposal's scope. 

#### Default initialization

The size-modifying operations of the `fixed_capacity_vector` that do not require a value
also have the following analogous counterparts that perform default
initialization instead of value initialization:

```c++
struct default_initialized_t {};
inline constexpr default_initialized_t default_initialized{};

template <typename Value, std::size_t Capacity>
struct fixed_capacity_vector {
    // ...
    constexpr fixed_capacity_vector(default_initialized_t, size_type n);
    constexpr void resize(default_initialized_t, size_type sz);
    constexpr void resize_unchecked(default_initialized_t, size_type sz);
};
```

#### Unchecked mutating operations

In the current proposal exceeding the capacity on the mutating operations is
considered a logic-error and results in undefined behavior, which allows
implementations to cheaply provide an assertion in debug builds without
introducing checks in release builds. If a future revision of this paper changes
this to an alternative solution that has an associated cost for checking the
invariant, it might be worth it to consider adding support to unchecked mutating
operations like `resize_unchecked`,`push_back_unchecked`, `assign_unchecked`,
`emplace`, and `insert`.


#### `with_size` / `with_capacity` constructors

Consider:

```c++
using vec_t = fixed_capacity_vector<std::size_t, N>;
vec_t v0(2);  // two-elements: 0, 0
vec_t v1{2};  // one-element: 2
vec_t v2(2, 1);  // two-elements: 1, 1
vec_t v3{2, 1};  // two-elements: 2, 1
```

A way to avoid this problem introduced by initializer list and braced
initialization, present in the interface of `fixed_capacity_vector` and
`std::vector`, would be to use a tagged-constructor of the form
`fixed_capacity_vector(with_size_t, std::size_t N, T const& t = T())` to
indicate that constructing a vector with `N` elements is inteded. For
`std::vector`, a similar constructor using a `with_capacity_t` and maybe
combinations thereof might make sense. This proposal does not propose any of
these, but this is a problem that should definetely be solved in STL2, and if it
solved, it should be solved for `fixed_capacity_vector` as well.

Note: a static member member function, e.g.,
`fixed_capacity_vector::with_size(N, T)` is a worse solution than using a
tagged-constructor because copy elision not work when the returned value is
perfect-forwarded to the destination object.

# Technical specification

This enhancement is a pure header-only addition to the C++ standard library as
the `<experimental/fixed_capacity_vector>` header.

```c++
template<typename T, std::size_t C /* Capacity */>
struct fixed_capacity_vector {

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
constexpr fixed_capacity_vector() noexcept;
constexpr explicit fixed_capacity_vector(size_type n);
constexpr fixed_capacity_vector(size_type n, const value_type& value);
template<class InputIterator>
constexpr fixed_capacity_vector(InputIterator first, InputIterator last);
constexpr fixed_capacity_vector(fixed_capacity_vector const& other)
  noexcept(is_nothrow_copy_constructible<value_type>{});
constexpr fixed_capacity_vector(fixed_capacity_vector && other)
  noexcept(is_nothrow_move_constructible<value_type>{});
constexpr fixed_capacity_vector(initializer_list<value_type> il);

/* constexpr ~fixed_capacity_vector(); */ // implicitly generated

constexpr fixed_capacity_vector& operator=(fixed_capacity_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
constexpr fixed_capacity_vector& operator=(fixed_capacity_vector && other);
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
  constexpr reference emplace_back(Args&&... args);
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

constexpr void swap(fixed_capacity_vector&)
  noexcept(noexcept(swap(declval<value_type&>(), declval<value_type&>()))));
};

template <typename T, std::size_t Capacity>
constexpr bool operator==(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b) noexcept(...);
template <typename T, std::size_t Capacity>
constexpr bool operator!=(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b) noexcept(...);
template <typename T, std::size_t Capacity>
constexpr bool operator<(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b) noexcept(...);
template <typename T, std::size_t Capacity>
constexpr bool operator<=(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b) noexcept(...);
template <typename T, std::size_t Capacity>
constexpr bool operator>(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b) noexcept(...);
template <typename T, std::size_t Capacity>
constexpr bool operator>=(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b) noexcept(...);

template <typename T, std::size_t Capacity>
constexpr void swap(fixed_capacity_vector<T, Capacity>&, fixed_capacity_vector<T, Capacity>&)
  noexcept(is_nothrow_swappable<T>{});
```

## Construction

---

```c++
constexpr fixed_capacity_vector() noexcept;
```

> Constructs an empty `fixed_capacity_vector`.
>
> - _Requirements_: none.
>
> - _Enabled_: always.
>
> - _Complexity_:
>   - time: O(1),
>   - space: O(1).
>
> - _Exception safety_: `noexcept` always.
>
> - _Constexpr_: always.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: none.
> 
> - _Post-condition_: `size() == 0`.

---

```c++
constexpr explicit fixed_capacity_vector(size_type n);
```

> Constructs an `fixed_capacity_vector` containing `n` value-initialized elements.
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
>   - `noexcept`: never, narrow contract.
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

---

```c++
constexpr fixed_capacity_vector(size_type n, const value_type& value);
```

> Constructs an `fixed_capacity_vector` containing `n` copies of `value`.
>
> - _Requirements_: `value_type` shall be `EmplaceConstructible` into `*this`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `n` calls to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept`: never, narrow contract.
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

---

```c++ 
template<class InputIterator>
constexpr fixed_capacity_vector(InputIterator first, InputIterator last);
```

> Constructs an `fixed_capacity_vector` containing a copy of the elements in the range `[first, last)`.
>
> - _Requirements_: `value_type` shall be either:
>   - `CopyInsertable` into `*this` _if_ the reference type of `InputIterator`
>      is an lvalue reference, or
>   - `MoveInsertable` into `*this` _if_ the reference type of `InputIterator`
>      is an rvalue reference.
>   - `InputIterator` must model `InputIterator`
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `last - first` calls to `value_type`'s copy or move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept`: never, narrow contract.
>   - strong guarantee: if [first, last) span a `ForwardRange`, the range is not modified and
>     all constructed elements shall be destroyed on failure,
>   - basic guarantee: if [first, last) span an `InputRange`, all constructed 
>     elements shall be destroyed on failure,
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

---

```c++
constexpr fixed_capacity_vector(fixed_capacity_vector const& other);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

> Constructs a `fixed_capacity_vector` whose elements are copied from `other`.
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
>   - `noexcept` if `is_nothrow_copy_constructible<value_type>{}`
>   - strong guarantee: all constructed elements shall be destroyed on failure,
>     other is not modified.
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

---

```c++
constexpr fixed_capacity_vector(fixed_capacity_vector&& other)
  noexcept(is_nothrow_move_constructible<value_type>{});
```

> Constructs an `fixed_capacity_vector` whose elements are moved from `other`.
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
>   - `noexcept` if `std::nothrow_move_assignable<T>` is true
>   - basic guarantee otherwise: all moved elements shall be destroyed on failure.
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

---

```c++
constexpr fixed_capacity_vector(initializer_list<value_type> il);
  noexcept(is_nothrow_copy_constructible<value_type>{});
```

> Equivalent to `fixed_capacity_vector(il.begin(), il.end())`.

---

## Assignment

```c++
constexpr fixed_capacity_vector& operator=(fixed_capacity_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
```

> Replaces the contents of the container by copy-assignment of the elements in `other`.
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
>   - `noexcept` if `std::nothrow_copy_assignable<T>`
>   - basic guarantee otherwise: `other` is not modified.
>   - re-throws if `value_type`'s copy constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly `other.size()` calls to `value_type`s copy constructor.
>
> - _Pre-condition_: none.
> - _Post-condition_: `size() == other.size()`.
> - _Invariant_: `other.size()` does not change.

---

```c++
constexpr fixed_capacity_vector& operator=(fixed_capacity_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
```

> Replaces the contents of the container by move-assignment of the elements in `other`.
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
>   - `noexcept` if `std::nothrow_move_assignable<T>`
>   - basic guarantee otherwise.
>   - re-throws if `value_type`'s move constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: always.
>
> - _Effects_: exactly `other.size()` calls to `value_type`s copy constructor.
>
> - _Pre-condition_: none.
> - _Post-condition_: `size() == other.size()`.
> - _Invariant_: `other.size()` does not change.

---

```c++
template<class InputIterator>
constexpr void assign(InputIterator first, InputIterator last);
```

> Replaces the contents of the container with the elements in range [first, last).
>
> - _Requirements_: `value_type` shall be either:
>   - `CopyInsertable` into `*this` _if_ the reference type of `InputIterator`
>      is an lvalue reference, or
>   - `MoveInsertable` into `*this` _if_ the reference type of `InputIterator`
>      is an rvalue reference.
>   - `InputIterator` must model `InputIterator`
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `last - first` calls to `value_type`'s copy or move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept`: never, narrow contract.
>   - basic guarantee: if [first, last) span a `ForwardRange` and 
>     the reference type of `InputIterator` is an lvalue reference
>     the range is not modified but the container is.
>   - basic guarantee: if [first, last) span an `InputRange` the 
>     input range and the container are modified.
>   - re-throws if `value_type`'s copy or move constructors throws,
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: always.
>
> - _Effects_: exactly `last - first` calls to `value_type`s copy or move constructor.
>
> - _Pre-condition_: none.
> - _Pre-condition_: `last - first <= Capacity`.
> - _Post-condition_: `size() == last - first`.

---

```c++
constexpr void assign(size_type n, const value_type& value);
```

> Replaces the contents of the container with `n` copies of `value`
>
> - _Requirements_: `value_type` shall be `EmplaceConstructible` into `*this`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: exactly `n` calls to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept`: never, narrow contract.
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

---

```c++
constexpr void assign(initializer_list<value_type> il);
```

> Equivalent to `fixed_capacity_vector::assign(il.begin(), il.end())`.

---

## Destruction

The destructor should be implicitly generated and it must be constexpr
if `is_trivial<value_type>`.

```c++
/* constexpr ~fixed_capacity_vector(); */ // implicitly generated
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
- _Exception safety_: `noexcept` always, wide contract.
- _Constexpr_: always.
- _Effects_: none.

The `iterator` and `const_iterator` types are implementation defined and model
the `ContiguousIterator` concept. 

Note: if the container is empty the result of the iterator functions is unspecified,
but implementations are encourages to make it consistent with the result 
of `fixed_capacity_vector::data()`.

## Size / capacity

---

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
> - _Exception safety_: `noexcept` always, wide contract.
>
> - _Constexpr_: always.
>
> - _Effects_: none.

---

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
> - _Exception safety_: `noexcept` always, wide contract.
>
> - _Constexpr_: always.
>
> - _Effects_: none.
>
> - _Note_:  
>   - if `capacity() == 0`, then `sizeof(fixed_capacity_vector) == 0`,
>   - if `sizeof(T) == 0 and capacity() > 0`, then `sizeof(fixed_capacity_vector) == sizeof(unsigned char)`.

---

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
> - _Exception safety_: `noexcept` always, wide contract.
>
> - _Constexpr_: always.
>
> - _Effects_: none.

---

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
> - _Exception safety_: `noexcept` always, wide contract.
>
> - _Constexpr_: always.
>
> - _Effects_: none.


---

For the checked `resize` functions:

```c++
constexpr void resize(size_type new_size);  // (1)
constexpr void resize(size_type new_size, const value_type& c); // (2)
```
the following holds:

- _Requirements_: `value_type` models:
  - (1): `DefaultInsertable` 
  - (2): `CopyInsertable`.
- _Enabled_: if requirements satisfied.
- _Complexity_: O(size()) time, O(1) space.
- _Exception safety_:
   - `noexcept` never: narrow contract.
   - basic guarantee: all constructed elements shall be destroyed on failure,
   - rethrows if `value_type`'s default or copy constructors throws,
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_:
  - if `new_size > size` exactly `new_size - size` elements (1): default / (2): copy constructed.
  - if `new_size < size`:
      - exactly `size - new_size` elements destroyed.
      - all iterators pointing to elements with `position > new_size` are invalidated.
- _Precondition_: `new_size <= capacity()`.

---

## Element / data access

---

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
- _Exception safety_: `noexcept` always, wide contract.
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_: none.
- _Pre-conditions_: `size() > n` for `operator[]`, `size() > 0` for `front` and `back`.

---

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

---

For the data access:

```c++
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;
```

the following holds: 

- _Requirements_: none.
- _Enabled_: always.
- _Complexity_: O(1) in time and space.
- _Exception safety_: `noexcept` always, wide contract.
- _Constexpr_: if `is_trivial<value_type>`.
- _Effects_: none.
- _Pre-conditions_: none.
- _Returns_: if the container is empty the return value is unspecified. If the container 
  is not empty, `[data(), data() + size())` is a valid range, and `data() == addressof(front())`.

---

## Modifiers

---

```c++
template<class... Args>
constexpr reference emplace_back(Args&&... args);
```

> Construct a new element at the end of the vector in place using the constructor arguments `args...`.
>
> - _Requirements_: `Constructible<value_type, Args...>`.
>
> - _Enabled_: if requirements are met.
>
> - _Complexity_:
>   - time: O(1), exactly one call to `value_type`'s constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee: no side-effects if `value_type`'s constructor throws. 
>   - re-throws if `value_type`'s constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `value_type`'s constructor, the `size()` of the vector 
> is incremented by one.
>
> - _Pre-condition_: `size() < Capacity`.
> - _Post-condition_: `size() == size_before + 1`.

---

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
>   - time: O(1), exactly one call to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee: no side-effects if `value_type`'s copy constructor throws. 
>   - re-throws if `value_type`'s constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `value_type`'s copy constructor, the `size()` of the vector is 
> incremented by one.
>
> - _Pre-condition_: `size() < Capacity`.
> - _Post-condition_: `size() == size_before + 1`.

---

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
>   - time: O(1), exactly one call to `value_type`'s move constructor,
>   - space: O(1).
>
> - _Exception safety_:
>   - `noexcept` never: narrow contract.
>   - basic guarantee: the vector is not modified if `value_type`'s move constructor throws,
>     whether `x` is modified depends on the guarantees of `value_type`'s' move constructor.
>   - re-throws if `value_type`'s constructor throws.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `value_type`'s move constructor, the `size()` of the vector is 
> incremented by one.
>
> - _Pre-condition_: `size() < Capacity`.
> - _Post-condition_: `size() == size_before + 1`.

---

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
>   - time: O(1), exactly one call to `value_type`'s destructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - basic guarantee (note: `fixed_capacity_vector` requires `Destructible<value_type>`). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: none.
>
> - _Effects_: exactly one call to `value_type`'s destructor, the `size()` of the vector is 
> decremented by one.
>
> - _Pre-condition_: `size() > 0`.
> - _Post-condition_: `size() == size_before - 1`.

---

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
>   - time: O(size() + 1), exactly `end() - position` swaps, one call to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `value_type`s copy constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position` swaps, one call to `value_type`'s copy constructor, the `size()` 
> of the vector is incremented by one.
>
> - _Pre-condition_: `size() + 1 <= Capacity`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before + 1`.
> - _Invariant_: the relative order of the elements before and after \p position is preserved.

---

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
>   - time: O(size() + 1), exactly `end() - position` swaps, one call to `value_type`'s move constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `value_type`s move constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position` swaps, one call to `value_type`'s move constructor, the `size()` 
> of the vector is incremented by one.
>
> - _Pre-condition_: `size() + 1 <= Capacity`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before - 1`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.

---

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
>   - time: O(size() + n), exactly `end() - position + n - 1` swaps, `n` calls to `value_type`'s copy constructor,
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `value_type`s copy constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position + n - 1` swaps, `n` calls to `value_type`'s copy constructor, the `size()` 
> of the vector is incremented by `n`.
>
> - _Pre-condition_: `size() + n <= Capacity`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before + n`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.

---

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
>   - time: O(size() + distance(first, last)), exactly `end() - position + distance(first, last) - 1` swaps, `n` calls to `value_type`'s copy constructor (note: independently of `InputIt`'s iterator category),
>   - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects 
> (note: even if `value_type`s copy constructor can throw). 
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position + distance(first, last) - 1` swaps, `n` calls to `value_type`'s copy constructor, the `size()` 
> of the vector is incremented by `n`.
>
> - _Pre-condition_: `size() + distance(first, last) <= Capacity`, `position` is in range `[begin(), end())`, `[first, last)` is not a sub-range of `[position, end())`.
> - _Post-condition_: `size() == size_before + distance(first, last)`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.

---
  
```c++
constexpr iterator insert(const_iterator position, initializer_list<value_type> il);
```

> Equivalent to `fixed_capacity_vector::insert(position, begin(il), end(il))`.

---

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
> - time: O(size()), exactly `end() - position - 1` swaps, 1 call to `value_type`'s destructor. 
> - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `position` are invalidated.
>
> - _Effects_: exactly `end() - position - 1` swaps, 1 call to `value_type`'s destructor, the `size()` 
> of the vector is decremented by 1.
>
> - _Pre-condition_: `size() - 1 >= 0`, `position` is in range `[begin(), end())`.
> - _Post-condition_: `size() == size_before - 1`, `size() >= 0`.
> - _Invariant_: the relative order of the elements before and after `position` is preserved.

---

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
>   calls to `value_type`'s destructor. 
> - space: O(1).
>
> - _Exception safety_: 
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to elements after `first` are invalidated.
>
> - _Effects_: exactly `end() - first - distance(first, last)` swaps, 
>   `distance(first, last)` calls to `value_type`'s destructor, the `size()` 
> of the vector is decremented by `distance(first, last)`.
>
> - _Pre-condition_: `size() - distance(first, last) >= 0`, `[first, last)` is a sub-range of `[begin(), end())`.
> - _Post-condition_: `size() == size_before - distance(first, last)`, `size() >= 0`.
> - _Invariant_: the relative order of the elements before and after `position` remains unchanged.

---

```c++
constexpr void clear() noexcept(is_nothrow_destructible<value_type>{});
```

> Equivalent to `fixed_capacity_vector::erase(begin(), end())`. 

---

```c++
constexpr void swap(fixed_capacity_vector& other)
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
>   - `noexcept` never: narrow contract.
>   - strong guarantee if `std::is_nothrow_swappable<value_type>`: no observable side-effects.
>
> - _Constexpr_: if `is_trivial<value_type>`.
>
> - _Iterator invalidation_: all iterators pointing to the elements of both vectors are invalidated.
>
> - _Effects_: exactly `max(size(), other.size())` swaps.
>
> - _Pre-condition_: none.
> - _Post-condition_: `size() == other_size_before`, `other.size() == size_before`.

---

## Comparison operators

```c++
template <typename T, std::size_t Capacity>
constexpr bool operator==(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b);
template <typename T, std::size_t Capacity>
constexpr bool operator!=(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b);
template <typename T, std::size_t Capacity>
constexpr bool operator<(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b);
template <typename T, std::size_t Capacity>
constexpr bool operator<=(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b);
template <typename T, std::size_t Capacity>
constexpr bool operator>(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b);
template <typename T, std::size_t Capacity>
constexpr bool operator>=(const fixed_capacity_vector<T, Capacity>& a, const fixed_capacity_vector<T, Capacity>& b);
```

The following holds for the comparison operators:

- _Requirements_: only enabled if `value_type` supports the corresponding operations.
- _Enabled_: if requirements are met.
- _Complexity_: for two vectors of sizes `N` and `M`, the complexity is `O(1)` if `N != M`, and the comparison operator
  of `value_type` is invoked at most `N` times otherwise.
- _Exception safety_: `noexcept` if the comparison operator of `value_type` is `noexcept`, otherwise strong guarantee (can only throw if the comparison operator can throw).

Note: the comparison operators have no preconditions, hence they have wide contracts, and this proposal makes them noexcept when the corresponding operator (e.g. `<`) for `value_type` is `noexcept(true)`.

# Acknowledgments

The following people have significantly contributed to the development of this
proposal. This proposal is based on Boost.Container's
`boost::container::static_vector` and my extensive usage of this class over the
years. As a consequence the authors of Boost.Container (Adam Wulkiewicz, Andrew
Hundt, and Ion Gaztanaga) have had a very significant indirect impact on this
proposal. The implementation of libc++ `std::vector` and the libc++ test-suite
have been used extensively while prototyping this proposal, such that its
author, Howard Hinnant, has had a significant indirect impact on the result of
this proposal as well. The following people provided valuable feedback that
influenced some aspects of this proposal: Zach Laine, Rein Halbersma, and
Andrzej Krzemieński (who provided an example that shows that using tags is
better than using static member functions for "special constructors" like the
`default_initialized_t` constructor). But I want to wholeheartedly acknowledge
Casey Carter for taking the time to do a very detailed analysis of the whole
proposal, which was invaluable and reshaped it in fundamental ways.

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
[eastldesign]: https://github.com/questor/eastl/blob/master/doc/EASTL%20Design.html#L284
[clump]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0274r0.pdf
[boostsmallvector]: http://www.boost.org/doc/libs/master/doc/html/boost/container/small_vector.html
[llvmsmallvector]: http://llvm.org/docs/doxygen/html/classllvm_1_1SmallVector.html
[contiguous_container]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0494r0.pdf
