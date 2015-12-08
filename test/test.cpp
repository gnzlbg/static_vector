/// \file
///
/// Test for stack::vector
///
/// Adapted from libc++: https://libcxx.llvm.org
/// under the following license:
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#include <iostream>
#include <memory>
#include <stack_vector>
#include <vector>
#include "utils.hpp"

// Explicit instantiations
template struct std::experimental::stack_vector<int, 0>;  // trivial empty
template struct std::experimental::stack_vector<int, 1>;  // trivial non-empty
template struct std::experimental::stack_vector<int, 2>;  // trivial nom-empty
template struct std::experimental::stack_vector<int, 3>;  // trivial nom-empty

struct non_trivial {
  int i = 0;
  virtual void foo() {}
};

static_assert(!std::is_trivial<non_trivial>{}, "");

// non-trivial empty:
template struct std::experimental::stack_vector<non_trivial, 0>;
// non-trivial non-empty:
template struct std::experimental::stack_vector<non_trivial, 1>;
template struct std::experimental::stack_vector<non_trivial, 2>;
template struct std::experimental::stack_vector<non_trivial, 3>;

template <typename T, std::size_t N>
using vector = std::experimental::stack_vector<T, N>;

template <typename T, std::size_t N>
constexpr bool test_bounds(vector<T, N> const& v, std::size_t sz) {
  assert(v.size() == sz);
  assert(v.max_size() == N);
  assert(v.capacity() == N);

  std::decay_t<T> count = std::decay_t<T>();
  for (std::size_t i = 0; i != sz; ++i) { assert(v[i] == ++count); }

  return true;
}

