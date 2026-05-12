(** TCP proxy command. *)

let port_of_string s =
  match int_of_string_opt s with
  | Some p when p > 0 && p < 65536 -> p
  | _ -> Cmd.Args.badf "invalid port '%s'" s

let peer_id addr =
  match addr with
  | Unix.ADDR_INET (_, port) -> ":" ^ Int.to_string port
  | _ -> "(?)"

let run argv =
  let name, name_spec = Cmd.Args.name () in
  let port = ref 0 in
  let addr = ref Unix.inet_addr_loopback in
  let host_port = ref 0 in
  let verbose = ref false in
  Cmd.parse argv
    ~anon:(fun p -> port := Cmd.Args.single p "port to proxy" |> port_of_string)
    [
      name_spec;
      ( "-b",
        Arg.String
          (fun a ->
            try addr := Unix.inet_addr_of_string a
            with _ -> Cmd.Args.badf "invalid bind address '%s'" a),
        "<addr> Address to bind to. [default: localhost]" );
      ( "-P",
        Arg.String (fun p -> host_port := port_of_string p),
        "<port> Host port to listen on. [default: container port]" );
      ("-v", Arg.Set verbose, " Enable verbose logging.");
    ]
    "dbx proxy [-b <addr>] [-P <port>] [-n <name>] [-v] <port>";

  if !host_port = 0 then host_port := !port;
  let logf fmt =
    Printf.ksprintf (if !verbose then print_endline else ignore) fmt
  in

  Container.start !name;

  let server =
    Cmd.with_err ~message:"failed to listen on host port" (fun () ->
        let server = Unix.(socket PF_INET SOCK_STREAM 0) in
        Unix.(setsockopt server TCP_NODELAY true);
        Unix.bind server Unix.(ADDR_INET (!addr, !host_port));
        Unix.listen server 16;
        server)
  in

  logf "listening on localhost:%d" !port;

  let uid = Unix.getuid () in
  let connection (client, addr) =
    Fun.protect
      (fun () ->
        logf "connected %s" @@ peer_id addr;
        try
          Proc.pipe "podman" ~stdin:client ~stdout:client
            [
              "exec";
              "--interactive";
              "--user=" ^ Int.to_string uid;
              !name;
              "/usr/bin/netcat";
              "-N";
              "localhost";
              Int.to_string !port;
            ]
        with e -> logf "error proxying connection: %s" @@ Printexc.to_string e)
      ~finally:(fun () ->
        (try Unix.(shutdown client SHUTDOWN_ALL)
         with e -> logf "client shutdown error: %s" @@ Printexc.to_string e);
        (try Unix.(close client)
         with e -> logf "client close error: %s" @@ Printexc.to_string e);
        logf "disconnected %s" @@ peer_id addr)
  in

  let rec loop () =
    let conn = Cmd.with_err ~message:"failed to accept
                 Unix.accept server in
    Thread.create connection conn |> ignore;
    loop ()
  in
  loop ()

let cmd = ("proxy", run, "Proxy a container port to the host.")
