(** Container run command. *)

let run argv =
  let name, name_spec = Cmd.Args.name () in
  let cmd = ref "" in
  let args = ref [] in
  Cmd.parse argv [ name_spec ]
    ~anon:(function
      | [] -> Cmd.Args.badf "missing command to run"
      | c :: a ->
          cmd := c;
          args := a)
    "dbx run [-n <name>] [--] <cmd> [<args>...]";

  let tty = Unix.(isatty stdin && isatty stdout) in
  let command = Printf.sprintf "exec '%s' $argv" !cmd in
  Container.shell_exec ~tty !name ([ "--command=" ^ command; "--" ] @ !args)

let cmd = ("run", run, "Run a command in the development container.")
