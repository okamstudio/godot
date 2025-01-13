// Copyright 2024 The Manifold Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once

#include <iterator>
#include <type_traits>

namespace manifold {

template <typename F, typename Iter>
struct TransformIterator {
 private:
  Iter iter;
  F f;

 public:
  using pointer = void;
  using reference = std::invoke_result_t<
      F, typename std::iterator_traits<std::remove_const_t<Iter>>::value_type>;
  using difference_type =
      typename std::iterator_traits<std::remove_const_t<Iter>>::difference_type;
  using value_type = reference;
  using iterator_category = typename std::iterator_traits<
      std::remove_const_t<Iter>>::iterator_category;

  constexpr TransformIterator(Iter iter, F f) : iter(iter), f(f) {}

  TransformIterator& operator=(const TransformIterator& other) {
    if (this == &other) return *this;
    // don't copy function, should be the same
    iter = other.iter;
    return *this;
  }

  constexpr reference operator*() const { return f(*iter); }

  constexpr reference operator[](size_t i) const { return f(iter[i]); }

  // prefix increment
  TransformIterator& operator++() {
    iter += 1;
    return *this;
  }

  // postfix
  TransformIterator operator++(int) {
    auto old = *this;
    operator++();
    return old;
  }

  // prefix increment
  TransformIterator& operator--() {
    iter -= 1;
    return *this;
  }

  // postfix
  TransformIterator operator--(int) {
    auto old = *this;
    operator--();
    return old;
  }

  constexpr TransformIterator operator+(size_t n) const {
    return TransformIterator(iter + n, f);
  }

  TransformIterator& operator+=(size_t n) {
    iter += n;
    return *this;
  }

  constexpr TransformIterator operator-(size_t n) const {
    return TransformIterator(iter - n, f);
  }

  TransformIterator& operator-=(size_t n) {
    iter -= n;
    return *this;
  }

  constexpr bool operator==(TransformIterator other) const {
    return iter == other.iter;
  }

  constexpr bool operator!=(TransformIterator other) const {
    return !(iter == other.iter);
  }

  constexpr bool operator<(TransformIterator other) const {
    return iter < other.iter;
  }

  constexpr difference_type operator-(TransformIterator other) const {
    return iter - other.iter;
  }

  constexpr operator TransformIterator<F, const Iter>() const {
    return TransformIterator(f, iter);
  }
};

template <typename T>
struct CountingIterator {
 private:
  T counter;

 public:
  using pointer = void;
  using reference = T;
  using difference_type = std::make_signed_t<T>;
  using value_type = T;
  using iterator_category = std::random_access_iterator_tag;

  constexpr CountingIterator(T counter) : counter(counter) {}

  constexpr value_type operator*() const { return counter; }
  constexpr value_type operator[](T i) const { return counter + i; }

  // prefix increment
  CountingIterator& operator++() {
    counter += 1;
    return *this;
  }

  // postfix
  CountingIterator operator++(int) {
    auto old = *this;
    operator++();
    return old;
  }

  // prefix increment
  CountingIterator& operator--() {
    counter -= 1;
    return *this;
  }

  // postfix
  CountingIterator operator--(int) {
    auto old = *this;
    operator--();
    return old;
  }

  constexpr CountingIterator operator+(T n) const {
    return CountingIterator(counter + n);
  }

  CountingIterator& operator+=(T n) {
    counter += n;
    return *this;
  }

  constexpr CountingIterator operator-(T n) const {
    return CountingIterator(counter - n);
  }

  CountingIterator& operator-=(T n) {
    counter -= n;
    return *this;
  }

  constexpr friend bool operator==(CountingIterator a, CountingIterator b) {
    return a.counter == b.counter;
  }

  constexpr friend bool operator!=(CountingIterator a, CountingIterator b) {
    return a.counter != b.counter;
  }

  constexpr friend bool operator<(CountingIterator a, CountingIterator b) {
    return a.counter < b.counter;
  }

  constexpr friend difference_type operator-(CountingIterator a,
                                             CountingIterator b) {
    return a.counter - b.counter;
  }

  constexpr operator CountingIterator<const T>() const {
    return CountingIterator(counter);
  }
};

constexpr CountingIterator<size_t> countAt(size_t i) {
  return CountingIterator(i);
}

template <typename Iter>
struct StridedRange {
 private:
  struct StridedRangeIter {
   private:
    Iter iter;
    size_t stride;

   public:
    using pointer =
        typename std::iterator_traits<std::remove_const_t<Iter>>::pointer;
    using reference =
        typename std::iterator_traits<std::remove_const_t<Iter>>::reference;
    using difference_type = typename std::iterator_traits<
        std::remove_const_t<Iter>>::difference_type;
    using value_type =
        typename std::iterator_traits<std::remove_const_t<Iter>>::value_type;
    using iterator_category = typename std::iterator_traits<
        std::remove_const_t<Iter>>::iterator_category;

    constexpr StridedRangeIter(Iter iter, int stride)
        : iter(iter), stride(stride) {}

    constexpr reference operator*() { return *iter; }

    constexpr std::add_const_t<reference> operator*() const { return *iter; }

    constexpr reference operator[](size_t i) { return iter[i * stride]; }

    constexpr std::add_const_t<reference> operator[](size_t i) const {
      return iter[i * stride];
    }

    // prefix increment
    StridedRangeIter& operator++() {
      iter += stride;
      return *this;
    }

    // postfix
    StridedRangeIter operator++(int) {
      auto old = *this;
      operator++();
      return old;
    }

    // prefix increment
    StridedRangeIter& operator--() {
      iter -= stride;
      return *this;
    }

    // postfix
    StridedRangeIter operator--(int) {
      auto old = *this;
      operator--();
      return old;
    }

    constexpr StridedRangeIter operator+(size_t n) const {
      return StridedRangeIter(iter + n * stride, stride);
    }

    StridedRangeIter& operator+=(size_t n) {
      iter += n * stride;
      return *this;
    }

    constexpr StridedRangeIter operator-(size_t n) const {
      return StridedRangeIter(iter - n * stride, stride);
    }

    StridedRangeIter& operator-=(size_t n) {
      iter -= n * stride;
      return *this;
    }

    constexpr friend bool operator==(StridedRangeIter a, StridedRangeIter b) {
      return a.iter == b.iter;
    }

    constexpr friend bool operator!=(StridedRangeIter a, StridedRangeIter b) {
      return !(a.iter == b.iter);
    }

    constexpr friend bool operator<(StridedRangeIter a, StridedRangeIter b) {
      return a.iter < b.iter;
    }

    constexpr friend difference_type operator-(StridedRangeIter a,
                                               StridedRangeIter b) {
      // note that this is not well-defined if a.stride != b.stride...
      return (a.iter - b.iter) / a.stride;
    }
  };
  Iter _start, _end;
  const size_t stride;

 public:
  constexpr StridedRange(Iter start, Iter end, size_t stride)
      : _start(start), _end(end), stride(stride) {}

  constexpr StridedRangeIter begin() const {
    return StridedRangeIter(_start, stride);
  }

  constexpr StridedRangeIter end() const {
    return StridedRangeIter(_start, stride) +
           ((std::distance(_start, _end) + (stride - 1)) / stride);
  }
};

}  // namespace manifold
