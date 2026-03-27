(** Container init process. *)

let run argv =
  let which cmd = Proc.output "which" [ cmd ] |> String.trim in
  let user uid =
    let Unix.{ pw_name } = Unix.getpwuid uid in
    pw_name
  in

  let uid = ref 1000 in
  let gid = ref 1000 in
  Cmd.parse argv
    [
      ("-u", Arg.Set_int uid, "<uid> UID of the container user");
      ("-g", Arg.Set_int gid, "<gid> GID of the container user");
    ]
    "dbx init [-u <uid>] [-g <gid>]";

  if Unix.getpid () != 1 then Cmd.fail "not running as an init process";
  if not (Sys.file_exists "/run/.containerenv") then
    Cmd.fail "not running inside a container";

  (* setup init process signal handling *)
  List.iter
    (fun s ->
      Sys.(
        set_signal s
          (Signal_handle (fun _ -> Cmd.fail ~code:(s + 128) "interrupted"))))
    Sys.[ sigint; sigterm ];

  if not (Sys.file_exists Container.home) then begin
    Proc.run "dnf" ("install" :: "-y" :: Container.packages);
    Proc.run "usermod"
      [
        "--home=" ^ Container.home;
        "--groups=wheel";
        "--password=";
        "--shell=" ^ which Container.shell;
        user !uid;
      ];
    Unix.mkdir Container.home 0o700;
    Unix.chown Container.home !uid !gid
  end;

  print_endline Container.ready_marker;
  Proc.exec "tini" [ "sleep"; "--"; "infinity" ]

let cmd = ("init", run, "")
