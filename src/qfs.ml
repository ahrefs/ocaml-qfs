open Printf

exception Error of string

let error fmt = Printf.ksprintf (fun s -> raise (Error s)) fmt
let error_lwt fmt = Printf.ksprintf (fun s -> Lwt.fail (Error s)) fmt

let init =
  let registered = ref false in
  fun () ->
    if not !registered then
      Callback.register_exception "Qfs.Error" (Error "");
    registered := true

let () =
  init ()

type client
type file
type stat = {
  fileid : int64; (** inode *)
  name : string;
  size : int; (** logical eof *)
  mtime : float; (** modification time *)
  ctime : float; (** attribute change time *)
  crtime : float; (** creation time *)
  is_dir : bool;
  subcount1 : int; (** number of chunks in the file or files in directory *)
  subcount2 : int; (** directories count *)
}
(*
    int16_t         numReplicas;
    int16_t         numStripes;
    int16_t         numRecoveryStripes;
    StripedFileType striperType;
    int32_t         stripeSize;
    kfsSTier_t      minSTier;
    kfsSTier_t      maxSTier;
*)


external connect : string -> int -> client = "ml_qfs_connect"
let connect s p =
  init (); (* http://caml.inria.fr/mantis/view.php?id=4166 *)
  connect s p
external release : client -> unit = "ml_qfs_release"

(** Create a directory.
    @raise Unix_error if directory already exists or parent directory doesn't exist
*)
external mkdir : client -> string -> unit = "ml_qfs_mkdir"

(** Create a directory hierarchy.
    If parent dirs don't exist - they will be created.
    Doesn't raise error if directory already exists.
*)
external mkdirs : client -> string -> unit = "ml_qfs_mkdirs"

external exists : client -> string -> bool = "ml_qfs_exists"
external is_file : client -> string -> bool = "ml_qfs_is_file"
external is_directory : client -> string -> bool = "ml_qfs_is_directory"
external create : client -> string -> excl:bool -> params:string -> file = "ml_qfs_create"
external openfile : client -> string -> Unix.open_flag list -> params:string -> file = "ml_qfs_open"
external close : client -> file -> unit = "ml_qfs_close"
external readdir : client -> string -> string array = "ml_qfs_readdir"
external readdir_plus : client -> string -> bool -> stat array = "ml_qfs_readdir_plus"

(** remove file *)
external remove : client -> string -> unit = "ml_qfs_remove"

(** remove directory *)
external rmdir : client -> string -> unit = "ml_qfs_rmdir"

(** remove directory hierarchy *)
external rmdirs : client -> string -> unit = "ml_qfs_rmdirs"

external rmdirs_fast : client -> string -> unit = "ml_qfs_rmdirs_fast"

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

external get_replication_factor : client -> string -> int = "ml_qfs_get_replication_factor"
external set_replication_factor : client -> string -> int -> int = "ml_qfs_set_replication_factor"
external set_replication_factor_r : client -> string -> int -> int = "ml_qfs_set_replication_factor_r"

(** in seconds *)
external get_default_io_timeout : client -> int = "ml_qfs_GetDefaultIOTimeout"
external set_default_io_timeout : client -> int -> unit = "ml_qfs_SetDefaultIOTimeout"
external get_default_meta_op_timeout : client -> int = "ml_qfs_GetDefaultMetaOpTimeout"
external set_default_meta_op_timeout : client -> int -> unit = "ml_qfs_SetDefaultMetaOpTimeout"
external get_retry_delay : client -> int = "ml_qfs_GetRetryDelay"
external set_retry_delay : client -> int -> unit = "ml_qfs_SetRetryDelay"
external get_max_retry_per_op : client -> int = "ml_qfs_GetMaxRetryPerOp"
external set_max_retry_per_op : client -> int -> unit = "ml_qfs_SetMaxRetryPerOp"

let stat fs ?(size=true) path = stat fs path size
let readdir_plus fs ?(size=true) path = readdir_plus fs path size

external read_unsafe : client -> file -> Bytes.t -> int -> int -> int = "ml_qfs_read"
external pread_unsafe : client -> file -> int -> Bytes.t -> int -> int -> int = "ml_qfs_pread_bytecode" "ml_qfs_pread"

let read_buf fs file ?pos buf ?(ofs=0) n =
  if ofs < 0 || n < 0 || Bytes.length buf < ofs + n then invalid_arg "Qfs.read_buf";
  match pos with
  | None -> read_unsafe fs file buf ofs n
  | Some pos -> pread_unsafe fs file pos buf ofs n

let read fs file ?pos n =
  if n < 0 then invalid_arg "Qfs.read";
  let s = Bytes.create n in
  let n = match pos with
  | None -> read_unsafe fs file s 0 n
  | Some pos -> pread_unsafe fs file pos s 0 n
  in
  if n = Bytes.length s then s else Bytes.sub s 0 n

external write_unsafe : client -> file -> string -> int -> int -> int = "ml_qfs_write"
external pwrite_unsafe : client -> file -> int -> string -> int -> int -> int = "ml_qfs_pwrite_bytecode" "ml_qfs_pwrite"

let write_sub_once fs file ?pos s ofs n =
  if ofs < 0 || n < 0 || ofs > String.length s || n > String.length s - ofs then invalid_arg "Qfs.write_sub_once";
  match pos with
  | None -> write_unsafe fs file s ofs n
  | Some pos -> pwrite_unsafe fs file pos s ofs n

let write_sub fs file ?pos s ofs n =
  let rec loop written =
    if written = n then
      ()
    else
      match write_sub_once fs file ?pos s (ofs+written) (n-written) with
      | m when m < 0 -> assert false
      | 0 -> raise (Error (Printf.sprintf "Qfs.write_sub stalled at %d of %d" written n))
      | m -> loop (written + m)
  in
  loop 0

let write fs file ?pos s = write_sub fs file ?pos s 0 (String.length s)

type block_info =
{
  offset : int64;
  chunkid : int64;
  version : int;
  server : (string * int);
  chunk_size : int;
}

let show_location (host,port) = Printf.sprintf "%s:%d" host port

let show_block_info { offset; chunk_size; chunkid; version; server; } =
  Printf.sprintf "%Ld.%d %d@%Ld %s" chunkid version chunk_size offset (show_location server)

(** only for testing/debugging!
  NB synchronous communication *)
external enumerate_blocks : client -> string -> block_info array = "ml_qfs_EnumerateBlocks"

external get_file_or_chunk_info : client -> int64 -> int64 -> stat * int64 * int * (string * int) array = "ml_qfs_GetFileOrChunkInfo"

type system_info = {
  total_space : int;
  used_space : int;
  free_space : int;
  uptime : int;
  buffers : int;
  clients : int;
  delayed_recovery : int;
  in_recovery : int;
  servers : int;
  sockets : int;
  chunks : int;
  requests : int;
  total_drives : int;
  writable_drives : int;
  max_clients : int;
  max_servers : int;
  buffers_total : int;
  pending_recovery : int;
  pending_replication : int;
  replication_backlog : int;
  repl_check_timeouts : int;
  replications : int;
  replications_check : int;
  fattr_nodes : int;
}

type server =
{
  host : string;
  port : int;
  free : int;
  used : int;
  total : int;
  rack : int;
  nblocks : int;
  lastheard : int;
  ncorrupt : int;
  nchunksToMove : int;
  numDrives : int;
  numWritableDrives : int;
  overloaded : int;
  numReplications : int;
  numReadReplications : int;
  good : int;
  nevacuate : int;
  bytesevacuate : int;
  nlost : int;
  nwrites : int;
  load : int;
  replay : int;
  connected : int;
  stopped : int;
  chunks : int;
  tiers : string;
  lostChunkDirs : string option;
  util_pct : float;
}

type down_server =
{
  ip : string;
  port : int;
  reason : string;
  since : string;
}

type evacuating_server =
{
  ip : string;
  port : int;
  eta : float;
  bytes_left : int;
  chunks_done : int;
  bytes_done : int;
  chunks_inflight : int;
  chunks_pending : int;
  chunks_per_second : float;
  bytes_per_second : float;
  chunks_left : int;
}

type info =
{
  build_version : string;
  source_version : string;
  worm : bool;
  system : system_info;
  servers : server list;
  down_servers : down_server list;
  evacuating_servers : evacuating_server list;
}

open ExtLib

let parse_kv ~delim s =
  let h = Hashtbl.create 10 in
  String.nsplit s delim |> List.iter begin fun s ->
    match String.split s "=" with
    | k,v -> Hashtbl.replace h String.(lowercase @@ strip k) (String.strip v)
    | exception _ -> error "no delimiter"
  end;
  h

let parse_servers v =
  String.nsplit v "\t" |> List.map begin fun s ->
    let h = parse_kv ~delim:"," s in
    let int k = try int_of_string @@ Hashtbl.find h k with _ -> error "bad server value %S" k in
    let float k = try float_of_string @@ Hashtbl.find h k with _ -> error "bad server value %S" k in
    let str k = try Hashtbl.find h k with _ -> error "bad server value %S" k in
    let maybe_str k = try Some(Hashtbl.find h k) with _ -> None in
    {
      host = str "s";
      port = int "p";
      free = int "free";
      used = int "used";
      total = int "total";
      rack = int "rack";
      nblocks = int "nblocks";
      lastheard = int "lastheard";
      ncorrupt = int "ncorrupt";
      nchunksToMove = int "nchunkstomove";
      numDrives = int "numdrives";
      numWritableDrives = int "numwritabledrives";
      overloaded = int "overloaded";
      numReplications = int "numreplications";
      numReadReplications = int "numreadreplications";
      good = int "good";
      nevacuate = int "nevacuate";
      bytesevacuate = int "bytesevacuate";
      nlost = int "nlost";
      nwrites = int "nwrites";
      load = int "load";
      replay = int "replay";
      connected = int "connected";
      stopped = int "stopped";
      chunks = int "chunks";
      tiers = str "tiers";
      lostChunkDirs = maybe_str "lostChunkDirs";
      util_pct = float "util";
    }
  end

let parse_down_servers v =
  String.nsplit v "\t" |> List.map begin fun s ->
    let h = parse_kv ~delim:"," s in
    let int k = try int_of_string @@ Hashtbl.find h k with _ -> error "bad down_server value %S" k in
    let str k = try Hashtbl.find h k with _ -> error "bad down_server value %S" k in
    {
      ip = str "s";
      port = int "p";
      reason = str "reason";
      since = str "down";
    }
  end

let parse_evacuating_servers v =
  String.nsplit v "\t" |> List.map begin fun s ->
    let h = parse_kv ~delim:"," s in
    let int k = try int_of_string @@ Hashtbl.find h k with _ -> error "bad evacuating_server value %S" k in
    let float k = try float_of_string @@ Hashtbl.find h k with _ -> error "bad evacuating_server value %S" k in
    let str k = try Hashtbl.find h k with _ -> error "bad evacuating_server value %S" k in
    {
      ip = str "s";
      port = int "p";
      eta = float "eta";
      bytes_left = int "b";
      chunks_done = int "cdone";
      bytes_done = int "bdone";
      chunks_inflight = int "cflight";
      chunks_pending = int "cpend";
      chunks_per_second = float "csec";
      bytes_per_second = float "bsec";
      chunks_left = int "c";
    }
  end

let parse_system_info v =
  let h = try parse_kv ~delim:"\t" v with Error s -> error "system info : %s" s in
  let int k = try int_of_string @@ Hashtbl.find h k with _ -> error "bad system info value %S" k in
  {
    total_space = int "total space";
    used_space = int "used space";
    free_space = int "free space";
    uptime = int "uptime";
    buffers = int "buffers";
    clients = int "clients";
    delayed_recovery = int "delayed recovery";
    in_recovery = int "in recovery";
    servers = int "chunk srvs";
    sockets = int "sockets";
    chunks = int "chunks";
    requests = int "requests";
    total_drives = int "total drives";
    writable_drives = int "writable drives";
    max_clients = int "max clients";
    max_servers = int "max chunk srvs";
    buffers_total = int "buffers total";
    pending_recovery = int "pending recovery";
    pending_replication = int "pending replication";
    repl_check_timeouts = int "repl check timeouts";
    replication_backlog = int "replication backlog";
    replications = int "replications";
    replications_check = int "replications check";
    fattr_nodes = int "fattr nodes";
  }

let parse_ping cin =
  let build_version = ref "" in
  let source_version = ref "" in
  let system = ref None in
  let worm = ref false in
  let servers = ref [] in
  let down_servers = ref [] in
  let evacuating_servers = ref [] in
  let%lwt () =
    Lwt_io.read_lines cin |> Lwt_stream.junk_while_s begin function
    | "" -> Lwt.return false
    | "OK" -> Lwt.return true
    | s ->
      Lwt.wrap @@ fun () ->
        try
          match String.split s ":" with
          | exception _ -> error "no delimiter"
          | (k,v) ->
          let v = String.strip v in
          let () = match String.(lowercase @@ strip k) with
          | "build-version" -> build_version := v
          | "source-version" -> source_version := v
          | "worm" -> worm := v <> "0"
          | "system info" -> system := Some (parse_system_info v)
          | "servers" -> servers := parse_servers v
          | "down servers" -> down_servers := parse_down_servers v
          | "evacuating servers" -> evacuating_servers := parse_evacuating_servers v
          | "cseq"
          | "status"
          | "vr status"
          | "retiring servers"
          | "rebalance status"
          | "config"
          | "watchdog"
          | "rusage self"
          | "rusage children"
          | "storage tiers info"
          | "storage tiers info names" -> () (* skip *)
          | _ -> prerr_endline @@ sprintf "PING response: unrecognized key %S, skipping" k
          in
          true
        with
        | Error err -> error "%s in line %S" err s
        | exn -> error "%s in line %S" (Printexc.to_string exn) s
    end
  in
  let get name = function None -> error_lwt "PING: missing %s" name | Some x -> Lwt.return x in
  let%lwt system = get "system" !system in
  Lwt.return {
    build_version = !build_version;
    source_version = !source_version;
    worm = !worm;
    system;
    servers = !servers;
    down_servers = !down_servers;
    evacuating_servers = !evacuating_servers;
  }

let ping client =
  let (host,port) = get_metaserver_location client in
  Lwt_io.with_connection Unix.(ADDR_INET (inet_addr_of_string host, port)) begin fun (cin,cout) ->
    let%lwt () = Lwt_io.write cout "PING\r\nVersion: KFS/1.0\r\nCseq: 1\r\nClient-Protocol-Version: 114\r\n\r\n" in
    parse_ping cin
  end
