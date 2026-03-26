(** Run child processes.

    This module provides a high-level and convenient API for spawing child
    processes in various forms. *)

let wrap_err f =
  try f ()
  with Unix.Unix_error (err, fn, param) ->
    let msg = Unix.error_message err in
    if String.equal param "" then Cmd.failf ~code:3 "%s: %s" fn msg
    else Cmd.failf ~code:3 "%s: %s '%s'" fn msg param

let exit_result cmd status =
  let code =
    match status with
    | Unix.WEXITED ec -> ec
    | Unix.WSIGNALED s | Unix.WSTOPPED s -> 128 + s
  in
  if code = 0 then ()
  else Cmd.failf ~code:3 "command '%s' exited with code %d" cmd code

let run cmd args =
  let cmdline =
    Filename.quote_command cmd args ~stdin:"/dev/null" ~stdout:"/dev/null"
  in
  let status = wrap_err (fun () -> Unix.system cmdline) in
  exit_result cmd status

let exec cmd args =
  let args' = Array.of_list (cmd :: args) in
  wrap_err (fun () -> Unix.execvp cmd args')

type line_result = Continue | Stop

let lines cmd args f =
  let args' = Array.of_list (cmd :: args) in
  let stdout, stdin = wrap_err (fun () -> Unix.open_process_args cmd args') in
  let status = ref None in
  Fun.protect
    (fun () ->
      close_out_noerr stdin;
      let should_continue = function Continue -> true | _ -> false in
      let rec loop () =
        match In_channel.input_line stdout with
        | Some line when should_continue @@ f line -> loop ()
        | _ -> ()
      in
      loop ())
    ~finally:(fun () -> status := Some (Unix.close_process (stdout, stdin)));
  exit_result cmd @@ Option.get !status
