(** Execute commands in a development container with the user shell. *)

let exec ~tty name args =
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
    if List.exists under binds then cwd else Container.home
  in

  let uid = Unix.getuid () in
  let dir = workdir name in
  Proc.quiet "podman" [ "start"; name ];
  Proc.exec "podman"
    ([
       "exec";
       "--detach-keys=";
       "--interactive";
       "--tty=" ^ Bool.to_string tty;
       "--user=" ^ Int.to_string uid;
       "--workdir=" ^ dir;
       name;
       Container.shell;
     ]
    @ args)

let which name cmd =
  let uid = Unix.getuid () in
  let command = Printf.sprintf "which '%s'" cmd in
  Proc.quiet "podman" [ "start"; name ];
  Proc.output "podman"
    [
      "exec";
      "--user=" ^ Int.to_string uid;
      name;
      Container.shell;
      "--command";
      command;
    ]
  |> String.trim
