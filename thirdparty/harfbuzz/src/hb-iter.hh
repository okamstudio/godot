/*
 * Copyright © 2018  Google, Inc.
 * Copyright © 2019  Facebook, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 * Facebook Author(s): Behdad Esfahbod
 */

#ifndef HB_ITER_HH
#define HB_ITER_HH

#include "hb.hh"
#include "hb-algs.hh"
#include "hb-meta.hh"


/* Unified iterator object.
 *
 * The goal of this template is to make the same iterator interface
 * available to all types, and make it very easy and compact to use.
 * hb_iter_tator objects are small, light-weight, objects that can be
 * copied by value.  If the collection / object being iterated on
 * is writable, then the iterator returns lvalues, otherwise it
 * returns rvalues.
 *
 * If iterator implementation implements operator!=, then it can be
 * used in range-based for loop.  That already happens if the iterator
 * is random-access.  Otherwise, the range-based for loop incurs
 * one traversal to find end(), which can be avoided if written
 * as a while-style for loop, or if iterator implements a faster
 * __end__() method. */

/*
 * Base classes for iterators.
 */

/* Base class for all iterators. */
template <typename iter_t, typename Item = typename iter_t::__item_t__>
struct hb_iter_t
{
  typedef Item item_t;
  constexpr unsigned get_item_size () const { return hb_static_size (Item); }
  static constexpr bool is_iterator = true;
  static constexpr bool is_random_access_iterator = false;
  static constexpr bool is_sorted_iterator = false;
  static constexpr bool has_fast_len = false; // Should be checked in combination with is_random_access_iterator.

  private:
  /* https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern */
  const iter_t* thiz () const { return static_cast<const iter_t *> (this); }
	iter_t* thiz ()       { return static_cast<      iter_t *> (this); }
  public:

  /* Operators. */
  iter_t iter () const { return *thiz(); }
  iter_t operator + () const { return *thiz(); }
  iter_t _begin () const { return *thiz(); }
  iter_t begin () const { return _begin (); }
  iter_t _end () const { return thiz()->__end__ (); }
  iter_t end () const { return _end (); }
  explicit operator bool () const { return thiz()->__more__ (); }
  unsigned len () const { return thiz()->__len__ (); }
  /* The following can only be enabled if item_t is reference type.  Otherwise
   * it will be returning pointer to temporary rvalue. */
  template <typename T = item_t,
	    hb_enable_if (std::is_reference<T>::value)>
  hb_remove_reference<item_t>* operator -> () const { return std::addressof (**thiz()); }
  item_t operator * () const { return thiz()->__item__ (); }
  item_t operator * () { return thiz()->__item__ (); }
  item_t operator [] (unsigned i) const { return thiz()->__item_at__ (i); }
  item_t operator [] (unsigned i) { return thiz()->__item_at__ (i); }
  iter_t& operator += (unsigned count) &  { thiz()->__forward__ (count); return *thiz(); }
  iter_t  operator += (unsigned count) && { thiz()->__forward__ (count); return *thiz(); }
  iter_t& operator ++ () &  { thiz()->__next__ (); return *thiz(); }
  iter_t  operator ++ () && { thiz()->__next__ (); return *thiz(); }
  iter_t& operator -= (unsigned count) &  { thiz()->__rewind__ (count); return *thiz(); }
  iter_t  operator -= (unsigned count) && { thiz()->__rewind__ (count); return *thiz(); }
  iter_t& operator -- () &  { thiz()->__prev__ (); return *thiz(); }
  iter_t  operator -- () && { thiz()->__prev__ (); return *thiz(); }
  iter_t operator + (unsigned count) const { auto c = thiz()->iter (); c += count; return c; }
  friend iter_t operator + (unsigned count, const iter_t &it) { return it + count; }
  iter_t operator ++ (int) { iter_t c (*thiz()); ++*thiz(); return c; }
  iter_t operator - (unsigned count) const { auto c = thiz()->iter (); c -= count; return c; }
  iter_t operator -- (int) { iter_t c (*thiz()); --*thiz(); return c; }
  template <typename T>
  iter_t& operator >> (T &v) &  { v = **thiz(); ++*thiz(); return *thiz(); }
  template <typename T>
  iter_t  operator >> (T &v) && { v = **thiz(); ++*thiz(); return *thiz(); }
  template <typename T>
  iter_t& operator << (const T v) &  { **thiz() = v; ++*thiz(); return *thiz(); }
  template <typename T>
  iter_t  operator << (const T v) && { **thiz() = v; ++*thiz(); return *thiz(); }

