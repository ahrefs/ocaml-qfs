// Simple template to wrap C++ object as OCaml custom value
// Copyright (C) 2010, ygrek <ygrek@autistici.org>
// 04/06/2013
//
// value wrapped<Ptr>::alloc(Ptr)
//    creates custom value with pointer to C++ object inside
//    finalizer will release pointer (whether destructor will be called 
//    depends on the semantics of the pointer)
// void wrapped<Ptr>::release(value)
//    releases wrapped pointer
// Ptr const& wrapped<Ptr>::get(value)
//    returns pointer to wrapped object
//    raises OCaml Invalid_argument exception if pointer was already released
// size_t wrapped<Ptr>::count()
//    returns the number of currently allocated Ptr wrappers
//
// wrapped<> manages smart pointers to C++ objects
// wrapped_ptr<> manages raw pointers (owns pointed object, release() destroys object)
//

extern "C" {
#define CAML_NAME_SPACE
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/custom.h>
#include <caml/fail.h>
#include <caml/signals.h>
}

#include <auto_ptr.h>

// name used as identifier for custom_operations
// should be instantiated for each wrapped pointer class
template<class T>
char const* ml_name();

// Ptr is a smart pointer class,
// e.g.: std::auto_ptr, boost::shared_ptr, boost::intrusive_ptr
template<class Ptr>
class wrapped
{
private:
  struct ml_wrapped
  {
    ml_wrapped(Ptr x, size_t t) : tag(t), p(x) {}
    size_t tag;
    Ptr p;
  };

  static size_t count_;

#define Wrapped_val(v) (*(ml_wrapped**)Data_custom_val(v))

  static void finalize(value v)
  {
    release(v);
    delete Wrapped_val(v);
  }

public:
  typedef Ptr type;

  static size_t count() { return count_; }
  static char const* name() { return ml_name<Ptr>(); }
  static size_t tag(value v) { return Wrapped_val(v)->tag; }

  static Ptr const& get(value v) // do not copy
  {
    Ptr const& p = Wrapped_val(v)->p;
    //printf("get %lx : %s\n",(size_t)p.get(),name());
    if (NULL == p.get()) caml_invalid_argument(name());
    return p;
  }

  static void release(value v)
  {
    Ptr& p = Wrapped_val(v)->p;
    //printf("release %lx : %s\n",(size_t)p.get(),name());
    if (NULL == p.get()) return;
    count_--;
    p.reset();
  }

  static value alloc(Ptr p, size_t tag = 0) // copy is ok
  {
    //printf("alloc %lx : %s\n",(size_t)p.get(),name());
    CAMLparam0();
    CAMLlocal1(v);

    static struct custom_operations wrapped_ops = {
      const_cast<char*>(name()),
      finalize,
      custom_compare_default,
      custom_hash_default,
      custom_serialize_default,
      custom_deserialize_default,
#if defined(custom_compare_ext_default)
      custom_compare_ext_default,
#endif
    };

    v = caml_alloc_custom(&wrapped_ops, sizeof(ml_wrapped*), 0, 1);

    ml_wrapped* ml = new ml_wrapped(p,tag); //(ml_wrapped*)caml_stat_alloc(sizeof(ml_wrapped));
    Wrapped_val(v) = ml;

    count_++;

    CAMLreturn(v);
  }

#undef Wrapped_val

}; //wrapped

template<class T>
size_t wrapped<T>::count_ = 0;

template<class T>
struct wrapped_ptr : public wrapped<std::auto_ptr<T> >
{
  typedef wrapped<std::auto_ptr<T> > base;
  static T* get(value v)
  {
    return base::get(v).get();
  }
  static value alloc(T* p, size_t tag = 0)
  {
    return base::alloc(std::auto_ptr<T>(p), tag);
  }
}; // wrapped_ptr

#if defined(__GNUC__)
#define UNUSED __attribute__((unused))
#else
#define UNUSED
#endif

static size_t UNUSED wrapped_tag(value x) { return wrapped_ptr<void>::tag(x); }

class caml_blocking_section // : boost::noncopyable
{
public:
  caml_blocking_section() { caml_enter_blocking_section(); }
  ~caml_blocking_section() { caml_leave_blocking_section(); }
private:
  caml_blocking_section( const caml_blocking_section& );
  const caml_blocking_section& operator=( const caml_blocking_section& );
};

