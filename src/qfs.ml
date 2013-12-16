
exception Error of string

let init () =
  Callback.register_exception "Qfs.Error" (Error "");
  ()

type client
type file
type stat = {
  name : string;
  size : int;
  mtime : float;
  is_dir : bool;
}

external connect : string -> int -> client = "ml_qfs_connect"
external release : client -> unit = "ml_qfs_release"
external mkdir : client -> string -> unit = "ml_qfs_mkdir"
external mkdirs : client -> string -> unit = "ml_qfs_mkdirs"
external exists : client -> string -> bool = "ml_qfs_exists"
external is_file : client -> string -> bool = "ml_qfs_is_file"
external is_directory : client -> string -> bool = "ml_qfs_is_directory"
external create : client -> string -> excl:bool -> params:string -> file = "ml_qfs_create"
external openfile : client -> string -> Unix.open_flag list -> params:string -> file = "ml_qfs_open"
external close : client -> file -> unit = "ml_qfs_close"
external readdir : client -> string -> string array = "ml_qfs_readdir"
external readdir_plus : client -> string -> bool -> stat array = "ml_qfs_readdir_plus"
external remove : client -> string -> unit = "ml_qfs_remove"
external rmdir : client -> string -> unit = "ml_qfs_rmdir"
external sync : client -> file -> unit = "ml_qfs_sync"
external rename : client -> string -> string -> bool -> unit = "ml_qfs_rename"
external stat : client -> string -> bool -> stat = "ml_qfs_stat"
external fstat : client -> file -> stat = "ml_qfs_fstat"
external set_skip_holes : client -> file -> unit = "ml_qfs_skip_holes"

external get_metaserver_location : client -> string * int = "ml_qfs_get_metaserver_location"

external get_default_iobuffer_size : client -> int = "ml_qfs_GetDefaultIoBufferSize"
external set_default_iobuffer_size : client -> int -> int = "ml_qfs_SetDefaultIoBufferSize"
external get_iobuffer_size : client -> file -> int = "ml_qfs_get_iobuffer_size"
external set_iobuffer_size : client -> file -> int -> int = "ml_qfs_set_iobuffer_size"

external get_default_readahead_size : client -> int = "ml_qfs_GetDefaultReadAheadSize"
external set_default_readahead_size : client -> int -> int = "ml_qfs_SetDefaultReadAheadSize"
external get_readahead_size : client -> file -> int = "ml_qfs_get_readahead_size"
external set_readahead_size : client -> file -> int -> int = "ml_qfs_set_readahead_size"

(** in seconds *)
external get_default_io_timeout : client -> int = "ml_qfs_GetDefaultIOTimeout"
external set_default_io_timeout : client -> int -> unit = "ml_qfs_SetDefaultIOTimeout"
external get_retry_delay : client -> int = "ml_qfs_GetRetryDelay"
external set_retry_delay : client -> int -> unit = "ml_qfs_SetRetryDelay"
external get_max_retry_per_op : client -> int = "ml_qfs_GetMaxRetryPerOp"
external set_max_retry_per_op : client -> int -> unit = "ml_qfs_SetMaxRetryPerOp"

let stat fs ?(size=true) path = stat fs path size
let readdir_plus fs ?(size=true) path = readdir_plus fs path size

external read_unsafe : client -> file -> string -> int -> int -> int = "ml_qfs_read"
external pread_unsafe : client -> file -> int -> string -> int -> int -> int = "ml_qfs_pread_bytecode" "ml_qfs_pread"

let read_buf fs file ?pos buf ?(ofs=0) n =
  if ofs < 0 || n < 0 || String.length buf < ofs + n then invalid_arg "Qfs.read_buf";
  match pos with
  | None -> read_unsafe fs file buf ofs n
  | Some pos -> pread_unsafe fs file pos buf ofs n

let read fs file ?pos n =
  if n < 0 then invalid_arg "Qfs.read";
  let s = String.create n in
  let n = match pos with
  | None -> read_unsafe fs file s 0 n
  | Some pos -> pread_unsafe fs file pos s 0 n
  in
  if n = String.length s then s else String.sub s 0 n
