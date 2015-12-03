# stack_vector
> A stack allocated vector implementation Revision 0

# Motivation

Provide a stack allocated vector that can be used to replace std::vector as
easily as possible when needed.

## Cost of heap allocation

## Use cases

### Returning a polygon of maximum bounded size

### Returning multiple shapes

# Previous work

- [http://github.com/gnzlbg/stack_vector][stack_vector] provides an
implementation of this proposal, which is basically a reimplementation of
[`boost::container::static_vector` (1.59)][boost_static_vector] for
standardization.

# Design space

## The stack_allocator approach

- doesn't work since the allocation pattern of std::vector is unknown

## The stack vector approach

### Fall back to heap allocation?

- If one wants fall back to heap allocation one should use a `std::vector` with
  a `stack_allocator` that provides fall-back to heap allocation (e.g. Howard
  Hinnant's [`stack_alloc`][stack_alloc]).

### Zero-capacity stack_vector

- similar to zero-sized `std::array`

- equal:
  - zero-capacity stack_vector's swap is noexcept(true)
  - `begin() == end() == unspecified unique value` (`nullptr` is intended)
  - `data()` returns unspecified (`nullptr` is intended)

- different:
  - front/back are not provided
    - `std::array` defines calling front and back as undefined behavior
    - `stack_vector` of zero capacity does not have `front()/back()`,
      `std::enable_if` can be used for this purpose.

- allowed, the stack vector then has zero size and can be used as such when doing EBO

## Constexpr-ness

## Exception-safety

## Comparison operators

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
struct my_type : stack_vector<int, 0> {
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

## Other

- swap
- fill

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
constexpr explicit stack_vector(size_type n);
constexpr stack_vector(size_type n, const T& value);
template<class InputIterator>
constexpr stack_vector(InputIterator first, InputIterator last);
template<std::size_t M, enable_if_t<(C > M)>>
  constexpr vector(vector<T, M> const&);
constexpr vector(vector const&);
constexpr vector(vector&&);
constexpr vector(initializer_list<T>);

~vector() if not is_trivially_destructible<T>;
// otherwise stack_vector<T, C> has a trivial destructor

vector<T, C>& operator=(vector<T, C>const&);
vector<T, C>& operator=(vector<T, C>&&);
template<std::size_t M, enable_if_t<(C != M)>>
vector<T, C>& operator=(vector<T, M>const&);
template<std::size_t M, enable_if_t<(C != M)>>
vector<T, C>& operator=(vector<T, M>&&);

template<class InputIterator>
void assign(InputIterator first, InputIterator last);
void assign(size_type n, const T& u);
void assign(initializer_list<T>);

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
constexpr size_type capacity() const noexcept;
constexpr size_type max_size() const noexcept;
constexpr void resize(size_type sz);
constexpr void resize(size_type sz, const T& c);
constexpr bool empty() const noexcept;
void reserve(size_type n) = deleted;  // TODO: QoI? should ease replacing std::vector
void shrink_to_fit() = deleted; // TODO: QoI?

// element access:
constexpr reference       operator[](size_type n);  // TODO: noexcept?
constexpr const_reference operator[](size_type n) const; // TODO: noexcept?
constexpr const_reference at(size_type n) const;
constexpr reference       at(size_type n);
constexpr reference       front();  // noexcept?
constexpr const_reference front() const;  // noexcept?
constexpr reference       back();  // noexcept?
constexpr const_reference back() const;  // noexcept?

// data access:
constexpr       T* data()       noexcept;
constexpr const T* data() const noexcept;

// modifiers:
template<class... Args> void emplace_back(Args&&... args);
void push_back(const T& x);
void push_back(T&& x);
void pop_back();

template<class... Args>
  iterator emplace(const_iterator position, Args&&...args);
iterator insert(const_iterator position, const T& x);
iterator insert(const_iterator position, size_type n, const T& x);
template<class InputIterator>
  iterator insert(const_iterator position, InputIterator first, InputIterator last);

iterator insert(const_iterator position, initializer_list<T> il);
iterator erase(const_iterator position);
iterator erase(const_iterator first, const_iterator last);

void clear() noexcept;

constexpr void swap(stack_vector<T, C>&)
noexcept(noexcept(swap(declval<T&>(), declval<T&>()))));

};

template<typename T, std::size_t C0, std::size_t C1>
bool operator==(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
bool operator!=(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
bool operator<(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
bool operator<=(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
bool operator>(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);
template<typename T, std::size_t C0, std::size_t C1>
bool operator>=(const stack_vector<T, C0>& a, const stack_vector<T, C1>& b);

```

# TODO: wording

<!-- Links -->
[stack_alloc]: https://howardhinnant.github.io/stack_alloc.html
[stack_vector]: http://github.com/gnzlbg/stack_vector
[boost_static_vector]: http://www.boost.org/doc/libs/1_59_0/doc/html/boost/container/static_vector.html