  protected:
  hb_iter_t () = default;
  hb_iter_t (const hb_iter_t &o HB_UNUSED) = default;
  hb_iter_t (hb_iter_t &&o HB_UNUSED) = default;
  hb_iter_t& operator = (const hb_iter_t &o HB_UNUSED) = default;
  hb_iter_t& operator = (hb_iter_t &&o HB_UNUSED) = default;
};

#define HB_ITER_USING(Name) \
  using item_t = typename Name::item_t; \
  using Name::_begin; \
  using Name::begin; \
  using Name::_end; \
  using Name::end; \
  using Name::get_item_size; \
  using Name::is_iterator; \
  using Name::iter; \
  using Name::operator bool; \
  using Name::len; \
  using Name::operator ->; \
  using Name::operator *; \
  using Name::operator []; \
  using Name::operator +=; \
  using Name::operator ++; \
  using Name::operator -=; \
  using Name::operator --; \
  using Name::operator +; \
  using Name::operator -; \
  using Name::operator >>; \
  using Name::operator <<; \
  static_assert (true, "")

/* Returns iterator / item type of a type. */
template <typename Iterable>
using hb_iter_type = decltype (hb_deref (hb_declval (Iterable)).iter ());
template <typename Iterable>
using hb_item_type = decltype (*hb_deref (hb_declval (Iterable)).iter ());


template <typename> struct hb_array_t;
template <typename> struct hb_sorted_array_t;

struct
{
  template <typename T> hb_iter_type<T>
  operator () (T&& c) const
  { return hb_deref (std::forward<T> (c)).iter (); }

  /* Specialization for C arrays. */

  template <typename Type> inline hb_array_t<Type>
  operator () (Type *array, unsigned int length) const
  { return hb_array_t<Type> (array, length); }

  template <typename Type, unsigned int length> hb_array_t<Type>
  operator () (Type (&array)[length]) const
  { return hb_array_t<Type> (array, length); }

}
HB_FUNCOBJ (hb_iter);
struct
{
  template <typename T> auto
  impl (T&& c, hb_priority<1>) const HB_RETURN (unsigned, c.len ())

  template <typename T> auto
  impl (T&& c, hb_priority<0>) const HB_RETURN (unsigned, c.len)

  public:

  template <typename T> auto
  operator () (T&& c) const HB_RETURN (unsigned, impl (std::forward<T> (c), hb_prioritize))
}
HB_FUNCOBJ (hb_len);

/* Mixin to fill in what the subclass doesn't provide. */
template <typename iter_t, typename item_t = typename iter_t::__item_t__>
struct hb_iter_fallback_mixin_t
{
  private:
  /* https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern */
  const iter_t* thiz () const { return static_cast<const iter_t *> (this); }
	iter_t* thiz ()       { return static_cast<      iter_t *> (this); }
  public:

  /* Access: Implement __item__(), or __item_at__() if random-access. */
  item_t __item__ () const { return (*thiz())[0]; }
  item_t __item_at__ (unsigned i) const { return *(*thiz() + i); }

  /* Termination: Implement __more__(), or __len__() if random-access. */
  bool __more__ () const { return bool (thiz()->len ()); }
  unsigned __len__ () const
  { iter_t c (*thiz()); unsigned l = 0; while (c) { c++; l++; } return l; }

  /* Advancing: Implement __next__(), or __forward__() if random-access. */
  void __next__ () { *thiz() += 1; }
  void __forward__ (unsigned n) { while (*thiz() && n--) ++*thiz(); }

  /* Rewinding: Implement __prev__() or __rewind__() if bidirectional. */
  void __prev__ () { *thiz() -= 1; }
  void __rewind__ (unsigned n) { while (*thiz() && n--) --*thiz(); }

  /* Range-based for: Implement __end__() if can be done faster,
   * and operator!=. */
  iter_t __end__ () const
  {
    if (thiz()->is_random_access_iterator)
      return *thiz() + thiz()->len ();
    /* Above expression loops twice. Following loops once. */
    auto it = *thiz();
    while (it) ++it;
    return it;
  }

  protected:
  hb_iter_fallback_mixin_t () = default;
  hb_iter_fallback_mixin_t (const hb_iter_fallback_mixin_t &o HB_UNUSED) = default;
  hb_iter_fallback_mixin_t (hb_iter_fallback_mixin_t &&o HB_UNUSED) = default;
  hb_iter_fallback_mixin_t& operator = (const hb_iter_fallback_mixin_t &o HB_UNUSED) = default;
  hb_iter_fallback_mixin_t& operator = (hb_iter_fallback_mixin_t &&o HB_UNUSED) = default;
};

