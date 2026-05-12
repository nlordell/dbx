(** Run child processes.

    This module provides a high-level and convenient API for spawing child
    processes in various forms. *)

let failwithf fmt = Printf.ksprintf failwith fmt

let exit_result cmd status =
  let code =
    match status with
    | Unix.WEXITED ec -> ec
    | Unix.WSIGNALED s | Unix.WSTOPPED s -> 128 + s
  in
  if code = 0 then () else failwithf "command '%s' exited with code %d" cmd code

let status cmd args =
  let cmdline =
    Filename.quote_command cmd args ~stdin:"/dev/null" ~stdout:"/dev/null"
      ~stderr:"/dev/null"
  in
  Unix.system cmdline

let success cmd args =
  match status cmd args with Unix.WEXITED 0 -> true | _ -> false

let quiet cmd args = status cmd args |> exit_result cmd

let run cmd args =
  let cmdline = Filename.quote_command cmd args in
  let status = Unix.system cmdline in
  exit_result cmd status

let exec cmd args =
  let args' = Array.of_list (cmd :: args) in
  Unix.execvp cmd args'

let pipe cmd args ~stdin ~stdout =
  let args' = Array.of_list (cmd :: args) in
  let pid = Unix.create_process cmd args' stdin stdout Unix.stderr in
  let _, status = Unix.waitpid [] pid in
  exit_result cmd status

let stream cmd args f =
  let args' = Array.of_list (cmd :: args) in
  let process = Unix.open_process_args cmd args' in
  let status = ref None in
  let result =
    Fun.protect
      (fun () ->
        try f process
        with ex ->
          begin try
            let pid = Unix.process_pid process in
            Unix.kill pid Sys.sigkill
          with _ -> ()
          end;
          raise ex)
      ~finally:(fun () -> status := Some (Unix.close_process process))
  in
  exit_result cmd @@ Option.get !status;
  result

let output cmd args =
  stream cmd args (fun (stdout, stdin) ->
      close_out_noerr stdin;
      In_channel.input_all stdout)

let wait_line cmd args line =
  try
    stream cmd args (fun (stdout, stdin) ->
        close_out_noerr stdin;
        let rec loop () =
          match In_channel.input_line stdout with
          | Some l when l = line -> raise Sys.Break
          | Some l -> print_endline l |> loop
          | None -> ()
        in
        loop ());
    failwithf "command '%s' exited unexpectedly" cmd
  with Sys.Break -> ()
