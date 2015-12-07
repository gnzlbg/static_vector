# stack_vector

> A stack allocated vector implementation (revision -1)

# Introduction

This paper proposes a resizable vector with stack storage provisionally called
`stack_vector<T, Capacity>`. Its API resembles that of `std::vector<T, A>` and
can be used almost as a drop-in replacement. A perfect replacement cannot be
provided because `stack_vector` lacks an `Allocator`.

The main aim of `stack_vector` is to enable users to replace `std::vector` in
the hot-path of their applications when the maximum number of elements that the
vector can hold is small and known at compile-time.

That is, if while profiling an application the developer discovers that memory
allocation due to the usage of `std::vector` dominates its hot path, it can then
go and grab `stack_vector` and perform a 1:1 replacement when a bound on the
capacity is known. The hassle to perform this replacement should be minimized,
and hence the reason of providing an almost 100% identical API to `std::vector`.

It is structured as follows. First the utility of `stack_vector` is addressed by
providing two motivating use cases: the efficient implementation of a `polygon`
(and how algorithms can use it), and the implementation of neighbor search in an
octree. Afterwards, previous work is cited and a current implementation of this
proposal is provided. The design space is considered next, the requirements are
listed, the different trade-offs that appeared while writing both the proposal
and the implementation are explored, and the design chosen are justified,
although since this is the first revision, they are all open for discussion.
Finally, the API is provided in a "standard-wording-style", and then its loosely
discussed in detail. Standard wording for this proposal is not provided.

# Motivation: mesh processing in HPC applications

## A polygon class and cell reshaping

Mesh generation and processing is an important part of numerical simulations in
high performance computing. Consider an algorithm in which the square-shaped
cells of a quadtree mesh need to be reshaped at the boundary with a moving
object by cutting them with linear surfaces. We can use a
`stack_vector<point<2>, 5>` to represent a polygon that can have a maximum of 5
points:

```c++
using polygon2d = stack_vector<point<2>, 5>;
```

The signature of our interesection algorithm can then look like this:

```c++
/// Splits a square2d with a surface2d into two polygons:
std::pair<polygon2d, polygon2d> split(square2d, surface2d);

```

Wether these polygons are to be stored on the stack or the heap is then irrelevant:

```c++
std::vector<polygon2d> heap_polygons(split(square, surface).first);
polygon2d stack_polygon = split(square, surface).second;
```

Alternative solutions would involve passing the container where the polygons
should be stored into the algorithm, and returning iterators. Such an algorithm
can be implemented on top of our solution.

## Leaf neighbor search in an octree

Consider finding all the leaf neighbors of a leaf node within an octree. Using
`stack_vector` the signature of such an algorithm can look like this:

```c++
template<std::size_t Dim>
stack_vector<node_iterator, max_number_of_leaf_neighbors(Dim)>
leaf_neighbors(octree<Dim> const&, node_iterator);
```

where `max_number_of_leaf_neighbors(Dim)` returns the maximum number of
neighbors that a leaf node can have in a given spatial dimension.

# Previous work and implementation