template <typename iter_t, typename item_t = typename iter_t::__item_t__>
struct hb_iter_with_fallback_t :
  hb_iter_t<iter_t, item_t>,
  hb_iter_fallback_mixin_t<iter_t, item_t>
{
  protected:
  hb_iter_with_fallback_t () = default;
  hb_iter_with_fallback_t (const hb_iter_with_fallback_t &o HB_UNUSED) = default;
  hb_iter_with_fallback_t (hb_iter_with_fallback_t &&o HB_UNUSED) = default;
  hb_iter_with_fallback_t& operator = (const hb_iter_with_fallback_t &o HB_UNUSED) = default;
  hb_iter_with_fallback_t& operator = (hb_iter_with_fallback_t &&o HB_UNUSED) = default;
};

/*
 * Meta-programming predicates.
 */

/* hb_is_iterator() / hb_is_iterator_of() */

template<typename Iter, typename Item>
struct hb_is_iterator_of
{
  template <typename Item2 = Item>
  static hb_true_type impl (hb_priority<2>, hb_iter_t<Iter, hb_type_identity<Item2>> *);
  static hb_false_type impl (hb_priority<0>, const void *);

  public:
  static constexpr bool value = decltype (impl (hb_prioritize, hb_declval (Iter*)))::value;
};
#define hb_is_iterator_of(Iter, Item) hb_is_iterator_of<Iter, Item>::value
#define hb_is_iterator(Iter) hb_is_iterator_of (Iter, typename Iter::item_t)
#define hb_is_sorted_iterator_of(Iter, Item) (hb_is_iterator_of<Iter, Item>::value && Iter::is_sorted_iterator)
#define hb_is_sorted_iterator(Iter) hb_is_sorted_iterator_of (Iter, typename Iter::item_t)

/* hb_is_iterable() */

template <typename T>
struct hb_is_iterable
{
  private:

  template <typename U>
  static auto impl (hb_priority<1>) -> decltype (hb_declval (U).iter (), hb_true_type ());

  template <typename>
  static hb_false_type impl (hb_priority<0>);

  public:
  static constexpr bool value = decltype (impl<T> (hb_prioritize))::value;
};
#define hb_is_iterable(Iterable) hb_is_iterable<Iterable>::value

/* hb_is_source_of() / hb_is_sink_of() */

template<typename Iter, typename Item>
struct hb_is_source_of
{
  private:
  template <typename Iter2 = Iter,
	    hb_enable_if (hb_is_convertible (typename Iter2::item_t, hb_add_lvalue_reference<const Item>))>
  static hb_true_type impl (hb_priority<2>);
  template <typename Iter2 = Iter>
  static auto impl (hb_priority<1>) -> decltype (hb_declval (Iter2) >> hb_declval (Item &), hb_true_type ());
  static hb_false_type impl (hb_priority<0>);

  public:
  static constexpr bool value = decltype (impl (hb_prioritize))::value;
};
#define hb_is_source_of(Iter, Item) hb_is_source_of<Iter, Item>::value

template<typename Iter, typename Item>
struct hb_is_sink_of
{
  private:
  template <typename Iter2 = Iter,
	    hb_enable_if (hb_is_convertible (typename Iter2::item_t, hb_add_lvalue_reference<Item>))>
  static hb_true_type impl (hb_priority<2>);
  template <typename Iter2 = Iter>
  static auto impl (hb_priority<1>) -> decltype (hb_declval (Iter2) << hb_declval (Item), hb_true_type ());
  static hb_false_type impl (hb_priority<0>);

  public:
  static constexpr bool value = decltype (impl (hb_prioritize))::value;
};
#define hb_is_sink_of(Iter, Item) hb_is_sink_of<Iter, Item>::value

/* This is commonly used, so define: */
#define hb_is_sorted_source_of(Iter, Item) \
	(hb_is_source_of(Iter, Item) && Iter::is_sorted_iterator)


struct
{
  template <typename Iterable,
	    hb_requires (hb_is_iterable (Iterable))>
  unsigned operator () (const Iterable &_) const { return hb_len (hb_iter (_)); }

  unsigned operator () (unsigned _) const { return _; }
}
HB_FUNCOBJ (hb_len_of);

/* Range-based 'for' for iterables. */

template <typename Iterable,
	  hb_requires (hb_is_iterable (Iterable))>
static inline auto begin (Iterable&& iterable) HB_AUTO_RETURN (hb_iter (iterable).begin ())

template <typename Iterable,
	  hb_requires (hb_is_iterable (Iterable))>
static inline auto end (Iterable&& iterable) HB_AUTO_RETURN (hb_iter (iterable).end ())

