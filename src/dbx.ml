let main () = print_endline "Hello, World!"
let () = if !Sys.interactive then () else Cmd.run main
