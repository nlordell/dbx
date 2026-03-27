let main () =
  Cmd.run
    [ Cmd_create.cmd; Cmd_enter.cmd; Cmd_init.cmd; Cmd_run.cmd ]
    ~default:"enter"

let () = if !Sys.interactive then () else main ()