/* begin()/end() are NOT looked up non-ADL.  So each namespace must declare them.
 * Do it for namespace OT. */
namespace OT {

template <typename Iterable,
	  hb_requires (hb_is_iterable (Iterable))>
static inline auto begin (Iterable&& iterable) HB_AUTO_RETURN (hb_iter (iterable).begin ())

template <typename Iterable,
	  hb_requires (hb_is_iterable (Iterable))>
static inline auto end (Iterable&& iterable) HB_AUTO_RETURN (hb_iter (iterable).end ())

}


/*
 * Adaptors, combiners, etc.
 */

template <typename Lhs, typename Rhs,
	  hb_requires (hb_is_iterator (Lhs))>
static inline auto
operator | (Lhs&& lhs, Rhs&& rhs) HB_AUTO_RETURN (std::forward<Rhs> (rhs) (std::forward<Lhs> (lhs)))

/* hb_map(), hb_filter(), hb_reduce() */

enum  class hb_function_sortedness_t {
  NOT_SORTED,
  RETAINS_SORTING,
  SORTED,
};

template <typename Iter, typename Proj, hb_function_sortedness_t Sorted,
	 hb_requires (hb_is_iterator (Iter))>
struct hb_map_iter_t :
  hb_iter_t<hb_map_iter_t<Iter, Proj, Sorted>,
	    decltype (hb_get (hb_declval (Proj), *hb_declval (Iter)))>
{
  hb_map_iter_t (const Iter& it, Proj f_) : it (it), f (f_) {}

  typedef decltype (hb_get (hb_declval (Proj), *hb_declval (Iter))) __item_t__;
  static constexpr bool is_random_access_iterator = Iter::is_random_access_iterator;
  static constexpr bool is_sorted_iterator =
    Sorted == hb_function_sortedness_t::SORTED ? true :
    Sorted == hb_function_sortedness_t::RETAINS_SORTING ? Iter::is_sorted_iterator :
    false;
  __item_t__ __item__ () const { return hb_get (f.get (), *it); }
  __item_t__ __item_at__ (unsigned i) const { return hb_get (f.get (), it[i]); }
  bool __more__ () const { return bool (it); }
  unsigned __len__ () const { return it.len (); }
  void __next__ () { ++it; }
  void __forward__ (unsigned n) { it += n; }
  void __prev__ () { --it; }
  void __rewind__ (unsigned n) { it -= n; }
  hb_map_iter_t __end__ () const { return hb_map_iter_t (it._end (), f); }
  bool operator != (const hb_map_iter_t& o) const
  { return it != o.it; }

  private:
  Iter it;
  mutable hb_reference_wrapper<Proj> f;
};

template <typename Proj, hb_function_sortedness_t Sorted>
struct hb_map_iter_factory_t
{
  hb_map_iter_factory_t (Proj f) : f (f) {}

  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  hb_map_iter_t<Iter, Proj, Sorted>
  operator () (Iter it)
  { return hb_map_iter_t<Iter, Proj, Sorted> (it, f); }

  private:
  Proj f;
};
struct
{
  template <typename Proj>
  hb_map_iter_factory_t<Proj, hb_function_sortedness_t::NOT_SORTED>
  operator () (Proj&& f) const
  { return hb_map_iter_factory_t<Proj, hb_function_sortedness_t::NOT_SORTED> (f); }
}
HB_FUNCOBJ (hb_map);
struct
{
  template <typename Proj>
  hb_map_iter_factory_t<Proj, hb_function_sortedness_t::RETAINS_SORTING>
  operator () (Proj&& f) const
  { return hb_map_iter_factory_t<Proj, hb_function_sortedness_t::RETAINS_SORTING> (f); }
}
HB_FUNCOBJ (hb_map_retains_sorting);
struct
{
  template <typename Proj>
  hb_map_iter_factory_t<Proj, hb_function_sortedness_t::SORTED>
  operator () (Proj&& f) const
  { return hb_map_iter_factory_t<Proj, hb_function_sortedness_t::SORTED> (f); }
}
HB_FUNCOBJ (hb_map_sorted);

template <typename Iter, typename Pred, typename Proj,
	 hb_requires (hb_is_iterator (Iter))>