int main() {
  std::cerr << "here-10" << std::endl;
  {  // const
    vector<const int, 0> v0 = {};
    test_bounds(v0, 0);

    constexpr vector<const int, 0> vc0 = {};
    test_bounds(vc0, 0);
    static_assert(test_bounds(vc0, 0), "");

    // one and two elements initializer_list don't work
    // vector<const int, 1> v1 = {1};
    // test_bounds(v1, 1);
    //
    // constexpr vector<const int, 1> vc1 = {1};
    //  test_bounds(vc1, 1);
    //  static_assert(test_bounds(vc1, 1), "");

    vector<const int, 3> v3 = {1, 2, 3};
    test_bounds(v3, 3);
    std::cerr << "here-1" << std::endl;
    constexpr vector<const int, 3> vc3 = {1, 2, 3};
    test_bounds(vc3, 3);
    static_assert(test_bounds(vc3, 3), "");
    std::cerr << "here00" << std::endl;
  }

  auto test_contiguous = [](auto&& c) {
    for (size_t i = 0; i < c.size(); ++i) {
      assert(*(c.begin() + i) == *(std::addressof(*c.begin()) + i));
    }
  };

  {  // contiguous
    typedef int T;
    typedef vector<T, 3> C;
    test_contiguous(C());
    std::cerr << "here00000000" << std::endl;
    test_contiguous(C(3, 5));
  }
  std::cerr << "here00000" << std::endl;
  {  // default construct element
    typedef int T;
    typedef vector<T, 3> C;
    C c(1);
    assert(c.back() == 0);
    assert(c.front() == 0);
    assert(c[0] == 0);
  }

  {  // iterator
    typedef int T;
    typedef vector<T, 3> C;
    C c;
    C::iterator i = c.begin();
    C::iterator j = c.end();
    assert(std::distance(i, j) == 0);
    assert(i == j);
  }
  std::cerr << "here0000" << std::endl;
  {  // const iterator
    typedef int T;
    typedef vector<T, 3> C;
    const C c{};
    C::const_iterator i = c.begin();
    C::const_iterator j = c.end();
    assert(std::distance(i, j) == 0);
    assert(i == j);
  }
  {  // cbegin/cend
    typedef int T;
    typedef vector<T, 3> C;
    C c;
    C::const_iterator i = c.cbegin();
    C::const_iterator j = c.cend();
    assert(std::distance(i, j) == 0);
    assert(i == j);
    assert(i == c.end());
  }
  std::cerr << "here000" << std::endl;
  {  // iterator constructor
    typedef int T;
    typedef vector<T, 10> C;
    const T t[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    C c(std::begin(t), std::end(t));
    for (auto&& j : c) { std::cout << j << std::endl; }
    assert(std::equal(std::begin(t), std::end(t), std::begin(c), std::end(c)));
    C::iterator i = c.begin();
    assert(*i == 0);
    ++i;
    assert(*i == 1);
    *i = 10;
    assert(*i == 10);
    assert(std::distance(std::begin(c), std::end(c)) == 10);
    std::cerr << "here0" << std::endl;
  }
  {  // N3644 testing
    typedef vector<int, 10> C;
    C::iterator ii1{}, ii2{};
    C::iterator ii4 = ii1;
    C::const_iterator cii{};
    assert(ii1 == ii2);
    assert(ii1 == ii4);

    assert(!(ii1 != ii2));

    assert((ii1 == cii));
    assert((cii == ii1));
    assert(!(ii1 != cii));
    assert(!(cii != ii1));
    assert(!(ii1 < cii));
    assert(!(cii < ii1));
    assert((ii1 <= cii));
    assert((cii <= ii1));
    assert(!(ii1 > cii));
    assert(!(cii > ii1));
    assert((ii1 >= cii));
    assert((cii >= ii1));
    assert((cii - ii1) == 0);
    assert((ii1 - cii) == 0);
  }

  {  // capacity
    vector<int, 10> a;
    assert(a.capacity() == std::size_t(10));
    for (int i = 0; i != 10; ++i) { a.push_back(0); }
    assert(a.capacity() == std::size_t(10));
    assert(a.size() == std::size_t(10));
  }

  {
   // erase
  }

  {  // resize copyable
    using Copyable = int;
    std::cerr << "here1" << std::endl;
    vector<Copyable, 10> a(std::size_t(10), 5);
    std::cerr << "here2" << std::endl;
    assert(a.size() == std::size_t(10));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
    for (int i = 0; i != 10; ++i) assert(a[i] == 5);
    a.resize(5);
    assert(a.size() == std::size_t(5));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
    a.resize(9);
    assert(a[4] == 5);
    for (int i = 5; i != 9; ++i) assert(a[i] == 0);
    assert(a.size() == std::size_t(9));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
    a.resize(10, 3);
    assert(a[4] == 5);
    assert(a[8] == 0);
    assert(a[9] == 3);
    assert(a.size() == std::size_t(10));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
  }
  {  // resize move-only
    using MoveOnly = std::unique_ptr<int>;
    vector<MoveOnly, 10> a(10);
    assert(a.size() == std::size_t(10));
    assert(a.capacity() == std::size_t(10));
    a.resize(5);
    assert(a.size() == std::size_t(5));
    assert(a.capacity() == std::size_t(10));
    a.resize(9);
    assert(a.size() == std::size_t(9));
    assert(a.capacity() == std::size_t(10));
  }

  {  // assign copy
    vector<int, 3> a = {0, 1, 2};
    assert(a.size() == std::size_t{3});
    vector<int, 3> b;
    assert(b.size() == std::size_t{0});
    b = a;
    assert(b.size() == std::size_t{3});
    assert(std::equal(std::begin(a), std::end(a), std::begin(b), std::end(b)));
  }

  {  // copy construct
    vector<int, 3> a = {0, 1, 2};
    assert(a.size() == std::size_t{3});
    vector<int, 3> b(a);
    assert(b.size() == std::size_t{3});

    assert(std::equal(std::begin(a), std::end(a), std::begin(b), std::end(b)));
  }

  {  // assign move
    using MoveOnly = std::unique_ptr<int>;
    vector<MoveOnly, 3> a(3);
    assert(a.size() == std::size_t{3});
    vector<MoveOnly, 3> b;
    assert(b.size() == std::size_t{0});
    b = std::move(a);
    assert(b.size() == std::size_t{3});
    assert(a.size() == std::size_t{3});
  }

  {  // move construct
    using MoveOnly = std::unique_ptr<int>;
    vector<MoveOnly, 3> a(3);
    assert(a.size() == std::size_t{3});
    vector<MoveOnly, 3> b(std::move(a));
    assert(b.size() == std::size_t{3});
    assert(a.size() == std::size_t{3});
  }

  {  // old tests

    using stack_vec = vector<int, 5>;
    stack_vec vec1(5);
    vec1[0] = 0;
    vec1[1] = 1;
    vec1[2] = 2;
    vec1[3] = 3;
    vec1[4] = 4;
    {
      stack_vec vec2;
      vec2.push_back(5);
      vec2.push_back(6);
      vec2.push_back(7);
      vec2.push_back(8);
      vec2.push_back(9);
      assert(vec1[0] == 0);
      assert(vec1[4] == 4);
      assert(vec2[0] == 5);
      assert(vec2[4] == 9);
    }
    {
      auto vec2 = vec1;
      assert(vec2[0] == 0);
      assert(vec2[4] == 4);
      assert(vec1[0] == 0);
      assert(vec1[4] == 4);
    }
    {
      int count_ = 0;
      for (auto i : vec1) { assert(i == count_++); }
    }

    {
      std::vector<int> vec2(5);
      vec2[0] = 4;
      vec2[1] = 3;
      vec2[2] = 2;
      vec2[3] = 1;
      vec2[4] = 0;
      stack_vec vec(vec2.size());
      copy(std::begin(vec2), std::end(vec2), std::begin(vec));
      int count_ = 4;
      for (auto i : vec) { assert(i == count_--); }
    }
  }
  {
    using stack_vec = vector<int, 0>;
    static_assert(sizeof(stack_vec) == 1, "");

    constexpr auto a = stack_vec{};
    static_assert(a.size() == std::size_t{0}, "");
  }

  return 0;
}
