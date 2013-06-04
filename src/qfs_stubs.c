
#include "cxx_wrapped.h"

#include <kfs/KfsClient.h>

using namespace KFS;

typedef wrapped_ptr<KfsClient> ml_client;

template<>
char const* ml_name<ml_client::type>() { return "KfsClient"; }

extern "C" {

#include <caml/callback.h>
#include <caml/signals.h>
#include <caml/unixsupport.h>

#include <memory.h>

/*
#define Val_none Val_int(0)

static value Val_some(value v)
{
    CAMLparam1(v);
    CAMLlocal1(some);
    some = caml_alloc_small(1, 0);
    Field(some, 0) = v;
    CAMLreturn(some);
}

static value Val_pair(value v1, value v2)
{
  CAMLparam2(v1,v2);
  CAMLlocal1(pair);
  pair = caml_alloc_small(2,0);
  Field(pair,0) = v1;
  Field(pair,1) = v2;
  CAMLreturn(pair);
}

static value Val_cons(value list, value v) { return Val_pair(v,list); }

#define Val_car(v) Field(v,0)
#define Val_cdr(v) Field(v,1)

#define Some_val(v) Field(v,0)
*/

static value value_of_string(std::string const& s)
{
  CAMLparam0();
  CAMLlocal1(v);
  v = caml_alloc_string(s.size());
  memcpy(String_val(v), s.c_str(), s.size());
  CAMLreturn(v);
}

/*
static void string_of_value(std::string& s, value v)
{
  s.assign(String_val(v), caml_string_length(v));
}
*/

static std::string get_string(value v)
{
  return std::string(String_val(v), caml_string_length(v));
}

static void raise_error(char const* message) 
{
  static value* exn = NULL;
  if (NULL == exn)
  {
    exn = caml_named_value("Qfs.Error");
    assert(NULL != exn);
  }
  caml_raise_with_string(*exn, message);
}

CAMLprim value ml_qfs_connect(value v_host, value v_port)
{
  std::string const& host = get_string(v_host);
  KfsClient* p = NULL;

  do {
    caml_blocking_section lock;
    p = Connect(host,Int_val(v_port));
  } while (0);

  if (NULL == p) raise_error("connect");

  return ml_client::alloc(p);
}

CAMLprim value ml_qfs_mkdirs(value v, value v_dir)
{
  int ret = ml_client::get(v)->Mkdirs(String_val(v_dir));
  if (0 != ret)
    unix_error(ret,"Qfs.mkdirs",v_dir);
  return Val_unit;
}

CAMLprim value ml_qfs_mkdir(value v, value v_dir)
{
  int ret = ml_client::get(v)->Mkdir(String_val(v_dir));
  if (0 != ret)
    unix_error(ret,"Qfs.mkdir",v_dir);
  return Val_unit;
}

CAMLprim value ml_qfs_exists(value v, value v_path)
{
  return Val_bool(ml_client::get(v)->Exists(String_val(v_path)));
}

CAMLprim value ml_qfs_is_file(value v, value v_path)
{
  return Val_bool(ml_client::get(v)->IsFile(String_val(v_path)));
}

CAMLprim value ml_qfs_is_directory(value v, value v_path)
{
  return Val_bool(ml_client::get(v)->IsDirectory(String_val(v_path)));
}

CAMLprim value ml_qfs_create(value v, value v_path, value v_exclusive, value v_params)
{
  int ret = ml_client::get(v)->Create((const char*)String_val(v_path), (bool)Bool_val(v_exclusive), (const char*)String_val(v_params));
  if (0 != ret)
    unix_error(ret,"Qfs.create",v_path);
  return Val_int(ret);
}

CAMLprim value ml_qfs_close(value v, value v_file)
{
  int ret = ml_client::get(v)->Close(Int_val(v_file));
  if (0 != ret)
    unix_error(ret,"Qfs.close",Nothing);
  return Val_unit;
}

CAMLprim value ml_qfs_readdir(value v, value v_path)
{
  CAMLparam2(v, v_path);
  CAMLlocal1(v_arr);
  std::vector<std::string> result;
  int ret = ml_client::get(v)->Readdir(String_val(v_path), result);
  if (0 != ret)
    unix_error(ret,"Qfs.readdir",v_path);
  v_arr = caml_alloc_tuple(result.size());
  for (size_t i = 0; i < result.size(); i++)
  {
    Store_field(v_arr, i, value_of_string(result[i]));
  }
  CAMLreturn(v_arr);
}

CAMLprim value ml_qfs_remove(value v, value v_path)
{
  int ret = ml_client::get(v)->Remove(String_val(v_path));
  if (0 != ret) // FIXME status code
    unix_error(ret,"Qfs.remove",v_path);
  return Val_unit;
}

CAMLprim value ml_qfs_rmdir(value v, value v_path)
{
  int ret = ml_client::get(v)->Rmdir(String_val(v_path));
  if (0 != ret)
    unix_error(ret,"Qfs.rmdir",v_path);
  return Val_unit;
}

} // extern "C"
