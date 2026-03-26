(** Command line interface errors. *)

exception Cmd_error of { code : int; message : string; details : string option }

let fail ?(code = 1) ?details message =
  raise (Cmd_error { code; message; details })

let failf ?code ?details fmt = Printf.ksprintf (fail ?code ?details) fmt

let run main =
  try main () with
  | Cmd_error { code; message; details } ->
      Printf.eprintf "ERROR: %s" message |> prerr_newline;
      Option.iter prerr_endline details;
      exit code
  | ex ->
      Printexc.to_string ex
      |> Printf.eprintf "ERROR: unxpected exception: %s\nThis is a bug!"
      |> prerr_newline;
      exit 125