This proposal is strongly inspired by
[`boost::container::static_vector` (1.59)][boost_static_vector]. A new
implementation is provided for standardization purposes here:
[http://github.com/gnzlbg/stack_vector][stack_vector]

# Design space

In this section the design space is explored. 

## Requirements

The first step is to explicitly state the requiremnts of our type.
The requirements are:

  1. **Handyness**: replacing `vector<T>` with `stack_vector<T,C>` should be as
     easy as possible.
  2. **Performance**: `stack_vector<T,C>` should be as efficient as possible since
     its raison-d'etre is a performance optimization.

If it's not trivial to replace `vector` with `stack_vector`, those programmers
trying to skim some ms off a frame during an all-nighter previous to release day
will hate us (**Handyness**). If it's not as fast as possible, those programers
will need to reimplement their own, and will also hate us (**Performance**).

A consequence of these requirements is that the design space is very small,
which is a good thing.

## The stack-only allocator approach

The generic programmer in me begged me to reuse `vector` for implementing
`stack_vector`, maybe by providing it as a container adaptor similar to `queue`
or `stack`. To control where `vector` allocates its memory, I modified Howard
Hinnant's stack allocator ([`stack_alloc`][stack_alloc]) to not fall back to
heap allocation, but fail instead.

I then tested the vector using that stack allocator in two different platforms,
MacOSX and Linux, with two different standard library implementations, libc++
and libstdc++, respectively. What happened might already be obvious to the
reader: since the allocation pattern of `vector` is implementation defined, it
is impossible to predict for a given desired capacity how big should the stack
allocator be. I had to guess and overpredict it for it to work on one platform.
It turned out that wasn't enough for it to work on the other.

The two main problems with this approach are:

- it cannot produce portable code,
- trying to produce portable code requires overallocating, which goes against
the **Performance** requirement.

Another minor problem is that a `vector` using a stack allocator will necessary
be one word bigger than it needs to be (it needs to store the capacity even
thought that is known at compile-time). This is IMO not a major issue, but must
be considered since it also goes against the **Performance** requirement.

## The `stack_allocator` with heap-fallback approach

So what if we use Howard Hinnant's stack allocator
([`stack_alloc`][stack_alloc]) without modification, i.e., with a fallback to
heap allocation in case the `vector` wants more memory than what was reserved on
the stack?

That partially addresses one problem of the stack-only allocator approach:
portability. The code will work across all platforms and library implementations
since it can always fall back to heap allocation.

It introduces a new problem though:

- library-dependent performance due to the unknown growth factor of vector which
  can trigger heap allocations in your clients platform but not in your
  development platform.

It also retains the minor problem of the previous approach (1 extra word in
size).

## The new container (`stack_vector`) approach

A new type, `stack_vector<T, Capacity>`, can be introduced that exactly lets us
specify the amount of stack storage we want to reserve. This is the approach
pursued in the rest of the paper.

The main drawbacks of this approach is:
- codebloat

For each type `T`, and for each `Capacity`, this approach will instantiate a new
template class, and generate a significant amount of code.

I doesn't have a solution for this problem. 

I've considered:
- type-erasing the `Capacity`, but this would go against **Performance**.
- implementing `stack_vector` on top of a plausible stack-allocated array of
run-time size (`dyn_array`-like). This is pure speculation based on the old
proposals since we don't have a proposal on track for this feature yet.

and decided that code-bloat is an acceptable trade-off.


The advantages of the new container approach with respect to the other
approaches are:
- portability, and portable performance, and
- exact size.

### Other trade-offs


#### Zero-capacity stack_vector

- similar to zero-sized `std::array`

- equal:
  - zero-capacity stack_vector's swap is noexcept(true)
  - `data() == begin() == end() == unspecified unique value` (`nullptr` is intended)

- different:
  - front/back are not provided
    - `std::array` defines calling front and back as undefined behavior
    - `stack_vector` of zero capacity does not have `front()/back()`,
      `std::enable_if` can be used for this purpose.

- allowed, the stack vector then has zero size and can be used as such when doing EBO

#### Constexpr-ness

#### Exception-safety and noexceptness

#### Comparison operators

# API Notes

## Struct

- the template parameters are called `T` and `C` for Type and Capacity,
  respectively.
  - in `std::array` the template parameter is called `N` for size, but here it
    denotes capacity
- struct is chosen as for `std::array`:
  - since `stack_vector` cannot support aggregate initialization maybe it would
    be better to make it a `class` instead to differentiate it from `std::array`

## Types

- It has the same nested types as `std::vector<T>` except for `allocator_type`
  which makes no sense since `stack_vector<T, C>` does not have an allocator.


## Storage

- I went for `std::aligned_storage` since I wanted to preserve the semantics of
standard vector: elements in the storage are not default constructed.

### Zero-sized stack_vector

- The storage is, however, implementation-defined. I don't want to expose
  anything about the storage in the API, however, EBO should be guaranted for
  zero-sized `stack_vector`:

  ```c++
  struct my_type : stack_vector<int, 0>
  {
      int a;
  };
  static_assert(sizeof(my_type) == sizeof(int), "");
  ```

## Construction/Assignment/Destruction

### Copy construction

### Copy assignment

### Move construction

### Move assignment

### Destruction

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

## TODO: Iterator invalidation

### List of functions that can potentionally invalidate iterators and how


## Other

- swap
- fill

## Related possible future improvements

- constexpr friendly reverse iterator
- `std::default_init_t` such that `stack_vector` can provide a size constructor
with default initialization:

  ```c++
  stack_vector(size_t n, default_init_t);
  ```

- annotate explicitly deleted functions with a reason for the explicit deletion
  to improve error messages. For example it would be useful to annotate
  `reserve(size_t n)` for the stack allocated vector with the reason of the
  deletion to help users switch from std::vector faster.

- placement new is not supported in constexpr functions, as a consequence the
  following functions cannot be made constexpr in general, but can be made
  constexpr for `trivially_default_constructible` types only:
  - `emplace_back`
  - `push_back` (uses `emplace_back`)
  - `insert` (uses `emplace_back`)
  - `resize` (uses `insert`)
  - `stack_vector(size)` (uses `emplace_back` / `resize`)
  - `stack_vector(size, value)` (uses `emplace_back` / `resize`)
  - `stack_vector(begin, end)` (uses `insert`)
  - `stack_vector(initializer_list)` (uses `insert`)
  - `stack_vector& operator=(other)` (if `size()`s differ might need to use `insert`)
  - `assign(size)` (uses `emplace_back` / `resize`)
  - `assign(size, value)` (uses `emplace_back` / `resize`)
  - `assign(begin, end)` (uses `insert`)

- explicit destructor calls are not supported in constexpr functions, as a
  consequence the following functions cannot be made constexpr in general, but
  can be made constexpr for `trivially_default_destructible types`:
  - `pop_back`
  - `erase` 
  - `resize` (uses `erase`)
  - `stack_vector& operator=(other)` (if `size()`s differ might need to use
    explicit destructor calls)

# WIP: API

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

// construct/copy/move/destroy: TODO: consider conditionally noexcept?
constexpr stack_vector() noexcept;
constexpr explicit stack_vector(size_type n);
constexpr stack_vector(size_type n, const T& value);
template<class InputIterator>
constexpr stack_vector(InputIterator first, InputIterator last);
  template<std::size_t M, enable_if_t<(C != M)>>
constexpr stack_vector(stack_vector<T, M> const&);
template<std::size_t M, enable_if_t<(C != M)>>
  constexpr stack_vector(stack_vector<T, M> &&);
constexpr stack_vector(stack_vector const&);
constexpr stack_vector(stack_vector&&);
constexpr stack_vector(initializer_list<T>);

constexpr ~stack_vector();

constexpr stack_vector<T, C>& operator=(stack_vector<T, C>const&);
constexpr stack_vector<T, C>& operator=(stack_vector<T, C>&&);
template<std::size_t M, enable_if_t<(C != M)>>
constexpr stack_vector<T, C>& operator=(stack_vector<T, M>const&);
template<std::size_t M, enable_if_t<(C != M)>>
constexpr stack_vector<T, C>& operator=(stack_vector<T, M>&&);

template<class InputIterator>
constexpr void assign(InputIterator first, InputIterator last);
constexpr void assign(size_type n, const T& u);
cosntexpr void assign(initializer_list<T>);

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
constexpr void resize(size_type sz, const T& c);
constexpr bool empty() const noexcept;
void reserve(size_type n) = deleted;  // TODO: QoI?
void shrink_to_fit() = deleted; // TODO: QoI?

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
constexpr void push_back(const T& x);
constexpr void push_back(T&& x);
constexpr void pop_back();

template<class... Args>
  constexpr iterator emplace(const_iterator position, Args&&...args);
constexpr iterator insert(const_iterator position, const T& x);
constexpr iterator insert(const_iterator position, size_type n, const T& x);
template<class InputIterator>
  constexpr iterator insert(const_iterator position, InputIterator first, InputIterator last);

constexpr iterator insert(const_iterator position, initializer_list<T> il);
constexpr iterator erase(const_iterator position);
constexpr iterator erase(const_iterator first, const_iterator last);

constexpr void clear() noexcept;

constexpr void swap(stack_vector<T, C>&)
  noexcept(noexcept(swap(declval<T&>(), declval<T&>()))));

};

template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator==(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator!=(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator<=(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
constexpr bool operator>=(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);

```

# TODO: wording

<!-- Links -->
[stack_alloc]: https://howardhinnant.github.io/stack_alloc.html
[stack_vector]: http://github.com/gnzlbg/stack_vector
[boost_static_vector]: http://www.boost.org/doc/libs/1_59_0/doc/html/boost/container/static_vector.html

