(** Container enter command. *)

let host_binds name =
  let fmt = {|{{ join .HostConfig.Binds "\n" }}|} in
  Proc.output "podman" [ "inspect"; "--format"; fmt; name ]
  |> String.split_on_char '\n'
  |> List.filter_map (fun bind ->
      match String.split_on_char ':' bind with
      | src :: _ when src <> "" -> Some src
      | _ -> None)

let workdir name =
  let cwd = Unix.realpath @@ Sys.getcwd () in
  let binds = host_binds name in
  let under src = String.starts_with ~prefix:src cwd in
  if List.exists under binds then cwd else Cmd_init.home

let run argv =
  let name, name_spec = Cmd.Args.name () in
  Cmd.parse argv [ name_spec ] "dbx enter [-n <name>]";

  Proc.output "podman" [ "start"; name () ] |> ignore;

  let uid = Unix.getuid () in
  let dir = workdir @@ name () in
  Proc.exec "podman"
    [
      "exec";
      "--detach-keys=";
      "--interactive";
      "--tty";
      "--user=" ^ Int.to_string uid;
      "--workdir=" ^ dir;
      name ();
      "fish";
      "--login";
    ]

let cmd = ("enter", run, "Enter the development container.")
