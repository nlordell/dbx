(** Run child processes.

    This module provides a high-level and convenient API for spawing child
    processes in various forms. *)

let wrap_err f =
  try f ()
  with Unix.Unix_error (err, fn, param) ->
    let msg = Unix.error_message err in
    if String.equal param "" then Cmd.failf ~code:3 "%s: %s" fn msg
    else Cmd.failf ~code:3 "%s: %s '%s'" fn msg param

let success cmd args =
  let cmdline =
    Filename.quote_command cmd args ~stdin:"/dev/null" ~stdout:"/dev/null"
      ~stderr:"/dev/null"
  in
  let status = wrap_err (fun () -> Unix.system cmdline) in
  match status with Unix.WEXITED 0 -> true | _ -> false

let exit_result cmd status =
  let code =
    match status with
    | Unix.WEXITED ec -> ec
    | Unix.WSIGNALED s | Unix.WSTOPPED s -> 128 + s
  in
  if code = 0 then ()
  else Cmd.failf ~code:3 "command '%s' exited with code %d" cmd code

let run cmd args =
  let cmdline = Filename.quote_command cmd args in
  let status = wrap_err (fun () -> Unix.system cmdline) in
  exit_result cmd status

let exec cmd args =
  let args' = Array.of_list (cmd :: args) in
  wrap_err (fun () -> Unix.execvp cmd args')

let pipe cmd args f =
  let args' = Array.of_list (cmd :: args) in
  let process = wrap_err (fun () -> Unix.open_process_args cmd args') in
  let status = ref None in
  let result =
    Fun.protect
      (fun () -> f process)
      ~finally:(fun () -> status := Some (Unix.close_process process))
  in
  exit_result cmd @@ Option.get !status;
  result

let output cmd args =
  pipe cmd args (fun (stdout, stdin) ->
      close_out_noerr stdin;
      In_channel.input_all stdout)

type line_result = Continue | Stop

let lines cmd args f =
  pipe cmd args (fun (stdout, stdin) ->
      close_out_noerr stdin;
      let should_continue = function Continue -> true | _ -> false in
      let rec loop () =
        match In_channel.input_line stdout with
        | Some line when should_continue @@ f line -> loop ()
        | _ -> ()
      in
      loop ())
