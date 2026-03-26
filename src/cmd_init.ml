(** Container init process. *)

let home = "/dbx"
let packages = [ "fish"; "netcat"; "which" ]
let ready_marker = "=== READY TO ROLL! ==="

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

  if not (Sys.file_exists home) then begin
    Proc.run "dnf" ("install" :: "-y" :: packages);
    Proc.run "usermod"
      [
        "--home";
        home;
        "--groups";
        "wheel";
        "--password";
        "";
        "--shell";
        which "fish";
        user !uid;
      ];
    Unix.mkdir home 0o700;
    Unix.chown home !uid !gid
  end;

  print_endline ready_marker;
  while true do
    Unix.sleep Int.max_int
  done

let cmd = ("init", run, "devbox init process")