struct hb_filter_iter_t :
  hb_iter_with_fallback_t<hb_filter_iter_t<Iter, Pred, Proj>,
			  typename Iter::item_t>
{
  hb_filter_iter_t (const Iter& it_, Pred p_, Proj f_) : it (it_), p (p_), f (f_)
  { while (it && !hb_has (p.get (), hb_get (f.get (), *it))) ++it; }

  typedef typename Iter::item_t __item_t__;
  static constexpr bool is_sorted_iterator = Iter::is_sorted_iterator;
  __item_t__ __item__ () const { return *it; }
  bool __more__ () const { return bool (it); }
  void __next__ () { do ++it; while (it && !hb_has (p.get (), hb_get (f.get (), *it))); }
  void __prev__ () { do --it; while (it && !hb_has (p.get (), hb_get (f.get (), *it))); }
  hb_filter_iter_t __end__ () const { return hb_filter_iter_t (it._end (), p, f); }
  bool operator != (const hb_filter_iter_t& o) const
  { return it != o.it; }

  private:
  Iter it;
  mutable hb_reference_wrapper<Pred> p;
  mutable hb_reference_wrapper<Proj> f;
};
template <typename Pred, typename Proj>
struct hb_filter_iter_factory_t
{
  hb_filter_iter_factory_t (Pred p, Proj f) : p (p), f (f) {}

  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  hb_filter_iter_t<Iter, Pred, Proj>
  operator () (Iter it)
  { return hb_filter_iter_t<Iter, Pred, Proj> (it, p, f); }

  private:
  Pred p;
  Proj f;
};
struct
{
  template <typename Pred = decltype ((hb_identity)),
	    typename Proj = decltype ((hb_identity))>
  hb_filter_iter_factory_t<Pred, Proj>
  operator () (Pred&& p = hb_identity, Proj&& f = hb_identity) const
  { return hb_filter_iter_factory_t<Pred, Proj> (p, f); }
}
HB_FUNCOBJ (hb_filter);

template <typename Redu, typename InitT>
struct hb_reduce_t
{
  hb_reduce_t (Redu r, InitT init_value) : r (r), init_value (init_value) {}

  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter)),
	    typename AccuT = hb_decay<decltype (hb_declval (Redu) (hb_declval (InitT), hb_declval (typename Iter::item_t)))>>
  AccuT
  operator () (Iter it)
  {
    AccuT value = init_value;
    for (; it; ++it)
      value = r (value, *it);
    return value;
  }

  private:
  Redu r;
  InitT init_value;
};
struct
{
  template <typename Redu, typename InitT>
  hb_reduce_t<Redu, InitT>
  operator () (Redu&& r, InitT init_value) const
  { return hb_reduce_t<Redu, InitT> (r, init_value); }
}
HB_FUNCOBJ (hb_reduce);


/* hb_zip() */

template <typename A, typename B>
struct hb_zip_iter_t :
  hb_iter_t<hb_zip_iter_t<A, B>,
	    hb_pair_t<typename A::item_t, typename B::item_t>>
{
  hb_zip_iter_t () {}
  hb_zip_iter_t (const A& a, const B& b) : a (a), b (b) {}

  typedef hb_pair_t<typename A::item_t, typename B::item_t> __item_t__;
  static constexpr bool is_random_access_iterator =
    A::is_random_access_iterator &&
    B::is_random_access_iterator;
  /* Note.  The following categorization is only valid if A is strictly sorted,
   * ie. does NOT have duplicates.  Previously I tried to categorize sortedness
   * more granularly, see commits:
   *
   *   513762849a683914fc266a17ddf38f133cccf072
   *   4d3cf2adb669c345cc43832d11689271995e160a
   *
   * However, that was not enough, since hb_sorted_array_t, hb_sorted_vector_t,
   * SortedArrayOf, etc all needed to be updated to add more variants.  At that
   * point I saw it not worth the effort, and instead we now deem all sorted
   * collections as essentially strictly-sorted for the purposes of zip.
   *
   * The above assumption is not as bad as it sounds.  Our "sorted" comes with
   * no guarantees.  It's just a contract, put in place to help you remember,
   * and think about, whether an iterator you receive is expected to be
   * sorted or not.  As such, it's not perfect by definition, and should not
   * be treated so.  The inaccuracy here just errs in the direction of being
   * more permissive, so your code compiles instead of erring on the side of
   * marking your zipped iterator unsorted in which case your code won't
   * compile.
   *
   * This semantical limitation does NOT affect logic in any other place I
   * know of as of this writing.
   */
  static constexpr bool is_sorted_iterator = A::is_sorted_iterator;

  __item_t__ __item__ () const { return __item_t__ (*a, *b); }
  __item_t__ __item_at__ (unsigned i) const { return __item_t__ (a[i], b[i]); }
  bool __more__ () const { return bool (a) && bool (b); }
  unsigned __len__ () const { return hb_min (a.len (), b.len ()); }
  void __next__ () { ++a; ++b; }
  void __forward__ (unsigned n) { a += n; b += n; }
  void __prev__ () { --a; --b; }
  void __rewind__ (unsigned n) { a -= n; b -= n; }
  hb_zip_iter_t __end__ () const { return hb_zip_iter_t (a._end (), b._end ()); }
  /* Note, we should stop if ANY of the iters reaches end.  As such two compare
   * unequal if both items are unequal, NOT if either is unequal. */
  bool operator != (const hb_zip_iter_t& o) const
  { return a != o.a && b != o.b; }

  private:
  A a;
  B b;
};
struct
{ HB_PARTIALIZE(2);
  template <typename A, typename B,
	    hb_requires (hb_is_iterable (A) && hb_is_iterable (B))>
  hb_zip_iter_t<hb_iter_type<A>, hb_iter_type<B>>
  operator () (A&& a, B&& b) const
  { return hb_zip_iter_t<hb_iter_type<A>, hb_iter_type<B>> (hb_iter (a), hb_iter (b)); }
}
HB_FUNCOBJ (hb_zip);

