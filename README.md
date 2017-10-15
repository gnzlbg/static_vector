# fixed_capacity_vector<T>

> A dynamically-resizable vector with fixed capacity and embedded storage (revision -1)

**Document number**: none.

**Audience**: LEWG/LWG.

**Date**: none.

**Author**: Gonzalo Brito Gadeschi.

# Table of contents

- [1. Introduction](#INTRODUCTION)
- [2. Motivation](#MOTIVATION)
- [3. Design](#DESIGN)
  - [3.1 FAQ](#FAQ)
  - [3.2 Existing practice](#PRACTICE)
  - [3.3 Proposed design and rationale](#RATIONALE)
- [4. Technical Specification](#TECHNICAL_SPECIFICATION)
  - [4.1 Overview](#OVERVIEW)
  - [4.2 Construction](#CONSTRUCTION)
  - [4.3 Assignment](#ASSIGNMENT)
  - [4.4 Destruction](#DESTRUCTION)
  - [4.5 Size and capacity](#SIZE)
  - [4.6 Element and data access](#ACCESS)
  - [4.7 Modifiers](#MODIFIERS)
  - [4.8 Zero sized `fixed_capacity_vector`](#ZERO_SIZED)
- [5. Acknowledgments](#ACKNOWLEDGEMENTS)
- [6. References](#REFERENCES)

# <a id="INTRODUCTION"></a>1. Introduction

This paper proposes a dynamically-resizable `vector` with fixed capacity and
contiguous embedded storage. That is, the elements of the vector are stored
within the vector object itself. This container is a modernized version of  
[`boost::container::static_vector<T, Capacity>`][boost_static_vector] [0] that
takes advantage of C++17.

Its API is almost a 1:1 map of `std::vector<T, A>`'s API. It is a contiguous
sequence random-access container (with contiguous storage), non-amortized `O(1)`
insertion and removal of elements at the end, and worst case `O(size())`
insertion and removal otherwise. Like `std::vector`, the elements are
initialized on insertion and destroyed on removal. It models
`ContiguousContainer` and its iterators model the `ContiguousIterator` concept.

# <a id="MOTIVATION"></a>2. Motivation

This container is useful when:

- memory allocation is not possible, e.g., embedded environments without a free store, 
  where only a stack and the static memory segment are available,
- memory allocation imposes an unacceptable performance penalty, e.g., with respect to latency, 
- allocation of objects with complex lifetimes in the _static_-memory segment is required,
- non-default constructible objects must be stored such that `std::array` is not an option,
- a dynamic resizable array is required within `constexpr` functions, 
- full control over the storage location of the vector elements is required.

# <a id="DESIGN"></a>3. Design

In this section Frequently Asked Questions are answered, an overview of existing
implementations is given, and the rationale behind the proposed design is
provided.

## <a id="FAQ"></a>3.1 FAQ

### 3.1.1 Can we reuse `std::vector` with a custom allocator? 

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

### 3.1.2 Can we reuse `small_vector`?

Yes, we can, but no, it does not result in a zero-cost abstraction.

The paper
[PR0274: Clump – A Vector-like Contiguous Sequence Container with Embedded Storage][clump] [10]
proposes a new type, `small_vector<T, N, Allocator>`, which is essentially a
`std::vector<T, Allocator>` that performs a Small Vector Optimization for up to
`N` elements, and then, depending on the `Allocator`, might fall-back to heap allocations,
or do something else (like `throw`, `assert`, `terminate`, introduce undefined behavior...). 

This small vector type is part of [Boost][boostsmallvector] [4], [LLVM][llvmsmallvector] [9], 
[EASTL][eastl] [6], and [Folly][folly] [8]. Most of these libraries special case `small_vector`
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

### 3.1.3 Should we special case `small_vector` for embedded-storage-only like EASTL and Folly do?

The types `fixed_capacity_vector` and `small_vector` have different algorithmic
complexity and exception-safety guarantees. They solve different problems and
should be different types. 

### 3.1.4 Can we reuse [P0494R0][contiguous_container] [11] - `contiguous_container` proposal?

The author has not tried but it might be possible to reuse this proposal to implement 
`fixed_capacity_vector` on top of it by defining a new `Storage` type. Note however, that
the semantics of `contiguous_container` highly depend on the concepts modeled by 
the `Storage` type. It is not even guaranteed that `contiguous_container` models 
`DefaultConstructible` (it only does so conditionally, if `Storage` does). So while
an implementation might be able to reuse `contiguous_container` to implement 
`fixed_capacity_vector`, its interface, or that of its storage, would still need to be 
specified.

## <a id="PRACTICE"></a>3.2 Existing practice

There are at least 3 widely used implementations of `fixed_capacity_vector`.

This proposal is strongly inspired by Boost.Container, which offers
[`boost::container::static_vector<T, Capacity>` (1.59)][boost_static_vector] [0],
and, as a different type, also offers `boost::container::small_vector<T, N, Allocator>`
as well.

The other two libraries that implement `fixed_capacity_vector` are [Folly][folly] [8] and
[EASTL][eastl] [6]. Both of these libraries implement it as a special case of
`small_vector` (which in both of these libraries has 4 template parameters).

EASTL `small_vector` is called `fixed_vector<T, N,
hasAllocator, OverflowAllocator>` and uses a boolean template parameter to
indicate if the only storage mode available is embedded storage. The
[design documents of EASTL][eastldesign] [7] seem to predate this special casing
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

## <a id="RATIONALE"></a>3.3 Proposed design and rationale

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
[design document of the EASTL][eastldesign] [7] points out, increased code size.
Since this container type is opt-in, only those users that need it will pay
this cost. Common techniques to reduce code size where explored in the 
prototype implementation (e.g. implementing `Capacity`/`value_type` agnostic 
functionality in a base class) without success, but implementations are 
encouraged to consider code-size as a quality of implementation issue.

### 3.3.1 Preconditions

This container requires that `value_type` models `Destructible`. If `value_type`'s destructor
throws the behavior is undefined.

### 3.3.2 Storage/Memory Layout

Specializations of `fixed_capacity_vector<T, Capacity>` model the `ContiguousContainer` concept.

The elements of the vector are properly aligned to an `alignof(T)` memory address.

The `fixed_capacity_vector<T, Capacity>::size_type` is the smallest unsigned integer type
that can represent `Capacity`.

If the container is not empty, the member function `data()` returns a pointer such that
`[data(), data() + size())` is a valid range and `data() == addressof(front()) == addressof(*begin())`.
Otherwise, the result of `data()` is unspecified.
  
**Note**: `fixed_capacity_vector<T, Capacity>` cannot be an aggregate since it provides
user-defined constructors.

#### 3.3.2.1 Zero-sized

It is required that `is_empty<fixed_capacity_vector<T, 0>>::value == true` and
that `swap(fixed_capacity_vector<T, 0>&, fixed_capacity_vector<T, 0>&)` is `noexcept(true)`.

### 3.3.3 Move semantics

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

### 3.3.4 Constexpr-support

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

### 3.3.5 Explicit instantiatiability

The class `fixed_capacity_vector<T, Capacity>` can be explicitly instantiated for
all combinations of `value_type` and `Capacity` if `value_type` satisfied the container preconditions 
(i.e. `value_type` models `Destructible<T>`).

### 3.3.6 Exception Safety

#### 3.3.6.1 What could possibly go wrong?

The only operations that can actually fail within `fixed_capacity_vector<T, Capacity>` are:

  1. `value_type` special member functions and `swap` can only fail due
     to throwing constructors/assignment/destructors/swap of `value_type`. 

  2. Mutating operations exceeding the capacity (`push_back`, `insert`, `emplace`, 
     `pop_back` when `empty()`, `fixed_capacity_vector(T, size)`, `fixed_capacity_vector(begin, end)`...).

  2. Out-of-bounds unchecked access:
     2.1 `front`/`back`/`pop_back` when empty, operator[] (unchecked random-access). 
     2.2  `at` (checked random-access) which can throw `out_of_range` exception.

#### 3.3.6.2 Rationale

Three points influence the design of `fixed_capacity_vector` with respect to its
exception-safety guarantees:

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
2. Make not exceeding the `Capacity` of an `fixed_capacity_vector` a
   precondition on its mutating method (and thus exceeding it
   undefined-behavior).

While throwing an exception makes the interface more similar to that of
`std::vector` and safer to use, it does introduces a performance cost since it
means that all the mutating methods must check for this condition. It also
raises the question: which exception? It cannot be `std::bad_alloc`, because
nothing is being allocated. It should probably be either `std::out_of_bounds` or
`std::logic_error`, but if exceeding the capacity is a logic error, why don't we
make it a precondition instead?

Making exceeding the capacity a precondition has some advantages:

- It allows implementations to trivially provide a run-time diagnostic on debug
  builds by, e.g., means of an assertion.

- It allows the methods to be conditionally marked `noexcept(true)` when `value_type` is `std::is_nothrow_default_constructible/copy_assignable>...`

- It makes `fixed_capacity_vector` a zero-cost abstraction by allowing the user to avoid unnecessary checks (e.g. hoisting checks out of a loop).

And this advantages come at the expense of safety. It is possible to have both by making the methods checked by default, but offering `unchecked_xxx` alternatives that omit the checks which increases the API surface.

Given this design space, this proposal opts for making not exceeding the `Capacity` of an `fixed_capacity_vector` a precondition. It still allows some safety by allowing implementations to make the operations checked in the debug builds of their standard libraries, while providing very strong exception safety guarantees (and conditional `noexcept(true)`), which makes `fixed_capacity_vector` a true zero-cost abstraction.

The final question to be answered is if we should mark the mutating methods to be conditionally `noexcept(true)` or not when it is safe to do so. The current proposal does so since it is easier to remove `noexcept(...)` than to add it, and since this should allow the compiler to generate better code, which is relevant for some fields in which `fixed_capacity_vector` is very useful, like in embedded systems programming. 

#### 3.3.6.3 Precondition on `value_type` modelling `Destructible`

If `value_type` models `Destructible` (that is, if `value_type` destructor never throws), 
`fixed_capacity_vector<T, Capacity>` provides at least the basic-exception guarantee.
If `value_type` does not model `Destructible`, the behavior of `fixed_capacity_vector` is undefined. 
Implementations are encouraged to rely on `value_type` modeling `Destructible` even
if `value_type`'s destructor is `noexcept(false)`. 

#### 3.3.6.4 Exception-safety guarantees of special member functions and swap

If `value_type`'s special member functions and/or swap are `noexcept(true)`, so are the respective
special member functions and/or swap operations of `fixed_capacity_vector` which then provide the
strong-exception guarantee. They provide the basic-exception guarantee otherwise. 

#### 3.3.6.5 Exception-safety guarantees of algorithms that perform insertions

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

#### 3.3.6.6 Exception-safety guarantees of unchecked access

Out-of-bounds unchecked access (`front`/`back`/`pop_back` when empty, `operator[]`) is undefined behavior
and a run-time diagnostic is encouraged but left as a Quality of Implementation issue.

These functions provide the strong-exception safety guarantee and are `noexcept(true)`.

#### 3.3.6.7 Exception-safety guarantees of checked access

Checked access via `at` provides the strong-exception safety guarantee and it throws the `std::out_of_range`
exception on out-of-bounds. It is `noexcept(false)`.

#### 3.3.6.8 On the general use of noexcept in this proposal

This proposal aims to follow the standard library guidelines for making functions `noexcept`. That is, a function can only be `noexcept` if it has no preconditions (a wide contract). For functions without preconditions, the proposal specifies when `noexcept` applies.

### 3.3.7 Iterators 

The iterators of `fixed_capacity_vector<T, Capacity>` model the `ContiguousIterator` concept.

#### 3.3.7.1 Iterator invalidation

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

### 3.3.8  Naming

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

### 3.3.9 Impact on the standard

`fixed_capacity_vector<T, Capacity>` is a pure library extension to the C++ standard library and can be implemented by any C++11 compiler in a separate header `<fixed_capacity_vector>`. 

### 3.3.10 Future extensions 

#### 3.3.10.1 Interoperability of embedded vectors with different capacities

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

#### 3.3.10.2 Default initialization

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

#### 3.3.10.3 Unchecked mutating operations

In the current proposal exceeding the capacity on the mutating operations is
considered a logic-error and results in undefined behavior, which allows
implementations to cheaply provide an assertion in debug builds without
introducing checks in release builds. If a future revision of this paper changes
this to an alternative solution that has an associated cost for checking the
invariant, it might be worth it to consider adding support to unchecked mutating
operations like `resize_unchecked`,`push_back_unchecked`, `assign_unchecked`,
`emplace`, and `insert`.


#### 3.3.10.4  `with_size` / `with_capacity` constructors

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

# <a id="TECHNICAL_SPECIFICATION"></a>4. Technical specification

---

Note to editor: This enhancement is a pure header-only addition to the C++ standard library as
the `<experimental/fixed_capacity_vector>` header. It belongs in the "Sequence
containers" (26.3) part of the "Containers library" (26) as "Class template
`fixed_capacity_vector`".

---

## 4. Class template `fixed_capacity_vector`

### <a id="OVERVIEW"></a>4.1 Class template `fixed_capacity_vector` overview

- 1. A `fixed_capacity_vector` is a contiguous container that supports constant
  time insert and erase operations at the end; insert and erase in the middle
  take linear time. Its capacity is part of its type and its elements are stored
  within the vector object itself, meaning that that if `v` is a
  `fixed_capacity_vector<T, Capacity>` then it obeys the identity `&v[n] ==
  &v[0] + n` for all `0 <= n <= v.size()`.

- 2. A `fixed_capacity_vector` satisfies all of the requirements of a container
  and of a reversible container (given in two tables in 26.2), of a sequence
  container, including the optional sequence container requirements (26.2.3),
  and of a contiguous container (26.2.1). The exceptions are the `push_front`,
  `pop_front`, and `emplace_front` member functions, which are not provided, and
  `swap`, which has linear complexity instead of constant complexity.
  Descriptions are provided here only for operations on `fixed_capacity_vector`
  that are not described in one of these tables or for operations where there is
  additional semantic information.

---

Note (not part of the specification): An incomplete type `T` cannot be used when
instantiating the vector, as opposed to `std::vector`.

---


```c++
namespace std {

template<typename T, std::size_t C /* Capacity */>
class fixed_capacity_vector {
public:
// types:
using value_type = T;
using pointer = T*;
using const_pointer = T const*; 
using reference = value_type&;
using const_reference = const value_type&;
using size_type =  /*smallest unsigned integer type that can represent C (Capacity)*/;
using difference_type = std::make_signed_t<size_type>;
using iterator = implementation-defined;  // see [container.requirements]
using const_iterator = implementation-defined; // see [container.requirements]
using reverse_iterator = reverse_iterator<iterator>;
using const_reverse_iterator = reverse_iterator<const_iterator>;

// 4.2, copy/move construction:
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

// 4.3, copy/move assignment:
constexpr fixed_capacity_vector& operator=(fixed_capacity_vector const& other)
  noexcept(is_nothrow_copy_assignable<value_type>{});
constexpr fixed_capacity_vector& operator=(fixed_capacity_vector && other);
  noexcept(is_nothrow_move_assignable<value_type>{});
template<class InputIterator>
constexpr void assign(InputIterator first, InputIterator last);
constexpr void assign(size_type n, const value_type& u);
constexpr void assign(initializer_list<value_type> il);

// 4.4, destruction
/* constexpr ~fixed_capacity_vector(); */ // implicitly generated

// iterators
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

// 4.5, size/capacity:
constexpr bool empty() const noexcept;
constexpr size_type size()     const noexcept;
static constexpr size_type max_size() noexcept;
static constexpr size_type capacity() noexcept;
constexpr void resize(size_type sz);
constexpr void resize(size_type sz, const value_type& c)

// 4.6, element and data access:
constexpr reference       operator[](size_type n) noexcept; 
constexpr const_reference operator[](size_type n) const noexcept;
constexpr const_reference at(size_type n) const;
constexpr reference       at(size_type n);
constexpr reference       front() noexcept;
constexpr const_reference front() const noexcept;
constexpr reference       back() noexcept;
constexpr const_reference back() const noexcept;
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;

// 4.7, modifiers:
constexpr iterator insert(const_iterator position, const value_type& x);
constexpr iterator insert(const_iterator position, value_type&& x);
constexpr iterator insert(const_iterator position, size_type n, const value_type& x);
template<class InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);
constexpr iterator insert(const_iterator position, initializer_list<value_type> il);

template<class... Args>
  constexpr iterator emplace(const_iterator position, Args&&...args)
template<class... Args>
  constexpr reference emplace_back(Args&&... args);
constexpr void push_back(const value_type& x);
constexpr void push_back(value_type&& x);

constexpr void pop_back();
constexpr iterator erase(const_iterator position)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator erase(const_iterator first, const_iterator last)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});

constexpr void clear() noexcept(is_nothrow_destructible<value_type>{});

constexpr void swap(fixed_capacity_vector x)
  noexcept(noexcept(is_nothrow_swappable_v<value_type>));
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

// 4.8, specialized algorithms:
template <typename T, std::size_t Capacity>
constexpr void swap(fixed_capacity_vector<T, Capacity>& x, fixed_capacity_vector<T, Capacity>& y)
  noexcept(noexcept(x.swap(y)));
  
}  // namespace std
```

## <a id="CONSTRUCTION"></a>4.2 `fixed_capacity_vector` constructors

```c++
constexpr fixed_capacity_vector() noexcept;
```

> - _Effects_: constructs an empty `fixed_capacity_vector`.
>
> - _Complexity_: Constant.

```c++
constexpr explicit fixed_capacity_vector(size_type n);
```

> - _Effects_: constructs a `fixed_capacity_vector` with `n` default-inserted elements.
>
> - _Requires_: `value_type` shall be `DefaultInsertable` into `*this`.
>
> - _Complexity_: Linear in `n`.

```c++
constexpr fixed_capacity_vector(size_type n, const value_type& value);
```

> - _Effects_: Constructs a `fixed_capacity_vector` with `n` copies of `value`.
>
> - _Requires_: `value_type` shall be `CopyInsertable` into `*this`.
>
> - _Complexity_: Linear in `n`.

```c++ 
template<class InputIterator>
constexpr fixed_capacity_vector(InputIterator first, InputIterator last);
```

> - _Effects_: Constructs a `fixed_capacity_vector` equal to the range `[first, last)`
>
> - _Requires_: `value_type` shall be `EmplaceConstructible` into `*this` from `*first`.
>
> - _Complexity_: Initializes `distance(first, last)` `value_type`s. 

## <a id="DESTRUCTION"></a>4.4 Destruction

- 1. The destructor shall be implicitly generated and constexpr if
`is_trivial<value_type>` is true.


## <a id="SIZE"></a>4.5 Size and capacity


```c++
static constexpr size_type capacity() noexcept;
```

> - _Returns_: the total number of elements that the vector can hold.
> 
> - _Complexity_: constant.

```c++
static constexpr size_type max_size() noexcept;
```

> - _Note_: returns `capacity()`.

```c++
constexpr void resize(size_type sz);  // (1)
```

> - _Effects_: If `sz < size()`, erases the last `size() - sz` elements from the
>   sequence. If `sz >= size()` and `sz <= capacity()`, appends `sz - size()`
>   copies of `c` to the sequence. The effect of calling resize if `sz >
>   capacity()` is undefined.
>
> - _Requires_: `value_type` shall be `MoveInsertable` and `DefaultInsertable` into `*this`.
>
> - _Note_: `constexpr `if `is_trivial<value_type>`.

Notes (not part of the specification):


> - _Exception safety_: `noexcept` never: narrow contract.
>
> - _Precondition_: `new_size <= capacity()`.


```c++
constexpr void resize(size_type sz, const value_type& c); // (2)
```

> - _Effects_: If `sz < size()`, erases the last `size() - sz` elements from the
>   sequence. If `sz >= size()` and `sz <= capacity()`, appends `sz - size()`
>   copies of `c` to the sequence. The effect of calling resize if `sz >
>   capacity()` is undefined.
>
> - _Requires_: `value_type` shall be `MoveInsertable` and `DefaultInsertable` into `*this`.
>
> - _Note_: `constexpr `if `is_trivial<value_type>`.

Notes (not part of the specification):

> - _Exception safety_: `noexcept` never: narrow contract.
>
> - _Precondition_: `new_size <= capacity()`.

## <a id="ACCESS"></a>4.6 Element and data access

- 1. The checked and unchecked element access functions: `at`, `front`, `back`,
  and `operator[]`, are `constexpr` if `is_trivial<value_type>`.

For the data access:

```c++
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;
```

> - _Returns_: A pointer such that `[data(), data() + size())` is a valid range.
>    For a non-empty vector, `data() == addressof(front())`.
>
> - _Complexity_: Constant time.

## <a id="MODIFIERS"></a>4.7 Modifiers

```c++
constexpr iterator insert(const_iterator position, const value_type& x);
constexpr iterator insert(const_iterator position, value_type&& x);
constexpr iterator insert(const_iterator position, size_type n, const value_type& x);
template <typename InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);
constexpr iterator insert(const_iterator position, initializer_list<value_type> il);

template<class... Args>
constexpr reference emplace_back(Args&&... args);
template<class... Args>
constexpr iterator emplace_back(const_iterator position, Args&&... args);
constexpr void push_back(const value_type& x);
constexpr void push_back(value_type&& x);
```

> - _Remarks_: If the new size is greater than the `capacity()` the behavior is
> undefined. All the iterators and references before the insertion point remain
> valid. If an exception is thrown other than by the copy constructor, move
> constructor, assignment operator, or move assignment operator of `value_type`
> or by any `InputIterator` operation there are no effects. If an exception is
> thrown while inserting a single element at the end and `value_type` is
> `CopyInsertable` or `is_nothrow_move_constructible_v<value_type>` is `true`,
> there are no effects. Otherwise, if an exception is thrown by the move
> constructor of a non-CopyInsertable `value_type`, the effects are unspecified.
>
> _Complexity_: Linear in the number of elements inserted plus the distance
> from the insertion point to the end of the vector.

Notes (not part of the specification):

> - _Pre-condition_: `new_size < Capacity`.
>
> - _Invariant_: the relative order of the elements before and after the
>   insertion point remains unchanged.


```c++
constexpr void pop_back();
constexpr iterator erase(const_iterator position)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
constexpr iterator erase(const_iterator first, const_iterator last)
  noexcept(is_nothrow_destructible<value_type>{} and is_nothrow_swappable<value_type>{});
```

> - _Effects_: Invalidates iterators and references at or after the point of the erase.
>
> - _Complexity_: The destructor of `value_type` is called the number of times
>   equal to the number of the elements erased, but the assignment operator of
>   `value_type` is called the number of times equal to the number of elements
>   in the vector after the erased elements.
>
> - Throws: Nothing unless an exception is thrown by the assignment operator or
>   move assignment operator of `value_type`.

Notes (not part of the specification):

> - _Pre-condition_: `new size > 0` is always true because size is an unsigned
>   integer, so these functions have no preconditions and wide contracts.
>
> - _Invariant_: the relative order of the elements before and after the erased
>   element range remains unchanged.


```c++
constexpr void swap(fixed_capacity_vector x)
  noexcept(noexcept(is_nothrow_swappable_v<value_type>));
```

> - _Effects_: Exchanges the contents of `*this` with `x`. All iterators
>   pointing to the elements of `*this` and `x` are invalidated.
>
> - _Complexity_: Linear in the number of elements  in `*this` and `x`.

## <a id="SPEC_ALG"></a>4.8 `fixed_capacity_vector` specialized algorithms

```c++
template <typename T, std::size_t Capacity>
constexpr void swap(fixed_capacity_vector<T, Capacity>& x, 
                    fixed_capacity_vector<T, Capacity>& y)
  noexcept(noexcept(x.swap(y)));
```

> - _Remarks_: This function shall not participate in overload resolution unless
>   `Capacity == 0` or `is_swappable_v<T>` is `true`.
>
> - _Effects_: As if by `x.swap(y)`.
>
> - _Complexity_: Linear in the number of elements in `x` and `y`.

## <a id="ZERO_SIZED"></a>4.9 Zero sized `fixed_capacity_vector`

- 1. `fixed_capacity_vector` shall provide support for the special case `Capacity == 0`.

- 2. In the case that `Capacity == 0`, `begin() == end() == unique value`. The return value of `data()` is unspecified.

- 3. The effect of calling `front()` or `back()` for a zero-sized array is undefined.

- 4. Non-member function `swap(fixed_capacity_vector, fixed_capacity_vector`)
  shall have a non-throwing exception specification.

# <a id="ACKNOWLEDGEMENTS"></a>5. Acknowledgments

The following people have significantly contributed to the development of this
proposal. This proposal is based on Boost.Container's
`boost::container::static_vector` and my extensive usage of this class over the
years. As a consequence the authors of Boost.Container (Adam Wulkiewicz, Andrew
Hundt, and Ion Gaztanaga) have had a very significant indirect impact on this
proposal. The implementation of libc++ `std::vector` and the libc++ test-suite
have been used extensively while prototyping this proposal, such that its
author, Howard Hinnant, has had a significant indirect impact on the result of
this proposal as well. The following people provided valuable feedback that
influenced some aspects of this proposal: Walter Brown, Zach Laine, Rein
Halbersma, and Andrzej Krzemieński (who provided an example that shows that
using tags is better than using static member functions for "special
constructors" like the `default_initialized_t` constructor). But I want to
wholeheartedly acknowledge Casey Carter for taking the time to do a very
detailed analysis of the whole proposal, which was invaluable and reshaped it in
fundamental ways.

# <a id="REFERENCES"></a>6. References

- [0] [Boost.Container::static_vector][boost_static_vector]: http://www.boost.org/doc/libs/1_59_0/doc/html/boost/container/static_vector.html .
  - Discussions in the Boost developers mailing list:
    - [1] [Interest in StaticVector - fixed capacity vector](https:>>groups.google.com>d>topic>boost-developers-archive>4n1QuJyKTTk>discussion):  https://groups.google.com/d/topic/boost-developers-archive/4n1QuJyKTTk/discussion .
    - [2] [Stack-based vector container](https:>>groups.google.com>d>topic>boost-developers-archive>9BEXjV8ZMeQ>discussion): https://groups.google.com/d/topic/boost-developers-archive/9BEXjV8ZMeQ/discussion.
    - [3] [static_vector: fixed capacity vector update](https:>>groups.google.com>d>topic>boost-developers-archive>d5_Kp-nmW6c>discussion).
- [4] [Boost.Container::small_vector][boostsmallvector]: http://www.boost.org/doc/libs/master/doc/html/boost/container/small_vector.html.
- [5] [Howard Hinnant's stack_alloc][stack_alloc]:  https://howardhinnant.github.io/stack_alloc.html .
- [6] [EASTL fixed_vector][eastl]: https://github.com/questor/eastl/blob/master/fixed_vector.h#L71 .
- [7] [EASTL design][eastldesign]: https://github.com/questor/eastl/blob/master/doc/EASTL%20Design.html#L284 .
- [8] [Folly small_vector][folly]: https://github.com/facebook/folly/blob/master/folly/docs/small_vector.md .
- [9] [LLVM small_vector][llvmsmallvector]: http://llvm.org/docs/doxygen/html/classllvm_1_1SmallVector.html .
- [10] [PR0274: Clump – A Vector-like Contiguous Sequence Container with Embedded Storage][clump]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0274r0.pdf
- [11] [P0494R0: `contiguous_container` proposal][contiguous_container]: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0494r0.pdf

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
