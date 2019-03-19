
#include "cxx_wrapped.h"
#include <boost/smart_ptr.hpp>
#include <kfs/KfsClient.h>

using namespace KFS;

typedef wrapped<boost::shared_ptr<KfsClient> > ml_client;

template<>
char const* ml_name<ml_client::type>() { return "KfsClient"; }

extern "C" {

#include <caml/callback.h>
#include <caml/signals.h>
#include <caml/unixsupport.h>

#include <memory.h>
#include <fcntl.h>

// from extunix
#ifndef O_NONBLOCK
#ifdef __MINGW32__
#define O_NONBLOCK 0 /* no O_NONBLOCK on mingw */
#else
#define O_NONBLOCK O_NDELAY
#endif
#endif
#ifndef O_DSYNC
#define O_DSYNC 0
#endif
#ifndef O_SYNC
#define O_SYNC 0
#endif
#ifndef O_RSYNC
#define O_RSYNC O_SYNC
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef O_KEEPEXEC
#define O_KEEPEXEC 0
#endif

} // extern "C"

/* sync with Unix.open_flag */
static int open_flag_table[] = {
  O_RDONLY, O_WRONLY, O_RDWR, O_NONBLOCK, O_APPEND, O_CREAT, O_TRUNC, O_EXCL,
  O_NOCTTY, O_DSYNC, O_SYNC, O_RSYNC,
  0, /* O_SHARE_DELETE */
  O_CLOEXEC,
  O_KEEPEXEC
};

static int unix_open_flags(value v_flags)
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

#define C_STR(v) (const char*)(get_string(v).c_str())

template<typename F, typename... Args>
decltype(auto) with_qfs(value v_client, F f, Args&&... args)
{
  ml_client::type p = ml_client::get(v_client);
  caml_blocking_section lock;
  return (p.get()->*f)(std::forward<Args>(args)...);
}

template<int (KfsClient::*mf)(char const*)>
value with_qfs_path(const char* name, value v_client, value v_path)
{
  CAMLparam2(v_client,v_path);
  int ret = with_qfs(v_client, mf, C_STR(v_path));
  if (0 != ret)
    unix_error(-ret,name,v_path);
  CAMLreturn(Val_unit);
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

extern "C"
{

value ml_qfs_connect(value v_host, value v_port)
{
  std::string const& host = get_string(v_host);
  boost::shared_ptr<KfsClient> p;

  do {
    caml_blocking_section lock;
    p.reset(Connect(host,Int_val(v_port)));
  } while (0);

  if (!p) raise_error("connect");

  return ml_client::alloc(p);
}

value ml_qfs_release(value v)
{
  ml_client::release(v);
  return Val_unit;
}

value ml_qfs_mkdirs(value v, value v_dir)
{
  CAMLparam2(v,v_dir);
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, kfsMode_t)>(&KfsClient::Mkdirs),
                     C_STR(v_dir), 0777); /* because default arguments are not handled by "perfect" forwarding */
  if (0 != ret)
    unix_error(-ret,"Qfs.mkdirs",v_dir);
  CAMLreturn(Val_unit);
}

value ml_qfs_mkdir(value v, value v_dir)
{
  CAMLparam2(v, v_dir);
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, kfsMode_t)>(&KfsClient::Mkdir),
                     C_STR(v_dir), 0777);
  if (0 != ret)
    unix_error(-ret,"Qfs.mkdir",v_dir);
  return Val_unit;
}

value ml_qfs_exists(value v, value v_path)
{
  return Val_bool(with_qfs(v, &KfsClient::Exists, C_STR(v_path)));
}

value ml_qfs_is_file(value v, value v_path)
{
  return Val_bool(with_qfs(v, &KfsClient::IsFile, C_STR(v_path)));
}

value ml_qfs_is_directory(value v, value v_path)
{
  return Val_bool(with_qfs(v, &KfsClient::IsDirectory, C_STR(v_path)));
}

value ml_qfs_create(value v, value v_path, value v_exclusive, value v_params)
{
  CAMLparam4(v,v_path,v_exclusive,v_params);
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, bool, const char*)>(&KfsClient::Create),
                     C_STR(v_path), Bool_val(v_exclusive), C_STR(v_params));
  if (ret < 0)
    unix_error(-ret,"Qfs.create",v_path);
  CAMLreturn(Val_file(ret));
}

