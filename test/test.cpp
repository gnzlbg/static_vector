/// \file
///
/// Test for inline_vector
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

#include <experimental/fixed_capacity_vector>
#include <memory>
#include <string>
#include <vector>
#include "utils.hpp"

#define FCV_ASSERT(...)                                                       \
    static_cast<void>((__VA_ARGS__)                                           \
                          ? void(0)                                           \
                          : ::std::experimental::fcv_detail::assert_failure(  \
                                static_cast<const char*>(__FILE__), __LINE__, \
                                "assertion failed: " #__VA_ARGS__))

template struct std::experimental::fcv_detail::empty_storage<int>;
template struct std::experimental::fcv_detail::trivial_storage<int, 10>;
template struct std::experimental::fcv_detail::non_trivial_storage<
    std::unique_ptr<int>, 10>;

template struct std::experimental::fcv_detail::empty_storage<const int>;
template struct std::experimental::fcv_detail::trivial_storage<const int, 10>;
template struct std::experimental::fcv_detail::non_trivial_storage<
    const std::unique_ptr<int>, 10>;

// empty:
template struct std::experimental::fixed_capacity_vector<int, 0>;

// trivial non-empty:
template struct std::experimental::fixed_capacity_vector<int, 1>;
template struct std::experimental::fixed_capacity_vector<int, 2>;
template struct std::experimental::fixed_capacity_vector<const int, 3>;

// non-trivial
template struct std::experimental::fixed_capacity_vector<std::string, 3>;
template struct std::experimental::fixed_capacity_vector<const std::string, 3>;

// move-only:
template struct std::experimental::fixed_capacity_vector<std::unique_ptr<int>,
                                                         3>;
template struct std::experimental::fixed_capacity_vector<
    const std::unique_ptr<int>, 3>;

struct[[gsl::suppress("cppcoreguidelines-special-member-functions")]] tint
{
    std::size_t i;
    tint()                      = default;
    constexpr tint(tint const&) = default;
    constexpr tint(tint &&)     = default;
    constexpr tint& operator=(tint const&) = default;
    constexpr tint& operator=(tint&&) = default;
    // FIXME: ~tint() = default;
    //        ^^^ adding this makes the class non-trivial in clang

    explicit constexpr tint(std::size_t j) : i(j)
    {
    }
    explicit operator std::size_t()
    {
        return i;
    }
};

static_assert(std::is_trivial<tint>{} and std::is_copy_constructible<tint>{}
                  and std::is_move_constructible<tint>{},
              "");

// Explicit instantiations
template struct std::experimental::fixed_capacity_vector<tint,
                                                         0>;  // trivial empty
template struct std::experimental::fixed_capacity_vector<tint, 1>;  // trivial
                                                                    // non-empty
template struct std::experimental::fixed_capacity_vector<tint, 2>;  // trivial
                                                                    // nom-empty
template struct std::experimental::fixed_capacity_vector<tint, 3>;  // trivial
                                                                    // nom-empty

struct moint final
{
    std::size_t i       = 0;
    moint()             = default;
    moint(moint const&) = delete;
    moint& operator=(moint const&) = delete;
    moint(moint&&)                 = default;
    moint& operator=(moint&&) = default;
    ~moint()                  = default;
    explicit operator std::size_t()
    {
        return i;
    }
    explicit constexpr moint(std::size_t j) : i(j)
    {
    }
    // it seems that deleting the copy constructor is not enough to make
    // this non-trivial using libstdc++:
    virtual void foo()
    {
    }
    bool operator==(moint b)
    {
        return i == b.i;
    }
};

static_assert(!std::is_trivial<moint>{} and !std::is_copy_constructible<moint>{}
                  and std::is_move_constructible<moint>{},
              "");

// cannot explicitly instantiate the type for some types
// // non-trivial empty:
// template struct std::experimental::fixed_capacity_vector<moint, 0>;
// // non-trivial non-empty:
// template struct std::experimental::fixed_capacity_vector<moint, 1>;
// template struct std::experimental::fixed_capacity_vector<moint, 2>;
// template struct std::experimental::fixed_capacity_vector<moint, 3>;

template <typename T, std::size_t N>
using vector = std::experimental::fixed_capacity_vector<T, N>;

template <typename T, std::size_t N>
constexpr bool test_bounds(vector<T, N> const& v, std::size_t sz)
{
    FCV_ASSERT(v.size() == sz);
    FCV_ASSERT(v.max_size() == N);
    FCV_ASSERT(v.capacity() == N);

    std::decay_t<T> count = std::decay_t<T>();
    for (std::size_t i = 0; i != sz; ++i)
    {
        ++count;
        FCV_ASSERT(v[i] == count);
    }

    return true;
}

class non_copyable
{
    int i_;
    double d_;

  public:
    non_copyable(const non_copyable&) = delete;
    non_copyable& operator=(const non_copyable&) = delete;

    non_copyable(int i, double d) : i_(i), d_(d)
    {
    }

    non_copyable(non_copyable&& a) noexcept : i_(a.i_), d_(a.d_)
    {
        a.i_ = 0;
        a.d_ = 0;
    }

    non_copyable& operator=(non_copyable&& a) noexcept
    {
        i_   = a.i_;
        d_   = a.d_;
        a.i_ = 0;
        a.d_ = 0;
        return *this;
    }

    int geti() const
    {
        return i_;
    }
    double getd() const
    {
        return d_;
    }
};

template <typename T, int N>
struct vec
{
    vec() = default;
    vec(std::initializer_list<T> /*il*/)
    {
    }
};

int main()
{
    {  // storage
        using std::experimental::fcv_detail::empty_storage;
        using std::experimental::fcv_detail::trivial_storage;
        using std::experimental::fcv_detail::non_trivial_storage;
        using std::experimental::fcv_detail::storage;

        static_assert(std::is_same<storage<int, 0>, empty_storage<int>>{}, "");
        static_assert(
            std::is_same<storage<int, 10>, trivial_storage<int, 10>>{}, "");
        static_assert(
            std::is_same<storage<std::unique_ptr<int>, 10>,
                         non_trivial_storage<std::unique_ptr<int>, 10>>{},
            "");

        constexpr storage<int, 10> s({1, 2, 3, 4});
        static_assert(s.size() == 4);

        constexpr storage<const int, 10> s2({1, 2, 3, 4});
        static_assert(s2.size() == 4);
    }

    {  // const
        vector<const int, 0> v0 = {};
        test_bounds(v0, 0);

        constexpr vector<const int, 0> vc0 = {};
        test_bounds(vc0, 0);
        static_assert(test_bounds(vc0, 0), "");

        // one and two elements initializer_list don't work
        vector<const int, 1> v1 = {1};
        test_bounds(v1, 1);
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
        for (size_t i = 0; i < c.size(); ++i)
        {
            FCV_ASSERT(*(c.begin() + i) == *(std::addressof(*c.begin()) + i));
        }
    };

    {  // contiguous
        using T = int;
        using C = vector<T, 3>;
        auto e  = C();
        FCV_ASSERT(e.empty());
        test_contiguous(e);
        test_contiguous(C(3, 5));
    }
    {  // default construct element
        using T = int;
        using C = vector<T, 3>;
        C c(1);
        FCV_ASSERT(c.back() == 0);
        FCV_ASSERT(c.front() == 0);
        FCV_ASSERT(c[0] == 0);
    }

    {  // iterator
        using T = int;
        using C = vector<T, 3>;
        C c;
        C::iterator i = c.begin();
        C::iterator j = c.end();
        FCV_ASSERT(std::distance(i, j) == 0);
        FCV_ASSERT(i == j);
    }
    {  // const iterator
        using T = int;
        using C = vector<T, 3>;
        const C c{};
        C::const_iterator i = c.begin();
        C::const_iterator j = c.end();
        FCV_ASSERT(std::distance(i, j) == 0);
        FCV_ASSERT(i == j);
    }
    {  // cbegin/cend
        using T = int;
        using C = vector<T, 3>;
        C c;
        C::const_iterator i = c.cbegin();
        C::const_iterator j = c.cend();
        FCV_ASSERT(std::distance(i, j) == 0);
        FCV_ASSERT(i == j);
        FCV_ASSERT(i == c.end());
    }
    {  // iterator constructor
        using T     = int;
        using C     = vector<T, 10>;
        const T t[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        C c(std::begin(t), std::end(t));
        FCV_ASSERT(
            std::equal(std::begin(t), std::end(t), std::begin(c), std::end(c)));
        C::iterator i = c.begin();
        FCV_ASSERT(*i == 0);
        ++i;
        FCV_ASSERT(*i == 1);
        *i = 10;
        FCV_ASSERT(*i == 10);
        FCV_ASSERT(std::distance(std::begin(c), std::end(c)) == 10);
    }
    {  // N3644 testing
        using C = vector<int, 10>;
        C::iterator ii1{}, ii2{};
        C::iterator ii4 = ii1;
        C::const_iterator cii{};
        FCV_ASSERT(ii1 == ii2);
        FCV_ASSERT(ii1 == ii4);

        FCV_ASSERT(!(ii1 != ii2));

        FCV_ASSERT((ii1 == cii));
        FCV_ASSERT((cii == ii1));
        FCV_ASSERT(!(ii1 != cii));
        FCV_ASSERT(!(cii != ii1));
        FCV_ASSERT(!(ii1 < cii));
        FCV_ASSERT(!(cii < ii1));
        FCV_ASSERT((ii1 <= cii));
        FCV_ASSERT((cii <= ii1));
        FCV_ASSERT(!(ii1 > cii));
        FCV_ASSERT(!(cii > ii1));
        FCV_ASSERT((ii1 >= cii));
        FCV_ASSERT((cii >= ii1));
        FCV_ASSERT((cii - ii1) == 0);
        FCV_ASSERT((ii1 - cii) == 0);
    }

    {  // capacity
        vector<int, 10> a;
        static_assert(a.capacity() == std::size_t(10));
        FCV_ASSERT(a.empty());
        for (std::size_t i = 0; i != 10; ++i)
        {
            a.push_back(0);
        }
        static_assert(a.capacity() == std::size_t(10));
        FCV_ASSERT(a.size() == std::size_t(10));
        FCV_ASSERT(!a.empty());
    }

    {  // resize copyable
        using Copyable = int;
        vector<Copyable, 10> a(std::size_t(10), 5);
        FCV_ASSERT(a.size() == std::size_t(10));
        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
        for (std::size_t i = 0; i != 10; ++i)
        {
            FCV_ASSERT(a[i] == 5);
        }
        a.resize(5);
        FCV_ASSERT(a.size() == std::size_t(5));

        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
        a.resize(9);
        FCV_ASSERT(a[4] == 5);
        for (std::size_t i = 5; i != 9; ++i)
        {
            FCV_ASSERT(a[i] == 0);
        }
        FCV_ASSERT(a.size() == std::size_t(9));
        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
        a.resize(10, 3);
        FCV_ASSERT(a[4] == 5);
        FCV_ASSERT(a[8] == 0);
        FCV_ASSERT(a[9] == 3);
        FCV_ASSERT(a.size() == std::size_t(10));
        static_assert(a.capacity() == std::size_t(10));
        a.resize(5, 2);
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(a[i] == 5);
        }
        test_contiguous(a);
    }
    {  // resize move-only
        using MoveOnly = std::unique_ptr<int>;
        vector<MoveOnly, 10> a(10);
        FCV_ASSERT(a.size() == std::size_t(10));
        static_assert(a.capacity() == std::size_t(10));
        a.resize(5);
        test_contiguous(a);
        FCV_ASSERT(a.size() == std::size_t(5));
        static_assert(a.capacity() == std::size_t(10));
        a.resize(9);
        FCV_ASSERT(a.size() == std::size_t(9));
        static_assert(a.capacity() == std::size_t(10));
    }

    {  // resize value:
        using Copyable = int;
        vector<Copyable, 10> a(std::size_t(10));
        FCV_ASSERT(a.size() == std::size_t(10));
        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
        for (std::size_t i = 0; i != 10; ++i)
        {
            FCV_ASSERT(a[i] == 0);
        }
        a.resize(5);
        FCV_ASSERT(a.size() == std::size_t(5));
        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(a[i] == 0);
        }
        a.resize(9, 5);
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(a[i] == 0);
        }
        for (std::size_t i = 5; i != 9; ++i)
        {
            FCV_ASSERT(a[i] == 5);
        }
        FCV_ASSERT(a.size() == std::size_t(9));
        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
        a.resize(10, 3);
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(a[i] == 0);
        }
        for (std::size_t i = 5; i != 9; ++i)
        {
            FCV_ASSERT(a[i] == 5);
        }
        FCV_ASSERT(a[9] == 3);
        FCV_ASSERT(a.size() == std::size_t(10));
        static_assert(a.capacity() == std::size_t(10));
        test_contiguous(a);
    }

    {  // assign copy
        vector<int, 3> z(3, 5);
        vector<int, 3> a = {0, 1, 2};
        FCV_ASSERT(a.size() == std::size_t{3});
        vector<int, 3> b;
        FCV_ASSERT(b.size() == std::size_t{0});
        b = a;
        FCV_ASSERT(b.size() == std::size_t{3});
        FCV_ASSERT(
            std::equal(std::begin(a), std::end(a), std::begin(b), std::end(b)));
    }

    {  // copy construct
        vector<int, 3> a = {0, 1, 2};
        FCV_ASSERT(a.size() == std::size_t{3});
        vector<int, 3> b(a);
        FCV_ASSERT(b.size() == std::size_t{3});

        FCV_ASSERT(
            std::equal(std::begin(a), std::end(a), std::begin(b), std::end(b)));
    }

    {  // assign move
        using MoveOnly = std::unique_ptr<int>;
        vector<MoveOnly, 3> a(3);
        FCV_ASSERT(a.size() == std::size_t{3});
        vector<MoveOnly, 3> b;
        FCV_ASSERT(b.size() == std::size_t{0});
        b = std::move(a);
        FCV_ASSERT(b.size() == std::size_t{3});
        [[gsl::suppress("misc-use-after-move")]]
        {
            FCV_ASSERT(a.size() == std::size_t{3});
        }
    }

    {  // move construct
        using MoveOnly = std::unique_ptr<int>;
        vector<MoveOnly, 3> a(3);
        FCV_ASSERT(a.size() == std::size_t{3});
        vector<MoveOnly, 3> b(std::move(a));
        FCV_ASSERT(b.size() == std::size_t{3});
        [[gsl::suppress("misc-use-after-move")]]
        {
            FCV_ASSERT(a.size() == std::size_t{3});
        }
    }

    {  // old tests
        using vec_t = vector<int, 5>;
        vec_t vec1(5);
        vec1[0] = 0;
        vec1[1] = 1;
        vec1[2] = 2;
        vec1[3] = 3;
        vec1[4] = 4;
        {
            vec_t vec2;
            vec2.push_back(5);
            vec2.push_back(6);
            vec2.push_back(7);
            vec2.push_back(8);
            vec2.push_back(9);
            FCV_ASSERT(vec1[0] == 0);
            FCV_ASSERT(vec1[4] == 4);
            FCV_ASSERT(vec2[0] == 5);
            FCV_ASSERT(vec2[4] == 9);
        }
        {
            auto vec2 = vec1;
            FCV_ASSERT(vec2[0] == 0);
            FCV_ASSERT(vec2[4] == 4);
            FCV_ASSERT(vec1[0] == 0);
            FCV_ASSERT(vec1[4] == 4);
        }
        {
            int count_ = 0;
            for (auto i : vec1)
            {
                FCV_ASSERT(i == count_);
                count_++;
            }
        }

        {
            std::vector<int> vec2(5);
            vec2[0] = 4;
            vec2[1] = 3;
            vec2[2] = 2;
            vec2[3] = 1;
            vec2[4] = 0;
            vec_t vec(vec2.size());
            copy(std::begin(vec2), std::end(vec2), std::begin(vec));
            int count_ = 4;
            for (auto i : vec)
            {
                FCV_ASSERT(i == count_);
                count_--;
            }
        }
    }
    {
        using vec_t = vector<int, 0>;
        static_assert(sizeof(vec_t) == 1, "");

        constexpr auto a = vec_t{};
        static_assert(a.size() == std::size_t{0}, "");
    }

    {  // back and front:
        using C = vector<int, 2>;
        C c(1);
        FCV_ASSERT(c.back() == 0);
        FCV_ASSERT(c.front() == 0);
        FCV_ASSERT(c[0] == 0);
        c.clear();
        int one = 1;
        c.push_back(one);
        FCV_ASSERT(c.back() == 1);
        FCV_ASSERT(c.front() == 1);
        FCV_ASSERT(c[0] == 1);
        FCV_ASSERT(c.size() == 1);
        c.push_back(2);
        FCV_ASSERT(c.back() == 2);
        FCV_ASSERT(c.front() == 1);
        FCV_ASSERT(c[0] == 1);
        FCV_ASSERT(c[1] == 2);
        FCV_ASSERT(c.size() == 2);
        c.pop_back();
        FCV_ASSERT(c.front() == 1);
        FCV_ASSERT(c[0] == 1);
        FCV_ASSERT(c.back() == 1);
        c.pop_back();
        FCV_ASSERT(c.empty());
    }

    {  // const back:
        using C = vector<int, 2>;
        constexpr C c(1);
        static_assert(c.back() == 0);
        static_assert(c.front() == 0);
        static_assert(c[0] == 0);
        static_assert(c.size() == 1);
    }

    {  // swap: same type
        using C = vector<int, 5>;
        C c0(3, 5);
        C c1(5, 1);
        C c2(0);
        FCV_ASSERT(c0.size() == std::size_t(3));
        FCV_ASSERT(c1.size() == std::size_t(5));
        FCV_ASSERT(c2.size() == std::size_t(0));
        for (std::size_t i = 0; i != 3; ++i)
        {
            FCV_ASSERT(c0[i] == 5);
        }
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(c1[i] == 1);
        }
        c0.swap(c1);
        FCV_ASSERT(c0.size() == std::size_t(5));
        FCV_ASSERT(c1.size() == std::size_t(3));
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(c0[i] == 1);
        }
        for (std::size_t i = 0; i != 3; ++i)
        {
            FCV_ASSERT(c1[i] == 5);
        }
        c2.swap(c1);
        FCV_ASSERT(c1.size() == std::size_t(0));
        FCV_ASSERT(c2.size() == std::size_t(3));
        for (std::size_t i = 0; i != 3; ++i)
        {
            FCV_ASSERT(c2[i] == 5);
        }
    }

    {  // std::swap: same type
        using C = vector<int, 5>;
        C c0(3, 5);
        C c1(5, 1);
        C c2(0);
        FCV_ASSERT(c0.size() == std::size_t(3));
        FCV_ASSERT(c1.size() == std::size_t(5));
        FCV_ASSERT(c2.size() == std::size_t(0));
        for (std::size_t i = 0; i != 3; ++i)
        {
            FCV_ASSERT(c0[i] == 5);
        }
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(c1[i] == 1);
        }
        std::swap(c0, c1);
        FCV_ASSERT(c0.size() == std::size_t(5));
        FCV_ASSERT(c1.size() == std::size_t(3));
        for (std::size_t i = 0; i != 5; ++i)
        {
            FCV_ASSERT(c0[i] == 1);
        }
        for (std::size_t i = 0; i != 3; ++i)
        {
            FCV_ASSERT(c1[i] == 5);
        }
        std::swap(c2, c1);
        FCV_ASSERT(c1.size() == std::size_t(0));
        FCV_ASSERT(c2.size() == std::size_t(3));
        for (std::size_t i = 0; i != 3; ++i)
        {
            FCV_ASSERT(c2[i] == 5);
        }
    }

    {
        constexpr vector<int, 5> v;
        static_assert(v.data() != nullptr);

        constexpr vector<int, 0> v0;
        static_assert(v0.data() == nullptr);
    }

    {// emplace:
     {vector<non_copyable, 3> c;
    vector<non_copyable, 3>::iterator i = c.emplace(c.cbegin(), 2, 3.5);
    FCV_ASSERT(i == c.begin());
    FCV_ASSERT(c.size() == 1);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    i = c.emplace(c.cend(), 3, 4.5);
    FCV_ASSERT(i == c.end() - 1);
    FCV_ASSERT(c.size() == 2);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    FCV_ASSERT(c.back().geti() == 3);
    FCV_ASSERT(c.back().getd() == 4.5);
    i = c.emplace(c.cbegin() + 1, 4, 6.5);
    FCV_ASSERT(i == c.begin() + 1);
    FCV_ASSERT(c.size() == 3);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    FCV_ASSERT(c[1].geti() == 4);
    FCV_ASSERT(c[1].getd() == 6.5);
    FCV_ASSERT(c.back().geti() == 3);
    FCV_ASSERT(c.back().getd() == 4.5);
}
{
    vector<non_copyable, 3> c;
    vector<non_copyable, 3>::iterator i = c.emplace(c.cbegin(), 2, 3.5);
    FCV_ASSERT(i == c.begin());
    FCV_ASSERT(c.size() == 1);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    i = c.emplace(c.cend(), 3, 4.5);
    FCV_ASSERT(i == c.end() - 1);
    FCV_ASSERT(c.size() == 2);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    FCV_ASSERT(c.back().geti() == 3);
    FCV_ASSERT(c.back().getd() == 4.5);
    i = c.emplace(c.cbegin() + 1, 4, 6.5);
    FCV_ASSERT(i == c.begin() + 1);
    FCV_ASSERT(c.size() == 3);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    FCV_ASSERT(c[1].geti() == 4);
    FCV_ASSERT(c[1].getd() == 6.5);
    FCV_ASSERT(c.back().geti() == 3);
    FCV_ASSERT(c.back().getd() == 4.5);
}
}

