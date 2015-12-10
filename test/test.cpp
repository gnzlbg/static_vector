/// \file
///
/// Test for stack::vector
///
/// Most of the tests below are adapted from libc++: https://libcxx.llvm.org
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

struct tint {
  int i;
  tint()                      = default;
  constexpr tint(tint const&) = default;
  constexpr tint(tint&&)      = default;
  constexpr tint& operator=(tint const&) = default;
  constexpr tint& operator=(tint&&) = default;

  constexpr tint(int j) : i(j) {}
  operator int() { return i; }
};

static_assert(std::is_trivial<tint>{} and std::is_copy_constructible<tint>{}
               and std::is_move_constructible<tint>{},
              "");

// Explicit instantiations
template struct std::experimental::stack_vector<tint, 0>;  // trivial empty
template struct std::experimental::stack_vector<tint, 1>;  // trivial non-empty
template struct std::experimental::stack_vector<tint, 2>;  // trivial nom-empty
template struct std::experimental::stack_vector<tint, 3>;  // trivial nom-empty

struct moint {
  int i;
  moint()             = default;
  moint(moint const&) = delete;
  moint& operator=(moint const&) = delete;
  moint(moint&&)                 = default;
  moint& operator=(moint&&) = default;
  operator int() { return i; }
  constexpr moint(int j) : i(j) {}
  // it seems that deleting the copy constructor is not enough to make this
  // non-trivial using libstdc++:
  virtual void foo() {}
};

static_assert(!std::is_trivial<moint>{} and !std::is_copy_constructible<moint>{}
               and std::is_move_constructible<moint>{},
              "");

// cannot explicitly instantiate the type for some types
// // non-trivial empty:
// template struct std::experimental::stack_vector<moint, 0>;
// // non-trivial non-empty:
// template struct std::experimental::stack_vector<moint, 1>;
// template struct std::experimental::stack_vector<moint, 2>;
// template struct std::experimental::stack_vector<moint, 3>;

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

class A {
  int i_;
  double d_;

  A(const A&);
  A& operator=(const A&);

 public:
  A(int i, double d) : i_(i), d_(d) {}

  A(A&& a) : i_(a.i_), d_(a.d_) {
    a.i_ = 0;
    a.d_ = 0;
  }

  A& operator=(A&& a) {
    i_   = a.i_;
    d_   = a.d_;
    a.i_ = 0;
    a.d_ = 0;
    return *this;
  }

  int geti() const { return i_; }
  double getd() const { return d_; }
};