value ml_qfs_open(value v, value v_path, value v_flags, value v_params)
{
  CAMLparam4(v,v_path,v_flags,v_params);
  int flags = unix_open_flags(v_flags);
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, int, const char*, kfsMode_t)>(&KfsClient::Open),
                     C_STR(v_path), flags, C_STR(v_params), 0666);
  if (ret < 0)
    unix_error(-ret,"Qfs.open",v_path);
  CAMLreturn(Val_file(ret));
}

value ml_qfs_close(value v, value v_file)
{
  int ret = with_qfs(v, &KfsClient::Close, File_val(v_file));
  if (0 != ret)
    unix_error(-ret,"Qfs.close",Nothing);
  return Val_unit;
}

value ml_qfs_readdir(value v, value v_path)
{
  CAMLparam2(v, v_path);
  CAMLlocal1(v_arr);
  std::vector<std::string> result;
  int ret = with_qfs(v, &KfsClient::Readdir, C_STR(v_path), result);
  if (0 != ret)
    unix_error(-ret,"Qfs.readdir",v_path);
  v_arr = caml_alloc_tuple(result.size());
  for (size_t i = 0; i < result.size(); i++)
  {
    Store_field(v_arr, i, value_of_string(result[i]));
  }
  CAMLreturn(v_arr);
}

value ml_qfs_remove(value v, value v_path) { return with_qfs_path<&KfsClient::Remove>("Qfs.remove", v, v_path); }
value ml_qfs_rmdir(value v, value v_path) { return with_qfs_path<&KfsClient::Rmdir>("Qfs.rmdir", v, v_path); }
value ml_qfs_rmdirs(value v, value v_path) { return with_qfs_path<&KfsClient::Rmdirs>("Qfs.rmdirs", v, v_path); }
value ml_qfs_rmdirs_fast(value v, value v_path) { return with_qfs_path<&KfsClient::RmdirsFast>("Qfs.rmdirs_fast", v, v_path); }

value ml_qfs_sync(value v, value v_file)
{
  int ret = with_qfs(v, &KfsClient::Sync, File_val(v_file));
  if (0 != ret)
    unix_error(-ret,"Qfs.sync",Nothing);
  return Val_unit;
}

value ml_qfs_rename(value v, value v_old, value v_new, value v_overwrite)
{
  CAMLparam4(v,v_old,v_new,v_overwrite);
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, const char*, bool)>(&KfsClient::Rename),
                     C_STR(v_old), C_STR(v_new), Bool_val(v_overwrite));
  if (0 != ret)
    unix_error(-ret,"Qfs.rename",v_old); // -1
  CAMLreturn(Val_unit);
}

value make_stat(KfsFileAttr const& st)
{
  CAMLparam0();
  CAMLlocal1(v_st);

  v_st = caml_alloc_tuple(9);
  Store_field(v_st, 0, caml_copy_int64(st.fileId));
  Store_field(v_st, 1, value_of_string(st.filename));
  Store_field(v_st, 2, Val_int(st.fileSize));
  Store_field(v_st, 3, caml_copy_double((double) st.mtime.tv_sec + (double) st.mtime.tv_usec / 1e6));
  Store_field(v_st, 4, caml_copy_double((double) st.ctime.tv_sec + (double) st.ctime.tv_usec / 1e6));
  Store_field(v_st, 5, caml_copy_double((double) st.crtime.tv_sec + (double) st.crtime.tv_usec / 1e6));
  Store_field(v_st, 6, Val_bool(st.isDirectory));
  Store_field(v_st, 7, Val_int(st.subCount1));
  Store_field(v_st, 8, Val_int(st.subCount2));

  CAMLreturn(v_st);
}

value ml_qfs_stat(value v, value v_path, value v_filesize)
{
  CAMLparam3(v,v_path,v_filesize);
  KfsFileAttr st;
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, KfsFileAttr&, bool)>(&KfsClient::Stat),
                     C_STR(v_path), st, Bool_val(v_filesize));
  if (0 != ret)
    unix_error(-ret,"Qfs.stat",v_path);
  CAMLreturn(make_stat(st));
}