/* hb_concat() */

template <typename A, typename B>
struct hb_concat_iter_t :
    hb_iter_t<hb_concat_iter_t<A, B>, typename A::item_t>
{
  hb_concat_iter_t () {}
  hb_concat_iter_t (A& a, B& b) : a (a), b (b) {}
  hb_concat_iter_t (const A& a, const B& b) : a (a), b (b) {}


  typedef typename A::item_t __item_t__;
  static constexpr bool is_random_access_iterator =
    A::is_random_access_iterator &&
    B::is_random_access_iterator;
  static constexpr bool is_sorted_iterator = false;

  __item_t__ __item__ () const
  {
    if (!a)
      return *b;
    return *a;
  }

  __item_t__ __item_at__ (unsigned i) const
  {
    unsigned a_len = a.len ();
    if (i < a_len)
      return a[i];
    return b[i - a_len];
  }

  bool __more__ () const { return bool (a) || bool (b); }

  unsigned __len__ () const { return a.len () + b.len (); }

  void __next__ ()
  {
    if (a)
      ++a;
    else
      ++b;
  }

  void __forward__ (unsigned n)
  {
    if (!n) return;
    if (!is_random_access_iterator) {
      while (n-- && *this) {
        (*this)++;
      }
      return;
    }

    unsigned a_len = a.len ();
    if (n > a_len) {
      n -= a_len;
      a.__forward__ (a_len);
      b.__forward__ (n);
    } else {
      a.__forward__ (n);
    }
  }

  hb_concat_iter_t __end__ () const { return hb_concat_iter_t (a._end (), b._end ()); }
  bool operator != (const hb_concat_iter_t& o) const
  {
    return a != o.a
        || b != o.b;
  }

  private:
  A a;
  B b;
};
struct
{ HB_PARTIALIZE(2);
  template <typename A, typename B,
	    hb_requires (hb_is_iterable (A) && hb_is_iterable (B))>
  hb_concat_iter_t<hb_iter_type<A>, hb_iter_type<B>>
  operator () (A&& a, B&& b) const
  { return hb_concat_iter_t<hb_iter_type<A>, hb_iter_type<B>> (hb_iter (a), hb_iter (b)); }
}
HB_FUNCOBJ (hb_concat);

/* hb_apply() */

template <typename Appl>
struct hb_apply_t
{
  hb_apply_t (Appl a) : a (a) {}

  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  void operator () (Iter it)
  {
    for (; it; ++it)
      (void) hb_invoke (a, *it);
  }

  private:
  Appl a;
};
struct
{
  template <typename Appl> hb_apply_t<Appl>
  operator () (Appl&& a) const
  { return hb_apply_t<Appl> (a); }

  template <typename Appl> hb_apply_t<Appl&>
  operator () (Appl *a) const
  { return hb_apply_t<Appl&> (*a); }
}
HB_FUNCOBJ (hb_apply);

/* hb_range()/hb_iota()/hb_repeat() */

