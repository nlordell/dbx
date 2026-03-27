(** Container enter command. *)

let run argv =
  let name, name_spec = Cmd.Args.name () in
  Cmd.parse argv [ name_spec ] "dbx enter [-n <name>]";

  Shell.exec ~tty:true (name ()) [ "--login" ]

let cmd = ("enter", run, "Enter the development container.")