value ml_qfs_fstat(value v, value v_file)
{
  KfsFileAttr st;
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(int, KfsFileAttr&)>(&KfsClient::Stat),
                     File_val(v_file), st);
  if (0 != ret)
    unix_error(-ret,"Qfs.fstat",Nothing);
  return make_stat(st);
}

value ml_qfs_readdir_plus(value v, value v_path, value v_filesize)
{
  CAMLparam3(v, v_path, v_filesize);
  CAMLlocal1(v_arr);
  std::vector<KfsFileAttr> result;
  int ret = with_qfs(v, &KfsClient::ReaddirPlus, C_STR(v_path), result, Bool_val(v_filesize), true, false);
  if (0 != ret)
    unix_error(-ret,"Qfs.readdir_plus",v_path);
  v_arr = caml_alloc_tuple(result.size());
  for (size_t i = 0; i < result.size(); i++)
  {
    Store_field(v_arr, i, make_stat(result[i]));
  }
  CAMLreturn(v_arr);
}

value ml_qfs_read(value v, value v_file, value v_buf, value v_ofs, value v_bytes)
{
  CAMLparam5(v, v_file, v_buf, v_ofs, v_bytes);
  int ret = ml_client::get(v)->Read(File_val(v_file), String_val(v_buf) + Int_val(v_ofs), Int_val(v_bytes));
  if (ret < 0)
    unix_error(-ret,"Qfs.read",Nothing);
  CAMLreturn(Val_int(ret));
}

value ml_qfs_pread(value v, value v_file, value v_pos, value v_buf, value v_ofs, value v_bytes)
{
  CAMLparam5(v, v_file, v_pos, v_buf, v_ofs);
  CAMLxparam1(v_bytes);
  int ret = ml_client::get(v)->PRead(File_val(v_file), Int_val(v_pos), String_val(v_buf) + Int_val(v_ofs), Int_val(v_bytes));
  if (ret < 0)
    unix_error(-ret,"Qfs.pread",Nothing);
  CAMLreturn(Val_int(ret));
}

