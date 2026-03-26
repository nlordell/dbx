let main () = Cmd.run [ Cmd_init.cmd ] ~default:"enter"
let () = if !Sys.interactive then () else main ()
