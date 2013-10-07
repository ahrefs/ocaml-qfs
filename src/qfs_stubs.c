
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
#include <fcntl.h>

/* sync with Unix.open_flag */
static int open_flag_table[] = {
  O_RDONLY, O_WRONLY, O_RDWR, O_NONBLOCK, O_APPEND, O_CREAT, O_TRUNC, O_EXCL,
  O_NOCTTY, O_DSYNC, O_SYNC, O_RSYNC, 0 /* O_SHARE_DELETE */
};

int unix_open_flags(value v_flags)
{
  return caml_convert_flag_list(v_flags, open_flag_table);
}

#define File_val Int_val
#define Val_file Val_int

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

CAMLprim value ml_qfs_release(value v)
{
  ml_client::release(v);
  return Val_unit;
}

CAMLprim value ml_qfs_mkdirs(value v, value v_dir)
{
  int ret = ml_client::get(v)->Mkdirs(String_val(v_dir));
  if (0 != ret)
    unix_error(-ret,"Qfs.mkdirs",v_dir);
  return Val_unit;
}

CAMLprim value ml_qfs_mkdir(value v, value v_dir)
{
  int ret = ml_client::get(v)->Mkdir(String_val(v_dir));
  if (0 != ret)
    unix_error(-ret,"Qfs.mkdir",v_dir);
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
  if (ret < 0)
    unix_error(-ret,"Qfs.create",v_path);
  return Val_file(ret);
}

CAMLprim value ml_qfs_open(value v, value v_path, value v_flags, value v_params)
{
  int flags = unix_open_flags(v_flags);
  int ret = ml_client::get(v)->Open((const char*)String_val(v_path), flags, (const char*)String_val(v_params));
  if (ret < 0)
    unix_error(-ret,"Qfs.open",v_path);
  return Val_file(ret);
}

CAMLprim value ml_qfs_close(value v, value v_file)
{
  int ret = ml_client::get(v)->Close(File_val(v_file));
  if (0 != ret)
    unix_error(-ret,"Qfs.close",Nothing);
  return Val_unit;
}

CAMLprim value ml_qfs_readdir(value v, value v_path)
{
  CAMLparam2(v, v_path);
  CAMLlocal1(v_arr);
  std::vector<std::string> result;
  int ret = 0;

  do {
    std::string path = get_string(v_path);
    KfsClient* p = ml_client::get(v);
    caml_blocking_section lock;
    ret = p->Readdir(path.c_str(), result);
  } while(0);

  if (0 != ret)
    unix_error(-ret,"Qfs.readdir",v_path);
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
    unix_error(-ret,"Qfs.remove",v_path);
  return Val_unit;
}

CAMLprim value ml_qfs_rmdir(value v, value v_path)
{
  int ret = ml_client::get(v)->Rmdir(String_val(v_path));
  if (0 != ret)
    unix_error(-ret,"Qfs.rmdir",v_path);
  return Val_unit;
}

CAMLprim value ml_qfs_sync(value v, value v_file)
{
  int ret = ml_client::get(v)->Sync(File_val(v_file));
  if (0 != ret)
    unix_error(-ret,"Qfs.sync",Nothing);
  return Val_unit;
}

CAMLprim value ml_qfs_rename(value v, value v_old, value v_new, value v_overwrite)
{
  int ret = ml_client::get(v)->Rename(String_val(v_old), String_val(v_new), Bool_val(v_overwrite));
  if (0 != ret)
    unix_error(-ret,"Qfs.rename",v_old); // -1
  return Val_unit;
}

value make_stat(KfsFileAttr const& st)
{
  CAMLparam0();
  CAMLlocal1(v_st);

  v_st = caml_alloc_tuple(4);
  Store_field(v_st, 0, value_of_string(st.filename));
  Store_field(v_st, 1, Val_int(st.fileSize));
  Store_field(v_st, 2, caml_copy_double((double) st.mtime.tv_sec + (double) st.mtime.tv_usec / 1e6));
  Store_field(v_st, 3, Val_bool(st.isDirectory));

  CAMLreturn(v_st);
}

