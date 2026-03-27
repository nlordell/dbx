(** Container creation command. *)

let image = "registry.fedoraproject.org/fedora-toolbox:43"
let mounts = lazy [ Filename.concat (Sys.getenv "HOME") "Developer" ]

let check_selinux dir =
  let ctx = Proc.output "stat" [ "-c"; "%C"; dir ] |> String.trim in
  let t =
    match String.split_on_char ':' ctx with [ _; _; t; _ ] -> t | _ -> ctx
  in
  if t = "container_file_t" then ()
  else
    Cmd.failf "%s' is labeled '%s', not 'container_file_t'" dir t
      ~details:(Printf.sprintf "Run: `chcon -Rt container_file_t '%s'`" dir)

let run argv =
  let name, name_spec = Cmd.Args.name () in
  let dirs = ref [] in
  Cmd.parse argv
    [
      name_spec;
      ( "-w",
        Arg.String (fun d -> dirs := d :: !dirs),
        "<dir> Bind mount directory. [default: ~/Developer]" );
    ]
    "dbx create [-n <name>] [[-w <dir>]...]";

  if Proc.success "podman" [ "container"; "exists"; name () ] then begin
    print_endline "already created";
    exit 0
  end;

  let dirs =
    if List.is_empty !dirs then Lazy.force mounts else List.rev !dirs
  in
  List.iter check_selinux dirs;

  let exe = Unix.realpath Sys.executable_name in
  let uid = Unix.getuid () in
  let gid = Unix.getgid () in
  let volumes =
    List.map Unix.realpath dirs
    |> List.map (fun d -> Printf.sprintf "--volume=%s:%s" d d)
  in

  Proc.run "podman"
    ([
       "create";
       "--hostname=" ^ name ();
       "--label=manager=dbx";
       "--name=" ^ name ();
       "--user=root:root";
       "--userns=keep-id";
       "--volume=" ^ exe ^ ":/usr/local/bin/dbx:ro";
     ]
    @ volumes
    @ [ image; "dbx"; "init"; "-u"; Int.to_string uid; "-g"; Int.to_string gid ]
    );

  Proc.output "podman" [ "start"; name () ] |> ignore;
  Proc.wait_line "podman" [ "logs"; "--follow"; name () ] Cmd_init.ready_marker

let cmd = ("create", run, "Create a development container.")
