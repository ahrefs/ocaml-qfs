
open Printf

let error fmt = ksprintf (fun s -> prerr_endline s; exit 1) fmt

let main () =
  let host = ref "" in
  let port = ref (-1) in
  let usage = sprintf "Usage: %s -s <meta server name> -p <port>" Sys.argv.(0) in
  let args = [
    "-s",Arg.Set_string host," meta server host";
    "-p",Arg.Set_int port," meta server port";
  ] in
  Arg.parse args (fun s -> failwith (sprintf "Unknown flag %S" s)) usage;
  if !host = "" || !port < 0 then error "%s" usage;

  (* Get a handle to the KFS client object.  This is our entry into the KFS namespace. *)
  let fs = Qfs.connect !host !port in

  (* Make a directory /ctest *)
  let dir = "ctest" in
  Qfs.mkdir fs dir;

  (* What we just created better be a directory *)
  if not (Qfs.is_directory fs dir) then error "KFS doesn't think %S is a dir!" dir;

  (* Create a simple file with default replication (at most 3) *)
  let file1 = Filename.concat dir "foo.1" in

  (*
    fd is our file-handle to the file we are creating; this
    file handle should be used in subsequent I/O calls on the file.
  *)
  let fd = Qfs.create fs file1 false "" in

  (* Get the directory listings *)
  let entries = Qfs.readdir fs dir in

  print_endline "Read dir returned: ";
  Array.iter print_endline entries;

(*
    // write something to the file
    int numBytes = 2048;
    char *dataBuf = new char[numBytes];

    generateData(dataBuf, numBytes);

    // make a copy and write out using the copy; we keep the original
    // so we can validate what we get back is what we wrote.
    char *copyBuf = new char[numBytes];
    memcpy(copyBuf, dataBuf, numBytes);

    res = gKfsClient->Write(fd, copyBuf, numBytes);
    if (res != numBytes) {
        cout << "Was able to write only: " << res << " instead of " << numBytes << endl;
    }

    // flush out the changes
    gKfsClient->Sync(fd);

    // Close the file-handle
    gKfsClient->Close(fd);

    // Determine the file-size
    KFS::KfsFileAttr fileAttr;
    gKfsClient->Stat(tempFilename.c_str(), fileAttr);
    long size = fileAttr.fileSize;

    if (size != numBytes) {
        cout << "KFS thinks the file's size is: " << size << " instead of " << numBytes << endl;
    }

    // rename the file
    string newFilename = baseDir + "/foo.2";
    gKfsClient->Rename(tempFilename.c_str(), newFilename.c_str());

    if (gKfsClient->Exists(tempFilename.c_str())) {
        cout << tempFilename << " still exists after rename!" << endl;
        exit(-1);
    }

    // Re-create the file and try a rename that should fail...
    int fd1 = gKfsClient->Create(tempFilename.c_str());

    if (!gKfsClient->Exists(tempFilename.c_str())) {
        cout << " After rec-create..., " << tempFilename << " doesn't exist!" << endl;
        exit(-1);
    }

    gKfsClient->Close(fd1);

    // try to rename and don't allow overwrite
    if (gKfsClient->Rename(newFilename.c_str(), tempFilename.c_str(), false) == 0) {
        cout << "Rename  with overwrite disabled succeeded...error!" << endl;
        exit(-1);
    }

    // Remove the file
    gKfsClient->Remove(tempFilename.c_str());

    // Re-open the file
    if ((fd = gKfsClient->Open(newFilename.c_str(), O_RDWR)) < 0) {
        cout << "Open on : " << newFilename << " failed: " << KFS::ErrorCodeToStr(fd) << endl;
        exit(-1);
    }

    // read some bytes
    res = gKfsClient->Read(fd, copyBuf, 128);
    if (res != 128) {
        if (res < 0) {
            cout << "Read on : " << newFilename << " failed: " << KFS::ErrorCodeToStr(res) << endl;
            exit(-1);
        }
    }

    // Verify what we read matches what we wrote
    for (int i = 0; i < 128; i++) {
        if (dataBuf[i] != copyBuf[i]) {
            cout << "Data mismatch at : " << i << endl;
        }
    }
    delete[] dataBuf;

    // seek to offset 40
    gKfsClient->Seek(fd, 40);

    // Seek and verify that we are we think we are
    size = gKfsClient->Tell(fd);
    if (size != 40) {
        cout << "After seek, we are at: " << size << " should be at 40 " << endl;
    }

    gKfsClient->Close(fd);

    // remove the file
    gKfsClient->Remove(newFilename.c_str());
*)
  Qfs.close fs fd;
  Qfs.remove fs file1;
  Qfs.rmdir fs dir;
  print_endline "Tests passed!"

let () =
  try
    main ()
  with
    exn ->
      prerr_endline (Printexc.to_string exn);
      exit 1

(*
void generateData(char *buf, int numBytes)
{
    int i;

    srand(100);
    for (i = 0; i < numBytes; i++) {
        buf[i] = (char) ('a' + (rand() % 26));
    }
}
*)