int main() {
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
    constexpr vector<const int, 3> vc3 = {1, 2, 3};
    test_bounds(vc3, 3);
    static_assert(test_bounds(vc3, 3), "");
  }

  auto test_contiguous = [](auto&& c) {
    for (size_t i = 0; i < c.size(); ++i) {
      assert(*(c.begin() + i) == *(std::addressof(*c.begin()) + i));
    }
  };

  {  // contiguous
    typedef int T;
    typedef vector<T, 3> C;
    auto e = C();
    assert(e.empty());
    test_contiguous(e);
    test_contiguous(C(3, 5));
  }
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
    assert(a.empty());
    for (int i = 0; i != 10; ++i) { a.push_back(0); }
    assert(a.capacity() == std::size_t(10));
    assert(a.size() == std::size_t(10));
    assert(!a.empty());
  }

  {  // resize copyable
    using Copyable = int;
    vector<Copyable, 10> a(std::size_t(10), 5);
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
    a.resize(5, 2);
    for (int i = 0; i != 5; ++i) assert(a[i] == 5);
    test_contiguous(a);
  }
  {  // resize move-only
    using MoveOnly = std::unique_ptr<int>;
    vector<MoveOnly, 10> a(10);
    assert(a.size() == std::size_t(10));
    assert(a.capacity() == std::size_t(10));
    a.resize(5);
    test_contiguous(a);
    assert(a.size() == std::size_t(5));
    assert(a.capacity() == std::size_t(10));
    a.resize(9);
    assert(a.size() == std::size_t(9));
    assert(a.capacity() == std::size_t(10));
  }

  {  // resize value:
    using Copyable = int;
    vector<Copyable, 10> a(std::size_t(10));
    assert(a.size() == std::size_t(10));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
    for (int i = 0; i != 10; ++i) assert(a[i] == 0);
    a.resize(5);
    assert(a.size() == std::size_t(5));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
    for (int i = 0; i != 5; ++i) assert(a[i] == 0);
    a.resize(9, 5);
    for (int i = 0; i != 5; ++i) assert(a[i] == 0);
    for (int i = 5; i != 9; ++i) assert(a[i] == 5);
    assert(a.size() == std::size_t(9));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
    a.resize(10, 3);
    for (int i = 0; i != 5; ++i) assert(a[i] == 0);
    for (int i = 5; i != 9; ++i) assert(a[i] == 5);
    assert(a[9] == 3);
    assert(a.size() == std::size_t(10));
    assert(a.capacity() == std::size_t(10));
    test_contiguous(a);
  }

  {  // assign copy
    vector<int, 3> z(3, 5);
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

  {  // back and front:
    using C = vector<int, 2>;
    C c(1);
    assert(c.back() == 0);
    assert(c.front() == 0);
    assert(c[0] == 0);
    c.clear();
    int one = 1;
    c.push_back(one);
    assert(c.back() == 1);
    assert(c.front() == 1);
    assert(c[0] == 1);
    assert(c.size() == 1);
    c.push_back(2);
    assert(c.back() == 2);
    assert(c.front() == 1);
    assert(c[0] == 1);
    assert(c[1] == 2);
    assert(c.size() == 2);
    c.pop_back();
    assert(c.front() == 1);
    assert(c[0] == 1);
    assert(c.back() == 1);
    c.pop_back();
    assert(c.size() == 0);
  }

  {  // const back:
    using C = vector<int, 2>;
    const C c(1);
    assert(c.back() == 0);
    assert(c.front() == 0);
    assert(c[0] == 0);
    assert(c.size() == 1);
  }

  {  // swap: same type
    using C = vector<int, 5>;
    C c0(3, 5);
    C c1(5, 1);
    C c2(0);
    assert(c0.size() == std::size_t(3));
    assert(c1.size() == std::size_t(5));
    assert(c2.size() == std::size_t(0));
    for (int i = 0; i != 3; ++i) { assert(c0[i] == 5); }
    for (int i = 0; i != 5; ++i) { assert(c1[i] == 1); }
    c0.swap(c1);
    assert(c0.size() == std::size_t(5));
    assert(c1.size() == std::size_t(3));
    for (int i = 0; i != 5; ++i) { assert(c0[i] == 1); }
    for (int i = 0; i != 3; ++i) { assert(c1[i] == 5); }
    c2.swap(c1);
    assert(c1.size() == std::size_t(0));
    assert(c2.size() == std::size_t(3));
    for (int i = 0; i != 3; ++i) { assert(c2[i] == 5); }
  }

  {  // std::swap: same type
    using C = vector<int, 5>;
    C c0(3, 5);
    C c1(5, 1);
    C c2(0);
    assert(c0.size() == std::size_t(3));
    assert(c1.size() == std::size_t(5));
    assert(c2.size() == std::size_t(0));
    for (int i = 0; i != 3; ++i) { assert(c0[i] == 5); }
    for (int i = 0; i != 5; ++i) { assert(c1[i] == 1); }
    std::swap(c0, c1);
    assert(c0.size() == std::size_t(5));
    assert(c1.size() == std::size_t(3));
    for (int i = 0; i != 5; ++i) { assert(c0[i] == 1); }
    for (int i = 0; i != 3; ++i) { assert(c1[i] == 5); }
    std::swap(c2, c1);
    assert(c1.size() == std::size_t(0));
    assert(c2.size() == std::size_t(3));
    for (int i = 0; i != 3; ++i) { assert(c2[i] == 5); }
  }

  {  // TODO: throwing swap different types
    vector<int, 5> v;
    assert(v.data() != nullptr);

    vector<int, 0> v0;
    assert(v0.data() == nullptr);

    const vector<int, 5> cv;
    assert(cv.data() != nullptr);

    const vector<int, 0> cv0;
    assert(cv0.data() == nullptr);
  }

  {// emplace:
   {vector<A, 3> c;
  vector<A, 3>::iterator i = c.emplace(c.cbegin(), 2, 3.5);
  assert(i == c.begin());
  assert(c.size() == 1);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  i = c.emplace(c.cend(), 3, 4.5);
  assert(i == c.end() - 1);
  assert(c.size() == 2);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  assert(c.back().geti() == 3);
  assert(c.back().getd() == 4.5);
  i = c.emplace(c.cbegin() + 1, 4, 6.5);
  assert(i == c.begin() + 1);
  assert(c.size() == 3);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  assert(c[1].geti() == 4);
  assert(c[1].getd() == 6.5);
  assert(c.back().geti() == 3);
  assert(c.back().getd() == 4.5);
}
{
  vector<A, 3> c;
  vector<A, 3>::iterator i = c.emplace(c.cbegin(), 2, 3.5);
  assert(i == c.begin());
  assert(c.size() == 1);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  i = c.emplace(c.cend(), 3, 4.5);
  assert(i == c.end() - 1);
  assert(c.size() == 2);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  assert(c.back().geti() == 3);
  assert(c.back().getd() == 4.5);
  i = c.emplace(c.cbegin() + 1, 4, 6.5);
  assert(i == c.begin() + 1);
  assert(c.size() == 3);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  assert(c[1].geti() == 4);
  assert(c[1].getd() == 6.5);
  assert(c.back().geti() == 3);
  assert(c.back().getd() == 4.5);
}
}

{// emplace_back
 {vector<A, 2> c;
c.emplace_back(2, 3.5);
assert(c.size() == 1);
assert(c.front().geti() == 2);
assert(c.front().getd() == 3.5);
c.emplace_back(3, 4.5);
assert(c.size() == 2);
assert(c.front().geti() == 2);
assert(c.front().getd() == 3.5);
assert(c.back().geti() == 3);
assert(c.back().getd() == 4.5);
}
{
  vector<A, 2> c;
  c.emplace_back(2, 3.5);
  assert(c.size() == 1);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  c.emplace_back(3, 4.5);
  assert(c.size() == 2);
  assert(c.front().geti() == 2);
  assert(c.front().getd() == 3.5);
  assert(c.back().geti() == 3);
  assert(c.back().getd() == 4.5);
}
}

{// emplace extra:
 {vector<int, 4> v;
v = {1, 2, 3};
v.emplace(v.begin(), v.back());
assert(v[0] == 3);
}
{
  vector<int, 4> v;
  v = {1, 2, 3};
  v.emplace(v.begin(), v.back());
  assert(v[0] == 3);
}
}

{// erase
 {int a1[] = {1, 2, 3};
vector<int, 4> l1(a1, a1 + 3);
assert(l1.size() == 3);
vector<int, 4>::const_iterator i = l1.begin();
++i;
vector<int, 4>::iterator j = l1.erase(i);
assert(l1.size() == 2);
assert(std::distance(l1.begin(), l1.end()) == 2);
assert(*j == 3);
assert(*l1.begin() == 1);
assert(*std::next(l1.begin()) == 3);
j = l1.erase(j);
assert(j == l1.end());
assert(l1.size() == 1);
assert(std::distance(l1.begin(), l1.end()) == 1);
assert(*l1.begin() == 1);
j = l1.erase(l1.begin());
assert(j == l1.end());
assert(l1.size() == 0);
assert(std::distance(l1.begin(), l1.end()) == 0);
}
}

{  // erase iter iter
  int a1[]    = {1, 2, 3};
  using vec_t = vector<int, 5>;
  {
    vec_t l1(a1, a1 + 3);
    vec_t::iterator i = l1.erase(l1.cbegin(), l1.cbegin());
    assert(l1.size() == 3);
    assert(std::distance(l1.cbegin(), l1.cend()) == 3);
    assert(i == l1.begin());
  }
  {
    vec_t l1(a1, a1 + 3);
    vec_t::iterator i = l1.erase(l1.cbegin(), std::next(l1.cbegin()));
    assert(l1.size() == 2);
    assert(std::distance(l1.cbegin(), l1.cend()) == 2);
    assert(i == l1.begin());
    assert(l1 == vec_t(a1 + 1, a1 + 3));
  }
  {
    vec_t l1(a1, a1 + 3);
    vec_t::iterator i = l1.erase(l1.cbegin(), std::next(l1.cbegin(), 2));
    assert(l1.size() == 1);
    assert(std::distance(l1.cbegin(), l1.cend()) == 1);
    assert(i == l1.begin());
    assert(l1 == vec_t(a1 + 2, a1 + 3));
  }
  {
    vec_t l1(a1, a1 + 3);
    vec_t::iterator i = l1.erase(l1.cbegin(), std::next(l1.cbegin(), 3));
    assert(l1.size() == 0);
    assert(std::distance(l1.cbegin(), l1.cend()) == 0);
    assert(i == l1.begin());
  }
  {
    vector<vec_t, 3> outer(2, vec_t(1));
    outer.erase(outer.begin(), outer.begin());
    assert(outer.size() == 2);
    assert(outer[0].size() == 1);
    assert(outer[1].size() == 1);
  }
}

{// insert init list
 {vector<int, 15> d(10, 1);
vector<int, 15>::iterator i = d.insert(d.cbegin() + 2, {3, 4, 5, 6});
assert(d.size() == 14);
assert(i == d.begin() + 2);
assert(d[0] == 1);
assert(d[1] == 1);
assert(d[2] == 3);
assert(d[3] == 4);
assert(d[4] == 5);
assert(d[5] == 6);
assert(d[6] == 1);
assert(d[7] == 1);
assert(d[8] == 1);
assert(d[9] == 1);
assert(d[10] == 1);
assert(d[11] == 1);
assert(d[12] == 1);
assert(d[13] == 1);
}
}

{  // insert iter iter
  {
    vector<int, 120> v(100);
    int a[]             = {1, 2, 3, 4, 5};
    const std::size_t N = sizeof(a) / sizeof(a[0]);
    vector<int, 120>::iterator i = v.insert(v.cbegin() + 10, (a + 0), (a + N));
    assert(v.size() == 100 + N);
    assert(i == v.begin() + 10);
    int j;
    for (j = 0; j < 10; ++j) assert(v[j] == 0);
    for (std::size_t k = 0; k < N; ++j, ++k) assert(v[j] == a[k]);
    for (; j < 105; ++j) assert(v[j] == 0);
  }
  {
    vector<int, 120> v(100);
    size_t sz        = v.size();
    int a[]          = {1, 2, 3, 4, 5};
    const unsigned N = sizeof(a) / sizeof(a[0]);
    vector<int, 120>::iterator i = v.insert(v.cbegin() + 10, (a + 0), (a + N));
    assert(v.size() == sz + N);
    assert(i == v.begin() + 10);
    std::size_t j;
    for (j = 0; j < 10; ++j) assert(v[j] == 0);
    for (std::size_t k = 0; k < N; ++j, ++k) assert(v[j] == a[k]);
    for (; j < v.size(); ++j) assert(v[j] == 0);
  }
}

return 0;
}
