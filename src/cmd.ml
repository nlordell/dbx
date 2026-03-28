(** Command line interface errors. *)

exception Cmd_error of { code : int; message : string; details : string option }

let fail ?(code = 1) ?details message =
  raise (Cmd_error { code; message; details })

let failf ?code ?details fmt = Printf.ksprintf (fail ?code ?details) fmt

type run = string array -> unit
type subcommand = string * run * string

let run ?default cs =
  let ( .?() ) a i = try Some a.(i) with _ -> None in
  let oor a b = match a with Some a -> Some a | None -> b in
  let name () = Option.value Sys.argv.?(0) ~default:"(?)" in
  let commands () =
    let d = List.filter (fun (_, _, doc) -> String.length doc > 0) cs in
    let w =
      List.map (fun (name, _, _) -> String.length name) d
      |> List.fold_left Int.max 0
    in
    let b = Buffer.create 128 in
    Buffer.add_string b "dbx [command]\n";
    List.iter
      (fun (name, _, doc) -> Printf.bprintf b "  %-*s  %s\n" w name doc)
      d;
    Buffer.contents b
  in
  let cmd, run =
    match oor Sys.argv.?(1) default with
    | Some "-h" | Some "--help" ->
        print_string (commands ());
        exit 0
    | Some cmd ->
        begin match
          List.find_map
            (fun (name, run, _) -> if cmd = name then Some run else None)
            cs
        with
        | Some run -> (cmd, run)
        | None ->
            Printf.eprintf "%s: unknown command '%s'.\n%s" (name ()) cmd
              (commands ());
            exit 2
        end
    | None ->
        Printf.eprintf "%s: missing command.\n%s" (name ()) (commands ());
        exit 2
  in
  let argv =
    let len = Array.length Sys.argv in
    let rest = if len > 1 then Array.sub Sys.argv 1 (len - 1) else [| "" |] in
    rest.(0) <- Printf.sprintf "%s %s" (name ()) cmd;
    rest
  in
  try run argv with
  | Cmd_error { code; message; details } ->
      Printf.eprintf "ERROR: %s" message |> prerr_newline;
      Option.iter prerr_endline details;
      exit code
  | ex ->
      Printexc.to_string ex
      |> Printf.eprintf "ERROR: unxpected exception: %s\nThis is a bug!"
      |> prerr_newline;
      exit 125

module Args = struct
  let badf fmt = Printf.ksprintf (fun s -> raise (Arg.Bad s)) fmt

  let name () =
    let value = ref "dbx" in
    let set = ref false in
    let spec =
      ( "-n",
        Arg.String
          (fun v ->
            if not !set then begin
              value := v;
              set := true
            end
            else badf "option '-n' can only be specified once"),
        "<name> Development container name. [default: dbx]" )
    in
    (value, spec)

  let single args what =
    match args with
    | [] -> badf "exepected %s" what
    | [ v ] -> v
    | _ :: x :: _ -> badf "unexpected argument '%s'" x
end

let parse ?anon argv specs msg =
  let ( .?() ) a i = try Some a.(i) with _ -> None in
  let name () = Option.value argv.?(0) ~default:"(?)" in
  let help_msg = ref (fun () -> "") in
  let help () = raise (Arg.Help (!help_msg ())) in
  let specs' =
    Arg.(
      align @@ specs
      @ [
          ("-h", Unit help, " Display this list of options.");
          ("--help", Unit help, "");
          (* hide '-help' *)
          ("-help", Unit (fun () -> Args.badf "unknown option '-help'"), "");
        ])
    |> List.sort (fun (a, _, _) (b, _, _) -> String.compare a b)
  in
  let specs', anon', finish =
    match anon with
    | Some anon -> (
        let args = ref [] in
        let rest = ref false in
        ( ( "--",
            Arg.Rest_all
              (fun a ->
                if List.length !args > 0 then
                  Args.badf "unexpected separator '--'";
                rest := true;
                anon a),
            "" )
          :: specs',
          (fun arg -> args := arg :: !args),
          fun () ->
            if not !rest then
              try anon @@ List.rev !args
              with Arg.Bad msg ->
                Args.badf "%s: %s.\n%s" (name ()) msg (!help_msg ()) ))
    | None ->
        ( specs',
          (fun arg -> Args.badf "unexpected argument '%s'" arg),
          fun () -> () )
  in

  (help_msg := fun () -> Arg.usage_string specs' msg);
  try Arg.parse_argv argv specs' anon' msg |> finish with
  | Arg.Bad msg ->
      prerr_string msg;
      exit 2
  | Arg.Help msg ->
      print_string msg;
      exit 0
