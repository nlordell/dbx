(** Development container. *)

let home = "/dbx"
let shell = "fish"
let packages = [ "fish"; "netcat"; "tini"; "which" ]
let ready_marker = "=== READY TO ROLL! ==="
let exists name = Proc.success "podman" [ "container"; "exists"; name ]

let start name =
  if not @@ exists name then
    Cmd.failf "container '%s' does not exists" name
      ~details:"make sure to create it with `dbx create`";
  Proc.quiet "podman" [ "start"; name ]

let shell_exec ~tty name args =
  let host_binds name =
    Proc.output "podman"
      [ "inspect"; "--format"; {|{{ join .HostConfig.Binds "\n" }}|}; name ]
    |> String.split_on_char '\n'
    |> List.filter_map (fun bind ->
        match String.split_on_char ':' bind with
        | src :: _ when src <> "" -> Some src
        | _ -> None)
  in
  let workdir name =
    let cwd = Unix.realpath @@ Sys.getcwd () in
    let binds = host_binds name in
    let under src = String.starts_with ~prefix:src cwd in
    if List.exists under binds then cwd else home
  in

  start name;
  let uid = Unix.getuid () in
  let dir = workdir name in
  Proc.exec "podman"
    ([
       "exec";
       "--detach-keys=";
       "--interactive";
       "--tty=" ^ Bool.to_string tty;
       "--user=" ^ Int.to_string uid;
       "--workdir=" ^ dir;
       name;
       shell;
     ]
    @ args)
