(** Container link command. *)

let xdg_data_home () =
  match Sys.getenv_opt "XDG_DATA_HOME" with
  | Some value -> value
  | None -> Filename.concat (Sys.getenv "HOME") ".local/share"

let rec mkdir_p path perm =
  if Sys.is_directory path then ()
  else
    let parent = Filename.dirname path in
    mkdir_p parent perm;
    Sys.mkdir path perm

let write_link exe name cmd out =
  let nl () = output_char out '\n' in
  Printf.fprintf out {|#!/bin/sh|} |> nl;
  Printf.fprintf out {|exec '%s' run -n '%s' -- '%s' "$@"|} exe name cmd |> nl

let run argv =
  let name, name_spec = Cmd.Args.name () in
  let search = ref false in
  let cmd = ref "" in
  Cmd.parse argv
    ~anon:(function
      | [ c ] -> cmd := c
      | _ -> Cmd.Args.badf "expected exactly one command to link")
    [ name_spec; ("-s", Arg.Set search, " Links always search $PATH.") ]
    "dbx link [-n <name>] [-s] [--] <cmd>";

  let bin = Filename.concat (xdg_data_home ()) "dbx/bin" in
  let link = Filename.concat bin !cmd in
  if Sys.file_exists link then Cmd.fail "already linked";

  let path = Shell.which !name !cmd in
  let exe, cmd =
    if !search then (Filename.basename Sys.executable_name, !cmd)
    else (Unix.realpath Sys.executable_name, path)
  in

  mkdir_p bin 0o755;
  Out_channel.with_open_text link (write_link exe !name cmd);
  Unix.chmod link 0o755

let cmd = ("link", run, "Link a container command to the host.")