template <typename T, typename S>
struct hb_range_iter_t :
  hb_iter_t<hb_range_iter_t<T, S>, T>
{
  hb_range_iter_t (T start, T end_, S step) : v (start), end_ (end_for (start, end_, step)), step (step) {}

  typedef T __item_t__;
  static constexpr bool is_random_access_iterator = true;
  static constexpr bool is_sorted_iterator = true;
  __item_t__ __item__ () const { return hb_ridentity (v); }
  __item_t__ __item_at__ (unsigned j) const { return v + j * step; }
  bool __more__ () const { return v != end_; }
  unsigned __len__ () const { return !step ? UINT_MAX : (end_ - v) / step; }
  void __next__ () { v += step; }
  void __forward__ (unsigned n) { v += n * step; }
  void __prev__ () { v -= step; }
  void __rewind__ (unsigned n) { v -= n * step; }
  hb_range_iter_t __end__ () const { return hb_range_iter_t (end_, end_, step); }
  bool operator != (const hb_range_iter_t& o) const
  { return v != o.v; }

  private:
  static inline T end_for (T start, T end_, S step)
  {
    if (!step)
      return end_;
    auto res = (end_ - start) % step;
    if (!res)
      return end_;
    end_ += step - res;
    return end_;
  }

  private:
  T v;
  T end_;
  S step;
};
struct
{
  template <typename T = unsigned> hb_range_iter_t<T, unsigned>
  operator () (T end = (unsigned) -1) const
  { return hb_range_iter_t<T, unsigned> (0, end, 1u); }

  template <typename T, typename S = unsigned> hb_range_iter_t<T, S>
  operator () (T start, T end, S step = 1u) const
  { return hb_range_iter_t<T, S> (start, end, step); }
}
HB_FUNCOBJ (hb_range);

template <typename T, typename S>
struct hb_iota_iter_t :
  hb_iter_with_fallback_t<hb_iota_iter_t<T, S>, T>
{
  hb_iota_iter_t (T start, S step) : v (start), step (step) {}

  private:

  template <typename S2 = S>
  auto
  inc (hb_type_identity<S2> s, hb_priority<1>)
    -> hb_void_t<decltype (hb_invoke (std::forward<S2> (s), hb_declval<T&> ()))>
  { v = hb_invoke (std::forward<S2> (s), v); }

  void
  inc (S s, hb_priority<0>)
  { v += s; }

  public:

  typedef T __item_t__;
  static constexpr bool is_random_access_iterator = true;
  static constexpr bool is_sorted_iterator = true;
  __item_t__ __item__ () const { return hb_ridentity (v); }
  bool __more__ () const { return true; }
  unsigned __len__ () const { return UINT_MAX; }
  void __next__ () { inc (step, hb_prioritize); }
  void __prev__ () { v -= step; }
  hb_iota_iter_t __end__ () const { return *this; }
  bool operator != (const hb_iota_iter_t& o) const { return true; }

  private:
  T v;
  S step;
};
struct
{
  template <typename T = unsigned, typename S = unsigned> hb_iota_iter_t<T, S>
  operator () (T start = 0u, S step = 1u) const
  { return hb_iota_iter_t<T, S> (start, step); }
}
HB_FUNCOBJ (hb_iota);

template <typename T>
struct hb_repeat_iter_t :
  hb_iter_t<hb_repeat_iter_t<T>, T>
{
  hb_repeat_iter_t (T value) : v (value) {}

  typedef T __item_t__;
  static constexpr bool is_random_access_iterator = true;
  static constexpr bool is_sorted_iterator = true;
  __item_t__ __item__ () const { return v; }
  __item_t__ __item_at__ (unsigned j) const { return v; }
  bool __more__ () const { return true; }
  unsigned __len__ () const { return UINT_MAX; }
  void __next__ () {}
  void __forward__ (unsigned) {}
  void __prev__ () {}
  void __rewind__ (unsigned) {}
  hb_repeat_iter_t __end__ () const { return *this; }
  bool operator != (const hb_repeat_iter_t& o) const { return true; }

  private:
  T v;
};
struct
{
  template <typename T> hb_repeat_iter_t<T>
  operator () (T value) const
  { return hb_repeat_iter_t<T> (value); }
}
HB_FUNCOBJ (hb_repeat);

/* hb_enumerate()/hb_take() */

struct
{
  template <typename Iterable,
	    typename Index = unsigned,
	    hb_requires (hb_is_iterable (Iterable))>
  auto operator () (Iterable&& it, Index start = 0u) const HB_AUTO_RETURN
  ( hb_zip (hb_iota (start), it) )
}
HB_FUNCOBJ (hb_enumerate);

struct
{ HB_PARTIALIZE(2);
  template <typename Iterable,
	    hb_requires (hb_is_iterable (Iterable))>
  auto operator () (Iterable&& it, unsigned count) const HB_AUTO_RETURN
  ( hb_zip (hb_range (count), it) | hb_map_retains_sorting (hb_second) )

  /* Specialization arrays. */

  template <typename Type> inline hb_array_t<Type>
  operator () (hb_array_t<Type> array, unsigned count) const
  { return array.sub_array (0, count); }

  template <typename Type> inline hb_sorted_array_t<Type>
  operator () (hb_sorted_array_t<Type> array, unsigned count) const
  { return array.sub_array (0, count); }
}
HB_FUNCOBJ (hb_take);