value ml_qfs_pread_bytecode(value * argv, int argn)
{
  return ml_qfs_pread(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}

value ml_qfs_write(value v, value v_file, value v_buf, value v_ofs, value v_bytes)
{
  CAMLparam5(v, v_file, v_buf, v_ofs, v_bytes);
  int ret = ml_client::get(v)->Write(File_val(v_file), String_val(v_buf) + Int_val(v_ofs), Int_val(v_bytes));
  if (ret < 0)
    unix_error(-ret,"Qfs.write",Nothing);
  CAMLreturn(Val_int(ret));
}

value ml_qfs_pwrite(value v, value v_file, value v_pos, value v_buf, value v_ofs, value v_bytes)
{
  CAMLparam5(v, v_file, v_pos, v_buf, v_ofs);
  CAMLxparam1(v_bytes);
  int ret = ml_client::get(v)->PWrite(File_val(v_file), Int_val(v_pos), String_val(v_buf) + Int_val(v_ofs), Int_val(v_bytes));
  if (ret < 0)
    unix_error(-ret,"Qfs.pwrite",Nothing);
  CAMLreturn(Val_int(ret));
}

value ml_qfs_pwrite_bytecode(value * argv, int argn)
{
  return ml_qfs_pwrite(argv[0], argv[1], argv[2], argv[3], argv[4], argv[5]);
}

value ml_qfs_skip_holes(value v, value v_file)
{
  with_qfs(v, &KfsClient::SkipHolesInFile, File_val(v_file));
  return Val_unit;
}

value ml_qfs_get_iobuffer_size(value v, value v_file)
{
  return Val_int(with_qfs(v, &KfsClient::GetIoBufferSize, File_val(v_file)));
}

value ml_qfs_set_iobuffer_size(value v, value v_file, value v_size)
{
  return Val_int(with_qfs(v, &KfsClient::SetIoBufferSize, File_val(v_file), Int_val(v_size)));
}

value ml_qfs_get_readahead_size(value v, value v_file)
{
  return Val_int(with_qfs(v, &KfsClient::GetReadAheadSize, File_val(v_file)));
}

value ml_qfs_set_readahead_size(value v, value v_file, value v_size)
{
  return Val_int(with_qfs(v, &KfsClient::SetReadAheadSize, File_val(v_file), Int_val(v_size)));
}

#define INT_PARAM(name,set) \
value ml_qfs_Get##name(value v) \
{ \
  return Val_int(with_qfs(v, &KfsClient::Get##name)); \
} \
value ml_qfs_Set##name(value v, value v_set) \
{ \
  set(with_qfs(v, &KfsClient::Set##name, Int_val(v_set))); \
  return Val_unit; \
}

#define RETURN_VOID
#define RETURN_INT return Val_int

INT_PARAM(DefaultIoBufferSize,RETURN_INT)
INT_PARAM(DefaultReadAheadSize,RETURN_INT)
INT_PARAM(DefaultIOTimeout,RETURN_VOID)
INT_PARAM(DefaultMetaOpTimeout,RETURN_VOID)
INT_PARAM(RetryDelay,RETURN_VOID)
INT_PARAM(MaxRetryPerOp,RETURN_VOID)

value make_location(ServerLocation const& loc)
{
  CAMLparam0();
  CAMLlocal1(v);

  v = caml_alloc_tuple(2);
  Store_field(v, 0, value_of_string(loc.hostname));
  Store_field(v, 1, Val_int(loc.port));
  CAMLreturn(v);
}

value ml_qfs_get_metaserver_location(value v)
{
  CAMLparam1(v);
  CAMLlocal1(v_r);
  v_r = make_location(with_qfs(v, &KfsClient::GetMetaserverLocation));
  CAMLreturn(v_r);
}

value make_block_info(KfsClient::BlockInfo const& b)
{
  CAMLparam0();
  CAMLlocal1(v);

  v = caml_alloc_tuple(5);
  Store_field(v, 0, caml_copy_int64(b.offset));
  Store_field(v, 1, caml_copy_int64(b.id));
  Store_field(v, 2, Val_int(b.version));
  Store_field(v, 3, make_location(b.server));
  Store_field(v, 4, Val_int(b.size));

  CAMLreturn(v);
}

value ml_qfs_EnumerateBlocks(value v, value v_path)
{
  CAMLparam2(v, v_path);
  CAMLlocal1(v_res);
  KfsClient::BlockInfos blocks;
  int ret = with_qfs(v, static_cast<int (KfsClient::*)(const char*, KfsClient::BlockInfos&, bool)>(&KfsClient::EnumerateBlocks),
                     C_STR(v_path), blocks, true);

  if (0 != ret)
    unix_error(-ret,"Qfs.enumerate_blocks",v_path);

  v_res = caml_alloc_tuple(blocks.size());
  for (size_t i = 0; i < blocks.size(); i++)
  {
    Store_field(v_res, i, make_block_info(blocks[i]));
  }

  CAMLreturn(v_res);
}

value ml_qfs_GetFileOrChunkInfo(value v, value v_file, value v_chunk)
{
  CAMLparam3(v,v_file,v_chunk);
  CAMLlocal2(v_res,v_servers);
  KfsFileAttr attr;
  chunkOff_t offset;
  int64_t chunkVersion;
  vector<ServerLocation> servers;

  int ret = with_qfs(v, &KfsClient::GetFileOrChunkInfo, Int64_val(v_file), Int64_val(v_chunk), attr, offset, chunkVersion, servers);
  if (0 != ret)
    unix_error(-ret,"Qfs.get_file_or_chunk_info",Nothing);

  v_servers = caml_alloc_tuple(servers.size());
  for (size_t i = 0; i < servers.size(); i++)
  {
    Store_field(v_servers,i,make_location(servers[i]));
  }

  v_res = caml_alloc_tuple(4);
  Store_field(v_res,0,make_stat(attr));
  Store_field(v_res,1,caml_copy_int64(offset));
  Store_field(v_res,2,Val_int(chunkVersion));
  Store_field(v_res,3,v_servers);

  CAMLreturn(v_res);
}

} // extern "C"