CAMLprim value ml_qfs_stat(value v, value v_path, value v_filesize)
{
  KfsFileAttr st;
  int ret = ml_client::get(v)->Stat(String_val(v_path), st, Bool_val(v_filesize));
  if (0 != ret)
    unix_error(-ret,"Qfs.stat",v_path);
  return make_stat(st);
}

CAMLprim value ml_qfs_fstat(value v, value v_file)
{
  KfsFileAttr st;
  int ret = ml_client::get(v)->Stat(File_val(v_file), st);
  if (0 != ret)
    unix_error(-ret,"Qfs.fstat",Nothing);
  return make_stat(st);
}

CAMLprim value ml_qfs_readdir_plus(value v, value v_path, value v_filesize)
{
  CAMLparam3(v, v_path, v_filesize);
  CAMLlocal1(v_arr);
  std::vector<KfsFileAttr> result;
  int ret = 0;

  do {
    std::string path = get_string(v_path);
    KfsClient* p = ml_client::get(v);
    caml_blocking_section lock;
    ret = p->ReaddirPlus(path.c_str(), result, Bool_val(v_filesize));
  } while (0);

  if (0 != ret)
    unix_error(-ret,"Qfs.readdir_plus",v_path);
  v_arr = caml_alloc_tuple(result.size());
  for (size_t i = 0; i < result.size(); i++)
  {
    Store_field(v_arr, i, make_stat(result[i]));
  }
  CAMLreturn(v_arr);
}

CAMLprim value ml_qfs_read(value v, value v_file, value v_buf, value v_ofs, value v_bytes)
{
  CAMLparam5(v, v_file, v_buf, v_ofs, v_bytes);
  int ret = ml_client::get(v)->Read(File_val(v_file), String_val(v_buf) + Int_val(v_ofs), Int_val(v_bytes));
  if (ret < 0)
    unix_error(-ret,"Qfs.read",Nothing);
  CAMLreturn(Val_int(ret));
}

CAMLprim value ml_qfs_pread(value v, value v_file, value v_pos, value v_buf, value v_ofs, value v_bytes)
{
  CAMLparam5(v, v_file, v_pos, v_buf, v_ofs);
  CAMLxparam1(v_bytes);
  int ret = ml_client::get(v)->PRead(File_val(v_file), Int_val(v_pos), String_val(v_buf) + Int_val(v_ofs), Int_val(v_bytes));
  if (ret < 0)
    unix_error(-ret,"Qfs.pread",Nothing);
  CAMLreturn(Val_int(ret));
}

CAMLprim value ml_qfs_pread_bytecode(value * argv, int argn)
{
  return ml_qfs_pread(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}

CAMLprim value ml_qfs_skip_holes(value v, value v_file)
{
  ml_client::get(v)->SkipHolesInFile(File_val(v_file));
  return Val_unit;
}


CAMLprim value ml_qfs_get_default_iobuffer_size(value v)
{
  return Val_int(ml_client::get(v)->GetDefaultIoBufferSize());
}
CAMLprim value ml_qfs_set_default_iobuffer_size(value v, value v_size)
{
  return Val_int(ml_client::get(v)->SetDefaultIoBufferSize(Int_val(v_size)));
}
CAMLprim value ml_qfs_get_iobuffer_size(value v, value v_file)
{
  return Val_int(ml_client::get(v)->GetIoBufferSize(File_val(v_file)));
}
CAMLprim value ml_qfs_set_iobuffer_size(value v, value v_file, value v_size)
{
  return Val_int(ml_client::get(v)->SetIoBufferSize(File_val(v_file),Int_val(v_size)));
}


CAMLprim value ml_qfs_get_default_readahead_size(value v)
{
  return Val_int(ml_client::get(v)->GetDefaultReadAheadSize());
}
CAMLprim value ml_qfs_set_default_readahead_size(value v, value v_size)
{
  return Val_int(ml_client::get(v)->SetDefaultReadAheadSize(Int_val(v_size)));
}
CAMLprim value ml_qfs_get_readahead_size(value v, value v_file)
{
  return Val_int(ml_client::get(v)->GetReadAheadSize(File_val(v_file)));
}
CAMLprim value ml_qfs_set_readahead_size(value v, value v_file, value v_size)
{
  return Val_int(ml_client::get(v)->SetReadAheadSize(File_val(v_file),Int_val(v_size)));
}

} // extern "C"
