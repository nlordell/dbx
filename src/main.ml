let main () =
  Cmd.run
    [
      Cmd_create.cmd;
      Cmd_enter.cmd;
      Cmd_link.cmd;
      Cmd_proxy.cmd;
      Cmd_run.cmd;
      Cmd_init.cmd;
    ]
    ~default:"enter"

let () = if !Sys.interactive then () else main ()
