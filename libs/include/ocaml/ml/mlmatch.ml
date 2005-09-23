(*
 *  NekoML Compiler
 *  Copyright (c)2005 Nicolas Cannasse
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *)

(* -----
	We're using the same algorithm and source code as described in
	chapter 5.2.4 of "The Zinc experiment" by X. Leroy
	( http://citeseer.ist.psu.edu/leroy90zinc.html )
	also described in "The Implementation of Functional Programming Languages"
	by Simon Peyton Jones, in the Chapter 5 by Philip Wadler
	( http://research.microsoft.com/Users/simonpj/Papers/slpj-book-1987/ )
*)

open Mlast
open Mltype

type complete =
	| Total
	| Partial
	| Dubious

type lambda = match_op

type matching = (pattern list * lambda) list * lambda list

let fully_matched_ref = ref (fun cl -> false)
let error_ref = ref (fun (msg : string) (p : pos) -> assert false; ())

let error msg p =
	!error_ref msg p;
	assert false

let failure = MFailure
let handle l1 l2 = MHandle (l1,l2)
let exec e = MExecute e
let cond path lambdas = MConstants (path,lambdas)
let switch path lambdas = MSwitch (path,lambdas)
let ewhen e e2 = MWhen (e,e2)

let rec have_when = function
	| MWhen _ -> true
	| MBind (_,_,e) -> have_when e
	| _ -> false

let t_const = function
	| Int i -> TInt i
	| String s -> TString s
	| Float f -> TFloat f
	| Char c -> TChar c
	| _ -> assert false

let total p1 p2 =
	match p1 , p2 with
	| Total , Total -> Total
	| Partial , _ -> Partial
	| _ , Partial -> Partial
	| _ , _ -> Dubious

let partial p1 p2 = 
	match p1 , p2 with
	| Total , _ -> p2
	| _ , Total -> Total
	| _ , _ -> Dubious

let rec start_by_a_variable (p,_) =
	match p with
	| PAlias (_,p) -> start_by_a_variable p
	| PIdent _ -> true
	| _ -> false

let add_to_match (casel,pathl) cas =
	cas :: casel , pathl

let make_constant_match path cas =
	match path with
	| [] -> assert false
	| _ :: pathl -> [cas] , pathl

let make_construct_match tuple nargs pathl cas =
	match pathl with
	| [] -> assert false
	| path :: pathl ->
		let rec make_path i =
			if i >= nargs then
				pathl
			else
				let k = if tuple then MTuple (path,i) else MField (path,i) in
				k :: make_path (i + 1)
		in
		[cas] , make_path 0

let add_to_division make_match divlist key cas =
	try
		let matchref = List.assoc key divlist in
		matchref := add_to_match !matchref cas;
		divlist
	with Not_found ->
		(key , ref (make_match cas)) :: divlist

let lines_of_matching = fst

let fully_matched cl =
	!fully_matched_ref cl

let flatten = function
	| None -> []
	| Some (PTuple l,_) -> l
	| Some p -> [p]

let split_matching (m:matching) =
	match m with
	| _ , [] ->
		assert false
	| casel, (curpath :: endpathl as pathl) ->
		let rec split_rec = function
			| ((PTyped (p,_),_) :: l , act) :: rest ->
				split_rec ((p :: l, act) :: rest)
			| ((PAlias (var,p),_) :: l , act) :: rest ->
				split_rec ((p :: l, MBind (var,curpath,act)) :: rest)
			| ((PIdent var,_) :: l , act) :: rest ->
				let vars , others = split_rec rest in
				add_to_match vars (l, (if var = "_" then act else MBind (var,curpath,act))) , others
			| casel ->
				([] , endpathl) , (casel , pathl)
		in
		split_rec casel

let divide_matching (m:matching) = 
	match m with
	| _ , [] ->
		assert false
	| casel , (curpath :: tailpathl as pathl) ->
		let rec divide_rec = function
			| [] ->
				[] , [] , ([] , pathl)
			| ([],_) :: _ ->
				assert false
			| ((PTyped (p,_),_) :: l , act) :: rest ->
				divide_rec ((p :: l , act) :: rest)
			| ((PAlias (var,p),_) :: l, act) :: rest ->
				divide_rec ((p :: l , MBind (var,curpath,act)) :: rest)
			| ((PConst c,_) :: l, act) :: rest ->
				let constant , constrs, others = divide_rec rest in
				add_to_division (make_constant_match pathl) constant (t_const c) (l, act), constrs , others
			| ((PConstr (path,c,arg),_) :: l,act) :: rest ->				
				let constants , constrs, others = divide_rec rest in
				let args = flatten arg in
				constants , add_to_division (make_construct_match false (List.length args) pathl) constrs (TModule (path,TConstr c)) (args @ l,act) , others
			| ((PTuple [],_) :: l,act) :: rest ->
				let constants , constrs, others = divide_rec rest in
				constants , add_to_division (make_constant_match pathl) constrs TVoid (l, act), others
			| ((PTuple args,_) :: l,act) :: rest ->
				let constants , constrs, others = divide_rec rest in
				constants , add_to_division (make_construct_match true (List.length args) pathl) constrs TVoid (args @ l,act) , others			
			| casel ->
				[] , [] , (casel,pathl)
		in
		divide_rec casel

let rec conquer_divided_matching = function
	| [] ->
		[], Total, []
	| (key, matchref) :: rest ->
		let l1, p1, u1 = conquer_matching !matchref in
		let l2, p2, u2 = conquer_divided_matching rest in
		(key , l1) :: l2 , total p1 p2 , u1 @ u2

and conquer_matching (m:matching) =
	match m with
	| [] , _ ->
		failure , Partial , []
	| ([],action) :: rest , k ->
		if have_when action then
			let a , p , r = conquer_matching (rest,k) in
			MHandle (action,a) , p , r
		else
			action , Total, rest
	| _ , [] -> 
		assert false
	| (p :: _,_) :: _ , _ :: _ when start_by_a_variable p ->
		let vars , rest = split_matching m in
		let l1, p1, u1 = conquer_matching vars in
		let l2, p2, u2 = conquer_matching rest in
		if p1 = Total then
			l1 , Total, u1 @ lines_of_matching rest
		else
			handle l1 l2 , (if p2 = Total then Total else Dubious) , u1 @ u2
	| _ , path :: _ ->
		match divide_matching m with
		| [] , [] , vars ->
			conquer_matching vars
		| consts , [] , vars ->
			let l1, _ , u1 = conquer_divided_matching consts in
			let l2, p2, u2 = conquer_matching vars in
			handle (cond path l1) l2 , p2 , u1 @ u2
		| [] , constrs , vars ->
			let l1, p1, u1 = conquer_divided_matching constrs in
			let l2, p2, u2 = conquer_matching vars in
			if fully_matched (List.map fst constrs) && p1 = Total then
				switch path l1 , Total , u1 @ lines_of_matching vars
			else
				handle (switch path l1) l2 , partial p1 p2 , u1 @ u2
		| _ ->
			assert false

let make (cases : (pattern list * texpr option * texpr) list) p =
	let cases = List.concat (List.map (fun (pl,wcond,e) ->		
		let e = exec e in 
		let e = (match wcond with None -> e | Some e2 -> ewhen e2 e) in
		List.map (fun p -> [p] , e) pl
	) cases) in
	let m = cases , [MRoot] in
	let lambda, partial, unused = conquer_matching m in
	(match unused with
	| [] -> ()
	| ([] , _ ) :: _ -> error "Some pattern are never matched" p
	| ((_,p) :: _ , _) :: _ -> error "This pattern is never matched" p);
	(match partial with
	| Total -> lambda
	| _ -> error "This matching is not complete" p)