struct
{ HB_PARTIALIZE(2);
  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  auto operator () (Iter it, unsigned count) const HB_AUTO_RETURN
  (
    + hb_iota (it, hb_add (count))
    | hb_map (hb_take (count))
    | hb_take ((hb_len (it) + count - 1) / count)
  )
}
HB_FUNCOBJ (hb_chop);

/* hb_sink() */

template <typename Sink>
struct hb_sink_t
{
  hb_sink_t (Sink s) : s (s) {}

  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  void operator () (Iter it)
  {
    for (; it; ++it)
      s << *it;
  }

  private:
  Sink s;
};
struct
{
  template <typename Sink> hb_sink_t<Sink>
  operator () (Sink&& s) const
  { return hb_sink_t<Sink> (s); }

  template <typename Sink> hb_sink_t<Sink&>
  operator () (Sink *s) const
  { return hb_sink_t<Sink&> (*s); }
}
HB_FUNCOBJ (hb_sink);

/* hb-drain: hb_sink to void / blackhole / /dev/null. */

struct
{
  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  void operator () (Iter it) const
  {
    for (; it; ++it)
      (void) *it;
  }
}
HB_FUNCOBJ (hb_drain);

/* hb_unzip(): unzip and sink to two sinks. */

template <typename Sink1, typename Sink2>
struct hb_unzip_t
{
  hb_unzip_t (Sink1 s1, Sink2 s2) : s1 (s1), s2 (s2) {}

  template <typename Iter,
	    hb_requires (hb_is_iterator (Iter))>
  void operator () (Iter it)
  {
    for (; it; ++it)
    {
      const auto &v = *it;
      s1 << v.first;
      s2 << v.second;
    }
  }

  private:
  Sink1 s1;
  Sink2 s2;
};
struct
{
  template <typename Sink1, typename Sink2> hb_unzip_t<Sink1, Sink2>
  operator () (Sink1&& s1, Sink2&& s2) const
  { return hb_unzip_t<Sink1, Sink2> (s1, s2); }

  template <typename Sink1, typename Sink2> hb_unzip_t<Sink1&, Sink2&>
  operator () (Sink1 *s1, Sink2 *s2) const
  { return hb_unzip_t<Sink1&, Sink2&> (*s1, *s2); }
}
HB_FUNCOBJ (hb_unzip);


/* hb-all, hb-any, hb-none. */

struct
{
  template <typename Iterable,
	    typename Pred = decltype ((hb_identity)),
	    typename Proj = decltype ((hb_identity)),
	    hb_requires (hb_is_iterable (Iterable))>
  bool operator () (Iterable&& c,
		    Pred&& p = hb_identity,
		    Proj&& f = hb_identity) const
  {
    for (auto it = hb_iter (c); it; ++it)
      if (!hb_match (std::forward<Pred> (p), hb_get (std::forward<Proj> (f), *it)))
	return false;
    return true;
  }
}
HB_FUNCOBJ (hb_all);
struct
{
  template <typename Iterable,
	    typename Pred = decltype ((hb_identity)),
	    typename Proj = decltype ((hb_identity)),
	    hb_requires (hb_is_iterable (Iterable))>
  bool operator () (Iterable&& c,
		    Pred&& p = hb_identity,
		    Proj&& f = hb_identity) const
  {
    for (auto it = hb_iter (c); it; ++it)
      if (hb_match (std::forward<Pred> (p), hb_get (std::forward<Proj> (f), *it)))
	return true;
    return false;
  }
}
HB_FUNCOBJ (hb_any);
struct
{
  template <typename Iterable,
	    typename Pred = decltype ((hb_identity)),
	    typename Proj = decltype ((hb_identity)),
	    hb_requires (hb_is_iterable (Iterable))>
  bool operator () (Iterable&& c,
		    Pred&& p = hb_identity,
		    Proj&& f = hb_identity) const
  {
    for (auto it = hb_iter (c); it; ++it)
      if (hb_match (std::forward<Pred> (p), hb_get (std::forward<Proj> (f), *it)))
	return false;
    return true;
  }
}
HB_FUNCOBJ (hb_none);

/*
 * Algorithms operating on iterators.
 */

template <typename C, typename V,
	  hb_requires (hb_is_iterable (C))>
inline void
hb_fill (C&& c, const V &v)
{
  for (auto i = hb_iter (c); i; i++)
    *i = v;
}

template <typename S, typename D>
inline void
hb_copy (S&& is, D&& id)
{
  hb_iter (is) | hb_sink (id);
}


#endif /* HB_ITER_HH */