{// emplace_back
 {vector<non_copyable, 2> c;
c.emplace_back(2, 3.5);
FCV_ASSERT(c.size() == 1);
FCV_ASSERT(c.front().geti() == 2);
FCV_ASSERT(c.front().getd() == 3.5);
c.emplace_back(3, 4.5);
FCV_ASSERT(c.size() == 2);
FCV_ASSERT(c.front().geti() == 2);
FCV_ASSERT(c.front().getd() == 3.5);
FCV_ASSERT(c.back().geti() == 3);
FCV_ASSERT(c.back().getd() == 4.5);
}
{
    vector<non_copyable, 2> c;
    c.emplace_back(2, 3.5);
    FCV_ASSERT(c.size() == 1);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    c.emplace_back(3, 4.5);
    FCV_ASSERT(c.size() == 2);
    FCV_ASSERT(c.front().geti() == 2);
    FCV_ASSERT(c.front().getd() == 3.5);
    FCV_ASSERT(c.back().geti() == 3);
    FCV_ASSERT(c.back().getd() == 4.5);
}
}

{ // emplace extra:
 {//
  vector<int, 4> v;
v = {1, 2, 3};

v.emplace(v.begin(), v.back());
FCV_ASSERT(v[0] == 3);
}
{
    vector<int, 4> v;
    v = {1, 2, 3};
    v.emplace(v.begin(), v.back());
    FCV_ASSERT(v[0] == 3);
}
}

