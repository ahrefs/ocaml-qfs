
exception Error of string

let init () =
  Callback.register_exception "Qfs.Error" (Error "");
  ()

type client
type file

external connect : string -> int -> client = "ml_qfs_connect"
external mkdir : client -> string -> unit = "ml_qfs_mkdir"
external exists : client -> string -> bool = "ml_qfs_exists"
external is_file : client -> string -> bool = "ml_qfs_is_file"
external is_directory : client -> string -> bool = "ml_qfs_is_directory"
external create : client -> string -> bool -> string -> file = "ml_qfs_create"
external close : client -> file -> unit = "ml_qfs_close"
external readdir : client -> string -> string array = "ml_qfs_readdir"
external remove : client -> string -> unit = "ml_qfs_remove"
external rmdir : client -> string -> unit = "ml_qfs_rmdir"
