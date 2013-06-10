
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

let stat fs ?(size=true) path = stat fs path size
let readdir_plus fs ?(size=true) path = readdir_plus fs path size