{// erase
 {int a1[] = {1, 2, 3};
vector<int, 4> l1(a1, a1 + 3);
FCV_ASSERT(l1.size() == 3);
vector<int, 4>::const_iterator i = l1.begin();
++i;
vector<int, 4>::iterator j = l1.erase(i);
FCV_ASSERT(l1.size() == 2);
FCV_ASSERT(std::distance(l1.begin(), l1.end()) == 2);
FCV_ASSERT(*j == 3);
FCV_ASSERT(*l1.begin() == 1);
FCV_ASSERT(*std::next(l1.begin()) == 3);
j = l1.erase(j);
FCV_ASSERT(j == l1.end());
FCV_ASSERT(l1.size() == 1);
FCV_ASSERT(std::distance(l1.begin(), l1.end()) == 1);
FCV_ASSERT(*l1.begin() == 1);
j = l1.erase(l1.begin());
FCV_ASSERT(j == l1.end());
FCV_ASSERT(l1.empty());
FCV_ASSERT(std::distance(l1.begin(), l1.end()) == 0);
}
}

{  // erase iter iter
    int a1[]    = {1, 2, 3};
    using vec_t = vector<int, 5>;
    {
        vec_t l1(a1, a1 + 3);
        vec_t::iterator i = l1.erase(l1.cbegin(), l1.cbegin());
        FCV_ASSERT(l1.size() == 3);
        FCV_ASSERT(std::distance(l1.cbegin(), l1.cend()) == 3);
        FCV_ASSERT(i == l1.begin());
    }
    {
        vec_t l1(a1, a1 + 3);
        vec_t::iterator i = l1.erase(l1.cbegin(), std::next(l1.cbegin()));
        FCV_ASSERT(l1.size() == 2);
        FCV_ASSERT(std::distance(l1.cbegin(), l1.cend()) == 2);
        FCV_ASSERT(i == l1.begin());
        FCV_ASSERT(l1 == vec_t(a1 + 1, a1 + 3));
    }
    {
        vec_t l1(a1, a1 + 3);
        vec_t::iterator i = l1.erase(l1.cbegin(), std::next(l1.cbegin(), 2));
        FCV_ASSERT(l1.size() == 1);
        FCV_ASSERT(std::distance(l1.cbegin(), l1.cend()) == 1);
        FCV_ASSERT(i == l1.begin());
        FCV_ASSERT(l1 == vec_t(a1 + 2, a1 + 3));
    }
    {
        vec_t l1(a1, a1 + 3);
        vec_t::iterator i = l1.erase(l1.cbegin(), std::next(l1.cbegin(), 3));
        FCV_ASSERT(l1.empty());
        FCV_ASSERT(std::distance(l1.cbegin(), l1.cend()) == 0);
        FCV_ASSERT(i == l1.begin());
    }
    {
        vector<vec_t, 3> outer(2, vec_t(1));
        outer.erase(outer.begin(), outer.begin());
        FCV_ASSERT(outer.size() == 2);
        FCV_ASSERT(outer[0].size() == 1);
        FCV_ASSERT(outer[1].size() == 1);
    }
}

{// insert init list
 {vector<int, 15> d(10, 1);
vector<int, 15>::iterator i = d.insert(d.cbegin() + 2, {3, 4, 5, 6});
FCV_ASSERT(d.size() == 14);
FCV_ASSERT(i == d.begin() + 2);
FCV_ASSERT(d[0] == 1);
FCV_ASSERT(d[1] == 1);
FCV_ASSERT(d[2] == 3);
FCV_ASSERT(d[3] == 4);
FCV_ASSERT(d[4] == 5);
FCV_ASSERT(d[5] == 6);
FCV_ASSERT(d[6] == 1);
FCV_ASSERT(d[7] == 1);
FCV_ASSERT(d[8] == 1);
FCV_ASSERT(d[9] == 1);
FCV_ASSERT(d[10] == 1);
FCV_ASSERT(d[11] == 1);
FCV_ASSERT(d[12] == 1);
FCV_ASSERT(d[13] == 1);
}
}

{// insert iter iter
 {vector<int, 120> v(100);
int a[]                      = {1, 2, 3, 4, 5};
const std::size_t n          = sizeof(a) / sizeof(a[0]);
vector<int, 120>::iterator i = v.insert(v.cbegin() + 10, (a + 0), (a + n));
FCV_ASSERT(v.size() == 100 + n);
FCV_ASSERT(i == v.begin() + 10);
std::size_t j;
for (j = 0; j < 10; ++j)
{
    FCV_ASSERT(v[j] == 0);
}
for (std::size_t k = 0; k < n; ++j, ++k)
{
    FCV_ASSERT(v[j] == a[k]);
}
for (; j < 105; ++j)
{
    FCV_ASSERT(v[j] == 0);
}
}
[[gsl::suppress("cppcoreguidelines-pro-bounds-pointer-arithmetic")]]
{
    vector<int, 120> v(100);
    size_t sz                    = v.size();
    int a[]                      = {1, 2, 3, 4, 5};
    const unsigned n             = sizeof(a) / sizeof(a[0]);
    vector<int, 120>::iterator i = v.insert(v.cbegin() + 10, (a + 0), (a + n));
    FCV_ASSERT(v.size() == sz + n);
    FCV_ASSERT(i == v.begin() + 10);
    std::size_t j;
    for (j = 0; j < 10; ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
    for (std::size_t k = 0; k < n; ++j, ++k)
    {
        FCV_ASSERT(v[j] == a[k]);
    }
    for (; j < v.size(); ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
}
}

{// insert iter rvalue
 {vector<moint, 103> v(100);
vector<moint, 103>::iterator i = v.insert(v.cbegin() + 10, moint(3));
FCV_ASSERT(v.size() == 101);
FCV_ASSERT(i == v.begin() + 10);
std::size_t j;
for (j = 0; j < 10; ++j)
{
    FCV_ASSERT(v[j] == moint());
}
FCV_ASSERT(v[j] == moint(3));
for (++j; j < 101; ++j)
{
    FCV_ASSERT(v[j] == moint());
}
}
}

{// insert iter size
 {vector<int, 130> v(100);
vector<int, 130>::iterator i = v.insert(v.cbegin() + 10, 5, 1);
FCV_ASSERT(v.size() == 105);
FCV_ASSERT(i == v.begin() + 10);
std::size_t j;
for (j = 0; j < 10; ++j)
{
    FCV_ASSERT(v[j] == 0);
}
for (; j < 15; ++j)
{
    FCV_ASSERT(v[j] == 1);
}
for (++j; j < 105; ++j)
{
    FCV_ASSERT(v[j] == 0);
}
}
{
    vector<int, 130> v(100);
    size_t sz                    = v.size();
    vector<int, 130>::iterator i = v.insert(v.cbegin() + 10, 5, 1);
    FCV_ASSERT(v.size() == sz + 5);
    FCV_ASSERT(i == v.begin() + 10);
    std::size_t j;
    for (j = 0; j < 10; ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
    for (; j < 15; ++j)
    {
        FCV_ASSERT(v[j] == 1);
    }
    for (++j; j < v.size(); ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
}
{
    vector<int, 130> v(100);
    size_t sz                    = v.size();
    vector<int, 130>::iterator i = v.insert(v.cbegin() + 10, 5, 1);
    FCV_ASSERT(v.size() == sz + 5);
    FCV_ASSERT(i == v.begin() + 10);
    std::size_t j;
    for (j = 0; j < 10; ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
    for (; j < 15; ++j)
    {
        FCV_ASSERT(v[j] == 1);
    }
    for (++j; j < v.size(); ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
}
}

{// iter value:
 {vector<int, 130> v(100);
vector<int, 130>::iterator i = v.insert(v.cbegin() + 10, 1);
FCV_ASSERT(v.size() == 101);
FCV_ASSERT(i == v.begin() + 10);
std::size_t j;
for (j = 0; j < 10; ++j)
{
    FCV_ASSERT(v[j] == 0);
}
FCV_ASSERT(v[j] == 1);
for (++j; j < 101; ++j)
{
    FCV_ASSERT(v[j] == 0);
}
}
{
    vector<int, 130> v(100);
    size_t sz                    = v.size();
    vector<int, 130>::iterator i = v.insert(v.cbegin() + 10, 1);
    FCV_ASSERT(v.size() == sz + 1);
    FCV_ASSERT(i == v.begin() + 10);
    std::size_t j;
    for (j = 0; j < 10; ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
    FCV_ASSERT(v[j] == 1);
    for (++j; j < v.size(); ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
}
{
    vector<int, 130> v(100);
    v.pop_back();
    v.pop_back();  // force no reallocation
    size_t sz                    = v.size();
    vector<int, 130>::iterator i = v.insert(v.cbegin() + 10, 1);
    FCV_ASSERT(v.size() == sz + 1);
    FCV_ASSERT(i == v.begin() + 10);
    std::size_t j;
    for (j = 0; j < 10; ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
    FCV_ASSERT(v[j] == 1);
    for (++j; j < v.size(); ++j)
    {
        FCV_ASSERT(v[j] == 0);
    }
}
}

{  // push back move only
    {
        vector<moint, 6> c;
        c.push_back(moint(0));
        FCV_ASSERT(c.size() == 1);
        for (std::size_t j = 0; j < c.size(); ++j)
        {
            FCV_ASSERT(c[j] == moint(j));
        }
        c.push_back(moint(1));
        FCV_ASSERT(c.size() == 2);
        for (std::size_t j = 0; j < c.size(); ++j)
        {
            FCV_ASSERT(c[j] == moint(j));
        }
        c.push_back(moint(2));
        FCV_ASSERT(c.size() == 3);
        for (std::size_t j = 0; j < c.size(); ++j)
        {
            FCV_ASSERT(c[j] == moint(j));
        }
        c.push_back(moint(3));
        FCV_ASSERT(c.size() == 4);
        for (std::size_t j = 0; j < c.size(); ++j)
        {
            FCV_ASSERT(c[j] == moint(j));
        }
        c.push_back(moint(4));
        FCV_ASSERT(c.size() == 5);
        for (std::size_t j = 0; j < c.size(); ++j)
        {
            FCV_ASSERT(c[j] == moint(j));
        }
    }
}

return 0;
}
