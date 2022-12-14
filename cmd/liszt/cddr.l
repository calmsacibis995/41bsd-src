(include "caspecs.l")
(eval-when (compile)
  (fasl 'camacs))

(setq sectioncddrid "@(#)cddr.l	5.4	11/11/80")  ; id for SCCS

; cc-not :: compile a "not" or "null" expression		= cc-not =
;
(defun cc-not nil
  (makecomment '(beginning not))
  (If (null g-loc)
      then (let ((g-cc (cons (cdr g-cc) (car g-cc)))
		 (g-ret nil))
		(d-exp (cadr v-form)))
      else (let ((finlab (d-genlab))
		 (finlab2 (d-genlab))
		 (g-ret nil))
		; eval arg and jump to finlab if nil
		(let ((g-cc (cons finlab nil))
		      g-loc)
		     (d-exp (cadr v-form)))
		; didn't jump, answer must be t
		(d-move 'T g-loc)
		(If (car g-cc) then (e-goto (car g-cc))
			       else (e-goto finlab2))
		(e-label finlab)
		; answer is nil
		(d-move 'Nil g-loc)
		(If (cdr g-cc) then (e-goto (cdr g-cc)))
		(e-label finlab2))))


;--- cc-numberp :: check for numberness				= cc-numberp =
;
(defun cc-numberp nil
  (d-typecmplx (cadr v-form) 
	       '#.(concat '$ (plus 1_2 1_4 1_9))))


;--- cc-or :: compile an "or" expression			= cc-or =
;
(defun cc-or nil
  (let ((finlab (d-genlab))
	(finlab2)
	(exps (If (cdr v-form) thenret else '(nil)))) ; (or) => nil
       (If (null (car g-cc))
	   then (d-exp (do ((g-cc (cons finlab nil))
			    (g-loc (If g-loc then 'reg))
			    (g-ret nil)
			    (ll exps (cdr ll)))
			   ((null (cdr ll)) (car ll))
			   (d-exp (car ll))))
		(If g-loc then (setq finlab2 (d-genlab))
			       (e-goto finlab2)
			       (e-label finlab)
			       (d-move 'reg g-loc)
			       (e-label finlab2)
			  else (e-label finlab))
	   else (If (null g-loc) then (setq finlab (car g-cc)))
		(d-exp (do ((g-cc (cons finlab nil))
			    (g-loc (If g-loc then 'reg))
			    (g-ret nil)
			    (ll exps (cdr ll)))
			   ((null (cdr ll)) (car ll))
			   (d-exp (car ll))))
		(If g-loc then (setq finlab2 (d-genlab))
			       (e-goto finlab2)
			       (e-label finlab)
			       (d-move 'reg g-loc)
			       (e-goto (car g-cc))	; result is t
			       (e-label finlab2)))
       (d-clearreg)))  ; we are not sure of the state due to possible branches.
			       

;--- c-prog :: compile a "prog" expression			= c-prog =
;
; for interlisp compatibility, we allow the formal variable list to
; contain objects of this form (vrbl init) which gives the initial value
; for that variable (instead of nil)
;
(defun c-prog nil
  (let (g-loc g-cc seeninit initf ((spcs locs initsv . initsn) 
				   (d-classify (cadr v-form)))
	(p-rettrue g-ret) (g-ret nil))

       (e-pushnil (length locs))	; locals initially nil
       (d-bindprg spcs locs)		; bind locs and specs

       (cond (initsv (d-pushargs initsv)
		     (mapc '(lambda (x)
				    (d-move 'unstack (d-loc x))
				    (decr g-loccnt)
				    (unpush g-locs))
			   (nreverse initsn))))
       
       ; determine all possible labels
       (do ((ll (cddr v-form) (cdr ll))
	    (labs nil))
	   ((null ll) (setq g-labs `((,(d-genlab) ,@labs)
				     ,@g-labs)))
	   (If (and (car ll) (symbolp (car ll)))
	       then (If (assq (car ll) labs)
			then (comp-err "label is mulitiply defined " (car ll))
			else (setq labs (cons (cons (car ll) (d-genlab))
					      labs)))))
       
       ; compile each form which is not a label
       (d-clearreg)		; unknown state after binding
       (do ((ll (cddr v-form) (cdr ll)))
	   ((null ll))
	   (If (or (null (car ll)) (not (symbolp (car ll))))
	       then (d-exp (car ll))
	       else (e-label (cdr (assq (car ll) (cdar g-labs))))
		    (d-clearreg))))		; dont know state after label
  
  ; result is nil if fall out and care about value
  (If (or g-cc g-loc) then (d-move 'Nil 'reg))
  
  (e-label (caar g-labs))		; return to label
  (setq g-labs (cdr g-labs))
  (d-unbind))			; unbind our frame


;--- d-bindprg :: do binding for a prog expression
;	- spcs : list of special variables
;	- locs : list of local variables
;	- specinit : init values for specs (or nil if all are nil)
;
(defun d-bindprg (spcs locs)


	; place the local vrbls and prog frame entry on the stack
	(setq g-loccnt (+ g-loccnt (length locs))
	      g-locs (nconc locs `((prog . ,(length spcs)) ,@g-locs)))
	 
	; now bind the specials, if any, to nil
	(If spcs then (e-setupbind)
	    	      (mapc '(lambda (vrb)
				     (e-shallowbind vrb 'Nil))
			    spcs)
	              (e-unsetupbind)))

;--- d-unbind :: remove one frame from g-locs
;
(defun d-unbind nil
  (do ((count 0 (1+ count)))
      ((dtpr (car g-locs))
       (If (not (zerop (cdar g-locs)))
	   then (e-unshallowbind (cdar g-locs)))
       (cond ((not (zerop count))
	      (e-dropnp count)
	      
	      (setq g-loccnt (- g-loccnt count))))
       (setq g-locs (cdr g-locs)))
      (setq g-locs (cdr g-locs))))
	

;--- d-classify :: seperate variable list into special and non-special
;	- lst : list of variables
; returns ( xxx yyy zzz . aaa) 
;		where xxx is the list of special variables and
;		yyy is the list of local variables
;		zzz are the non nil initial values for prog variables
;		aaa are the names corresponding to the values in zzz
;
(defun d-classify (lst)
  (do ((ll lst (cdr ll))
       (locs) (spcs) (init) (initsv) (initsn) 
         (name))
      ((null ll) (cons spcs (cons locs (cons initsv initsn))))
      (If (atom (car ll)) then (setq name (car ll))
			  else (setq name (caar ll))
			       (Push initsn name)
			       (Push initsv (cadar ll)))
      (If (d-specialp name)
	  then (Push spcs name)
	  else (Push locs name))))

; cm-progn :: compile a "progn" expression			= cm-progn =
;
(defun cm-progn nil
  `((lambda nil ,@(cdr v-form))))


; cm-prog1 :: compile a "prog1" expression			= cm-prog1 =
;
(defun cm-prog1 nil
  (let ((gl (d-genlab)))
       `((lambda (,gl) 
		 ,@(cddr v-form)
		 ,gl)
	 ,(cadr v-form))))


; cm-prog2 :: compile a "prog2" expression			= cm-prog2 =
;
(defun cm-prog2 nil
  (let ((gl (d-genlab)))
       `((lambda (,gl) ,(cadr v-form)
		       (setq ,gl ,(caddr v-form))
		       ,@(cdddr v-form)
		       ,gl)
	 nil)))


;--- cc-quote : compile a "quote" expression			= cc-quote =
; 
; if we are just looking to set the ; cc, we just make sure 
; we set the cc depending on whether the expression quoted is
; nil or not.
(defun cc-quote nil
  (let ((arg (cadr v-form))
	argloc)

       (If (null g-loc) 
	   then (If (and (null arg) (cdr g-cc)
		    then (e-goto (cdr g-cc))
		 elseif (and arg (car g-cc))
		    then (e-goto (car g-cc)))
		 elseif (null g-cc)
		    then (comp-warn "losing the value of this expression " (or v-form)))
	else (d-move (d-loclit arg nil) g-loc)
	     (d-handlecc))))


;--- d-loc :: return the location of the variable or value in IADR form 
;	- form : form whose value we are to locate
;
; if we are given a xxx as form, we check yyy;
;	xxx		yyy
;     --------	     ---------
;	nil		Nil is always returned
;	symbol		return the location of the symbols value, first looking
;		     in the registers, then on the stack, then the bind list.
;		     If g-ingorereg is t then we don't check the registers.
;		     We would want to do this if we were interested in storing
;		     something in the symbol's value location.
;	number		always return the location of the number on the bind
;		     list (as a (lbind n))
;	other	     	always return the location of the other on the bind
;		     list (as a (lbind n))
;
(defun d-loc (form)
  (If (null form) then 'Nil
   elseif (numberp form) then 
          (If (and (fixp form) (greaterp form -1025) (lessp form 1024))
	      then `(fixnum ,form)		; small fixnum
	      else (d-loclit form nil))
   elseif (symbolp form) 
       then (If (and (null g-ignorereg) (car (d-bestreg form nil))) thenret
		else (If (d-specialp form) then (d-loclit form t)
				  else 
				    (do ((ll g-locs (cdr ll))	; check stack
					 (n g-loccnt))	
					((null ll) 
					 (comp-warn (or form) " declared special by compiler")
					 (d-makespec form)
					 (d-loclit form t))
					(If (atom (car ll))
					    then (If (eq form (car ll))
						     then (return `(stack ,n))
						     else (setq n (1- n)))))))
    else (d-loclit form nil)))


;--- d-loclit :: locate or add litteral to bind list
;	- form : form to check for and add if not present
;	- flag : if t then if we are given a symbol, return the location of
;		 its value, else return the location of the symbol itself
;
; scheme: we share the locations of atom (symbols,numbers,string) but always
;	 create a fresh copy of anything else.
(defun d-loclit (form flag)
  (prog (loc onplist symboltype)
	(If (null form) 
	    then (return 'Nil)
	 elseif (symbolp form)
	    then (setq symboltype t)
		 (cond ((setq loc (get form g-bindloc))
			(setq onplist t)))
	 elseif (atom form)
	    then (do ((ll g-lits (cdr ll))	; search for atom on list
		      (n g-litcnt (1- n)))
		     ((null ll))
		     (If (eq form (car ll))
			 then (setq loc n)	; found it
			 (return))))	; leave do
	(If (null loc)
	    then (Push g-lits form)
		 (setq g-litcnt (1+ g-litcnt)
		       loc g-litcnt)
		 (cond ((and symboltype (null onplist))
			(putprop form loc g-bindloc))))

       (return (If (and flag symboltype) then `(bind ,loc)
				     else `(lbind ,loc)))))
			     


;--- d-locv :: find the location of a value cell, and dont return a register
;
(defun d-locv (sm)
  (let ((g-ignorereg t))
       (d-loc sm)))


;--- c-setarg :: set a lexpr's arg				= cc-setarg  =
; form is (setarg index value)
;
(defun c-setarg nil
  (If (not (eq 'lexpr g-ftype))
      then (comp-err "setarg only allowed in lexprs"))
  (If (and fl-inter (eq (length (cdr v-form)) 3))	; interlisp setarg
      then (If (not (eq (cadr v-form) (car g-args)))
	       then (comp-err "setarg: can only compile local setargs " v-form)
	       else (setq v-form (cdr v-form))))
   (d-pushargs (list (cadr v-form)))    ; stack index
   (let ((g-loc 'reg)
	 (g-cc nil)
	 (g-ret nil))
	(d-exp (caddr v-form)))
   (d-clearreg 'r1)			; indicate we are clobbering r1
   (e-write3 'movl `(* -4 #.Np-reg) 'r1)	; actual number to r1
   (e-write3 'movl 'r0 "*-4(fp)[r1]")	; store value in
   (e-pop 1)
   (unpush g-locs)
   (decr g-loccnt))

;--- cc-stringp :: check for string ness			= cc-stringp =
;
(defun cc-stringp nil
  (d-typesimp (cadr v-form) '$0))


;--- cc-symbolp :: check for symbolness				= cc-symbolp =
;
(defun cc-symbolp nil
  (d-typesimp (cadr v-form) '$1))



;--- c-return :: compile a "return" statement			= c-return =
;
(defun c-return nil
  ; value is always put in r0
  (let ((g-loc 'reg)
	g-cc
	g-ret)
       (d-exp (cadr v-form)))

  ; if we are doing a non local return, compute number of specials to unbind
  ; and locals to pop
  (If (car g-labs) then (e-goto (caar g-labs))
      else (do ((loccnt 0)
		(speccnt 0)
		(ll g-labs (cdr ll))
		(locs g-locs))
	       ((null ll) (comp-err "return used not within a prog or do"))
	       (If (car ll) then  (comp-warn " non local return used ")
				      ; unbind down to but not including
				      ; this frame.
				      (If (greaterp loccnt 0)
					  then (e-pop loccnt))
				      (If (greaterp speccnt 0)
					  then (e-unshallowbind speccnt))
				     (e-goto (caar ll))
				     (return)
		 else ; determine number of locals and special on 
		      ; stack for this frame, add to running
		      ; totals
		      (do ()
			  ((dtpr (car locs))
			   (setq speccnt (+ speccnt (cdar locs))
				 locs (cdr locs)))
			  (incr loccnt)
			  (setq locs (cdr locs)))))))
					     
	 
; c-rplaca :: compile a "rplaca" expression			= c-rplaca =
;
(defun c-rplaca nil
  (let ((ssimp (d-simple (caddr v-form)))
	(g-ret nil))
       (let ((g-loc (If ssimp then 'reg else 'stack))
	     (g-cc nil))
	    (d-exp (cadr v-form)))
       (If (null ssimp) then (Push g-locs nil)
	                     (incr g-loccnt)
			     (let ((g-loc 'r1)
				   (g-cc nil))
				  (d-exp (caddr v-form)))
	                     (d-move 'unstack 'reg)
			     (unpush g-locs)
			     (decr g-loccnt)
			     (e-move 'r1 '(4 r0))
	   else (e-move (e-cvt ssimp)  '(4 r0)))
       (d-clearreg)))		; cant tell what we are clobbering


; c-rplacd :: compile a "rplacd" expression			= c-rplacd =
;
(defun c-rplacd nil
  (let ((ssimp (d-simple (caddr v-form)))
	(g-ret nil))
       (let ((g-loc (If ssimp then 'reg else 'stack))
	     (g-cc nil))
	    (d-exp (cadr v-form)))
       (If (null ssimp) then (Push g-locs nil)
	                     (incr g-loccnt)
			     (let ((g-loc 'r1)
				   (g-cc nil))
				  (d-exp (caddr v-form)))
	                     (d-move 'unstack 'reg)
			     (unpush g-locs)
			     (decr g-loccnt)
			     (e-move 'r1 '(0 r0))
	   else (e-move (e-cvt ssimp)  '(0 r0)))
       (d-clearreg)))

; c-set :: compile a "set" expression				= c-set =


;--- cc-setq :: compile a "setq" expression			= c-setq = 
;
(defun cc-setq nil
  (let (tmp)
       (If (oddp (length (cdr v-form)))
	   then (comp-err "wrong number of args to setq "
			  (or v-form))
	elseif (cdddr v-form)		; if multiple setq's
	   then (do ((ll (cdr v-form) (cddr ll))
		     (g-loc)
		     (g-cc nil))
		    ((null (cddr ll)) (setq tmp ll))
		    (setq g-loc (d-locv (car ll)))
		    (d-exp (cadr ll))
		    (d-clearuse (car ll)))
	else (setq tmp (cdr v-form)))

       ; do final setq
       (let ((g-loc (d-locv (car tmp)))
	     (g-cc (If g-loc then nil else g-cc))
	     (g-ret nil))
	    (d-exp (cadr tmp))
	    (d-clearuse (car tmp)))
       (If g-loc then (d-move (d-locv (car tmp)) g-loc)
		      (If g-cc then (d-handlecc)))))



; cc-typep :: compile a "typep" expression			= cc-typep =
; 
; this returns the type of the expression, it is always non nil
;
(defun cc-typep nil
  (let ((argloc (d-simple (cadr v-form)))
	(g-ret))
       (If (null argloc) then (let ((g-loc 'reg) g-cc)
				   (d-exp (cadr v-form)))
			      (setq argloc 'reg))
       (If g-loc then (e-write4 'ashl '$-9 (e-cvt argloc) 'r0)
	              (e-write3 'cvtbl "_typetable+1[r0]" 'r0)
		      (e-write3 'movl "_tynames+4[r0]" 'r0)
		      (e-write3 'movl "(r0)" (e-cvt g-loc)))
       (If (car g-cc) then (e-goto (car g-cc)))))



; cm-symeval :: compile a symeval expression.
; the symbol cell in franz lisp is just the cdr.
;
(defun cm-symeval nil
  `(cdr ,(cadr v-form)))


; c-*throw :: compile a "*throw" expression			=c-*throw =
;
; the form of *throw is (*throw 'tag 'val) .
; we calculate and stack the value of tag, then calculate val 
; we call Idothrow to do the actual work, and only return if the
; throw failed.
;
(defun c-*throw nil
  (let ((arg2loc (d-simple (caddr v-form)))
	g-cc
	g-ret
	arg1loc)
       (If arg2loc then (If (setq arg1loc (d-simple (cadr v-form)))
			    then (e-write2 'pushl (e-cvt arg2loc))
				 (e-write2 'pushl (e-cvt arg1loc))
			    else (let ((g-loc 'reg))
				      (d-exp (cadr v-form))	; calc tag
				      (e-write2 'pushl (e-cvt arg2loc))
				      (e-write2 'pushl (e-cvt 'reg))))
		   else (let ((g-loc 'stack))
			     (d-exp (cadr v-form))	; calc tag to stack
			     (Push g-locs nil)
			     (incr g-loccnt)
			     (setq g-loc 'reg)	
			     (d-exp (caddr v-form))	; calc value into r0
			     (e-write2 'pushl (e-cvt 'reg))
			     (e-write2 'pushl (e-cvt 'unstack))
			     (unpush g-locs)
			     (decr g-loccnt)))
       (e-write3 'calls '$0 '_Idothrow)
       (e-write2 'clrl '"-(sp)")			; non contuable error
       (e-write2 'pushab '__erthrow)		; string to print
       (e-write3 'calls '$2 '_error)))



;--- cm-zerop ::  convert zerop to a quick test			= cm-zerop =
; zerop is only allowed on fixnum and flonum arguments.  In both cases,
; if the value of the first 32 bits is zero, then we have a zero.
; thus we can define it as a macro:
(defun cm-zerop nil
  (cond ((atom (cadr v-form))
	 `(and (null (cdr ,(cadr v-form))) (not (bigp ,(cadr v-form)))))
	(t (let ((gnsy (gensym)))
		`((lambda (,gnsy)
			  (and (null (cdr ,gnsy)) 
				(not (bigp ,gnsy))))
		  ,(cadr v-form))))))



;------- FIXNUM arithmetic section ---------
;  beware all ye who read this section 
;



(declare (localf d-upordown d-fixop))

;--- c-1+ :: fixnum add1 function
;
(defun c-1+ nil
  (d-upordown 'addl3))

;--- c-1- :: fixnum sub1 function
;
(defun c-1- nil
  (d-upordown 'subl3))

(defun d-upordown (opcode)
  (let ((arg (cadr v-form))
	argloc)
       (If (setq argloc (d-simple `(cdr ,arg)))
	   then (e-write4 opcode '$1 (e-cvt argloc)  'r5)
	   else (let ((g-loc 'reg)
		      g-ret
		      g-cc)
		     (d-exp arg))
		(e-write4 opcode '$1 "(r0)" 'r5))
       (e-write2 "jsb" "_qnewint")
       (d-clearreg)))


;--- c-+  :: fixnum add						= c-+ =
;
(defun c-+ nil
  (d-fixop 'addl3 'plus))

(defun c-- nil
  (d-fixop 'subl3 'difference))

(defun c-* nil
  (d-fixop 'mull3 'times))

(defun c-/ nil
  (d-fixop 'divl3 'quotient))

(defun c-\\ nil
  (d-fixop 'ediv  'remainder))

(defun d-fixop (opcode lispopcode)
 (prog (op1 op2 rop1 rop2 simpleop1)
  (If (not (eq 3 (length v-form))) ; only handle two ops for now
      then (d-callbig lispopcode (cdr v-form))
      else (setq op1 (cadr v-form)
		 op2 (caddr v-form))
	   (If (fixp op1)
	       then (setq rop1 (concat '$ op1)  ; simple int
			  simpleop1 t)	    
	       else (If (setq rop1 (d-simple `(cdr ,op1)))
			then (setq rop1 (e-cvt rop1))
			else (let ((g-loc 'reg) g-cc g-ret)
				  (d-exp op1))
			     (setq rop1 '|(r0)|)))
	   (If (fixp op2)
	       then (setq rop2 (concat '$ op2))
	       else (If (setq rop2 (d-simple `(cdr ,op2)))
			then (setq rop2 (e-cvt rop2))
			else (e-write3 'movl rop1 "-(sp)")
			     (setq rop1 "(sp)+")
			     (let ((g-loc 'reg)
				   g-cc g-ret)
				  (d-exp op2))
			     (setq rop2 '|(r0)|)))
	   (If (eq opcode 'ediv)
	    then (If (not simpleop1) then (e-write3 'movl rop1 'r2)  ; need quad
					  (e-write4 'ashq '$-32 'r1 'r1)
					  (setq rop1 'r1))      ; word div.
		 (e-write5 'ediv rop2 rop1 'r0 'r5)
	    else (e-write4 opcode rop2 rop1 'r5))

	   (e-write2 'jsb "_qnewint")
	   (d-clearreg))))




;---- d routines (general ones, others are near function using them)



;--- d-cmp :: compare two IADR values
;
(defun d-cmp (arg1 arg2)
  (e-write3 'cmpl (e-cvt arg1) (e-cvt arg2)))


;--- d-handlecc :: handle g-cc
; at this point the Z condition code has been set up and if g-cc is
; non nil, we must jump on condition to the label given in g-cc
;
(defun d-handlecc nil
  (If (car g-cc) then (e-gotot (car g-cc))
   elseif (cdr g-cc) then (e-gotonil (cdr g-cc))))


;--- d-invert :: handle inverted condition codes
; this routine is called if a result has just be computed which alters
; the condition codes such that Z=1 if the result is t, and Z=0 if the
; result is nil (this is the reverse of the usual sense).  The purpose
; of this routine is to handle g-cc and g-loc.  That is if g-loc is 
; specified, we must convert the value of the Z bit of the condition 
; code to t or nil and store that in g-loc.  After handling g-loc we
; must handle g-cc, that is if the part of g-cc is non nil which matches
; the inverse of the current condition code, we must jump to that.
;
(defun d-invert nil
  (If (null g-loc) 
      then (If (car g-cc) then (e-gotonil (car g-cc))
	    elseif (cdr g-cc) then  (e-gotot (cdr g-cc)))
      else (let ((lab1 (d-genlab))
		 (lab2 (If (cdr g-cc) thenret else (d-genlab))))
		(e-gotonil lab1)
		; Z=1, but remember that this implies nil due to inversion
		(d-move 'Nil g-loc)
		(e-goto lab2)
		(e-label lab1)
		; Z=0, which means t
		(d-move 'T g-loc)
		(If (car g-cc) then (e-goto (car g-cc)))
		(If (null (cdr g-cc)) then (e-label lab2)))))
			

;--- d-noninvert :: handle g-cc and g-loc assuming cc non inverted
; 
; like d-invert except Z=0 implies nil, and Z=1 implies t
;
(defun d-noninvert nil
  (If (null g-loc) 
      then (If (car g-cc) then (e-gotot (car g-cc))
	    elseif (cdr g-cc) then  (e-gotonil (cdr g-cc)))
      else (let ((lab1 (d-genlab))
		 (lab2 (If (cdr g-cc) thenret else (d-genlab))))
		(e-gotot lab1)
		; Z=0, this implies nil
		(d-move 'Nil g-loc)
		(e-goto lab2)
		(e-label lab1)
		; Z=1, which means t
		(d-move 'T g-loc)
		(If (car g-cc) then (e-goto (car g-cc)))
		(If (null (cdr g-cc)) then (e-label lab2)))))

;--- d-macroexpand :: macro expand a form as much as possible
;
(defun d-macroexpand (form)
  (prog nil
	loop
	(If (and (dtpr form) 
		 (symbolp (car form))
		 (eq 'macro (d-functyp (car form))))
	    then (setq form (apply (car form) form))
	    (go loop))
	(return form)))

;--- d-makespec :: declare a variable to be special
;
(defun d-makespec (vrb)
  (putprop vrb t g-spec))


;--- d-move :: emit instructions to move value from one place to another
;
(defun d-move (from to)
  (makecomment `(from ,(e-uncvt from) to ,(e-uncvt to)))
  (cond ((eq 'Nil from) (e-write2 'clrl (e-cvt to)))
	(t (e-write3 'movl (e-cvt from) (e-cvt to)))))


;--- d-simple :: see of arg can be addresses in one instruction
; we define simple and really simple as follows
;  <rsimple> ::= number
;		 quoted anything
;		 local symbol
;		 t
;		 nil
;  <simple>  ::= <rsimple>
;		 (cdr <rsimple>)
;		 global symbol
;
(defun d-simple (arg)
  (let (tmp)
       (If (d-rsimple arg) thenret
	elseif (symbolp arg) then (d-loc arg)
	elseif (and (memq (car arg) '(cdr car cddr cdar))
		       (setq tmp (d-rsimple (cadr arg))))
	   then (If (eq 'Nil tmp) then tmp
		 elseif (atom tmp)
		     then (If (eq 'car (car arg)) then `(racc 4 ,tmp)
			   elseif (eq 'cdr (car arg)) then `(racc 0 ,tmp)
			   elseif (eq 'cddr (car arg)) then `(racc * 0 ,tmp)
			   elseif (eq 'cdar (car arg)) then `(racc * 4 ,tmp))
		 elseif (not (eq 'cdr (car arg))) then nil
		 elseif (eq 'lbind (car tmp)) then `(bind ,(cadr tmp))
		 elseif (eq 'stack (car tmp)) then `(vstack ,(cadr tmp))
		 elseif (eq 'fixnum (car tmp)) then `(immed ,(cadr tmp))
		 elseif (atom (car tmp))    then `(0 ,(cadr tmp))
		 else (comp-err "bad arg to d-simple: " (or arg))))))

(defun d-rsimple (arg)
  (If (atom arg) then
      (If (null arg) then 'Nil
       elseif (eq t arg) then 'T
       elseif (or (numberp arg)
		  (memq arg g-locs)) 
	  then (d-loc arg)
       else (car (d-bestreg arg nil)))
   elseif (eq 'quote (car arg)) then (d-loclit (cadr arg) nil)))

;--- d-movespec :: move from loc to loc where the first addr given is
;	           an EIADR
;	- from : EIADR 
;	- to   : IADR
;
(defun d-movespec (from to)
  (makecomment `(fromspec ,from to ,(e-uncvt to)))
  (e-write3 'movl from (e-cvt to)))


;--- d-specialp :: check if a variable is special
; a varible is special if it has been declared as such, or if
; the variable special is t
(defun d-specialp (vrb)
  (or special (get vrb g-spec)))


;--- d-tst :: test the given value (set the cc)
;
(defun d-tst (arg)
  (e-write2 'tstl (e-cvt arg)))

;--- d-typesimp ::  determine the type of the argument 
;
(defun d-typesimp (arg val)
  (let ((argloc (d-simple arg)))
	(If (null argloc) then (let ((g-loc 'reg)
				     g-cc g-ret)
				    (d-exp arg))
			       (setq argloc 'reg))
	(e-write4 'ashl '$-9 (e-cvt argloc) 'r0)
	(e-write3 'cmpb '"_typetable+1[r0]" val)
	(d-invert)))

;--- d-typecmplx  :: determine if arg has one of many types
;	- arg : lcode argument to be evaluated and checked
;	- vals : fixnum with a bit in position n if we are to check type n
;
(defun d-typecmplx (arg vals)
  (let ((argloc (d-simple arg))
	(reg))
       (If (null argloc) then (let ((g-loc 'reg)
				    g-cc g-ret)
				   (d-exp arg))
			      (setq argloc 'reg))
       (setq reg 'r0)
       (e-write4 'ashl '$-9 (e-cvt argloc) reg)
       (e-write3 'cvtbl (concat "_typetable+1[" reg "]") reg)
       (e-write4 'ashl reg '$1 reg)
       (e-write3 'bitw vals reg)
       (d-noninvert)))

       
;---- register handling routines.

;--- d-allocreg :: allocate a register 
;  name - the name of the register to allocate or nil if we should
;	  allocate the least recently used.
;
(defun d-allocreg (name)
  (If name 
      then (let ((av (assoc name g-reguse)))
		(If av then (rplaca (cdr av) (1+ (cadr av)))) ; inc used count
		name)
      else ; find smallest used count
	   (do ((small (car g-reguse))
		(smc (cadar g-reguse))
		(lis (cdr g-reguse) (cdr lis)))
	       ((null lis)
		(rplaca (cdr small) (1+ smc))
		(car small))
	       (If (< (cadar lis) smc)
		   then (setq small (car lis)
			      smc   (cadr small))))))


;--- d-bestreg :: determine the register which is closest to what we have
;  name - name of variable whose subcontents we want
;  pat  - list of d's and a's which tell which part we want
;
(defun d-bestreg (name pat)
  (do ((ll g-reguse (cdr ll))
       (val)
       (best)
       (tmp)
       (bestv -1))
      ((null ll) (If best then (rplaca (cdr best) (1+ (cadr best)))
			       (list (car best)
				     (If (> bestv 0) 
					 then (rplacd (nthcdr (1- bestv) 
							      (setq tmp 
								    (copy pat)))
						      nil)
					      tmp
					 else nil)
				     (nthcdr bestv pat))))
      (If (and (setq val (cddar ll))
	       (eq name (car val)))
	  then (If (> (setq tmp (d-matchcnt pat (cdr val)))
		      bestv)
		   then (setq bestv tmp
			      best  (car ll))))))

;--- d-matchcnt :: determine how many parts of a pattern match
; want - pattern we want to achieve
; have - pattern whose value exists in a register
; 
; we return a count of the number of parts of the pattern match.
; If this pattern will be any help at all, we return a value from 
; 0 to the length of the pattern.
; If this pattern will not work at all, we return a number smaller
; than -1.  
; For `have' to be useful for `want', `have' must be a substring of 
; `want'.  If it is a substring, we return the length of `have'.
; 
(defun d-matchcnt (want have)
  (let ((length 0))
       (If (do ((hh have (cdr hh))
		(ww want (cdr ww)))
	       ((null hh) t)
	       (If (or (null ww) (not (eq (car ww) (car hh))))
		   then (return nil)
		   else (incr length)))
	   then  length
	   else  -2)))



;--- d-clearreg :: clear all values in registers or just one
; if no args are given, clear all registers.
; if an arg is given, clear that register
;
(defun d-clearreg n
  (cond ((zerop n) 
	 (mapc '(lambda (x) (rplaca (cdr x) 0)
		     (rplacd (cdr x) nil))
	       g-reguse))
	(t (let ((av (assoc (arg 1) g-reguse)))
		(If av then (rplaca (cdr av) 0)
			    (rplacd (cdr av) nil))))))


;--- d-clearuse :: clear all register which reference a given variable
;
(defun d-clearuse (varib)
  (mapc '(lambda (x)
		 (If (eq (caddr x) varib) then (rplacd (cdr x) nil)))
	g-reguse))


;--- d-inreg :: declare that a value is in a register
; name - register name
; value - value in a register
;
(defun d-inreg (name value)
  (let ((av (assoc name g-reguse)))
       (If av then (rplacd (cdr av) value))
       name))


;---- e routines 



(defun e-cvt (arg)
  (If     (eq 'reg arg) then 'r0
   elseif (eq 'Nil arg) then '$0
   elseif (eq 'T arg) then (If g-trueloc thenret
			       else (setq g-trueloc (e-cvt (d-loclit t nil))))
   elseif (eq 'stack arg) then '(+ #.Np-reg)
   elseif (eq 'unstack arg) then '(- #.Np-reg)
   elseif (atom arg) then arg
   elseif (dtpr arg) then (If     (eq 'stack (car arg))
			      then `(,(* 4 (1- (cadr arg))) #.oLbot-reg)
			   elseif (eq 'vstack (car arg))
			      then `(* ,(* 4 (1- (cadr arg))) #.oLbot-reg)
			   elseif (eq 'bind (car arg))
			      then `(* ,(* 4 (1- (cadr arg))) #.bind-reg)
			   elseif (eq 'lbind (car arg))
			      then `( ,(* 4 (1- (cadr arg))) #.bind-reg)
			   elseif (eq 'fixnum (car arg))
			      then `(\# ,(cadr arg))
			   elseif (eq 'immed (car arg))
			      then `($ ,(cadr arg))
			   elseif (eq 'racc (car arg))
			      then (cdr arg)
			   else (comp-err " bad arg to e-cvt : "
					  (or arg)))
   else  (comp-warn "bad arg to e-cvt : " (or arg))))


;--- e-uncvt :: inverse of e-cvt, used for making comments pretty
;
(defun e-uncvt (arg)
  (If (atom arg) then (If (eq 'Nil arg) then nil
			  else arg)
   elseif (eq 'stack (car arg))
	  then (do ((i g-loccnt)
		    (ll g-locs))
		   ((and (equal i (cadr arg)) (atom (car ll))) (car ll))
		   (If (atom (car ll)) then (setq ll (cdr ll)
						  i (1- i))
					else (setq ll (cdr ll))))
   elseif (or (eq 'bind (car arg)) (eq 'lbind (car arg)))
	  then (do ((i g-litcnt (1- i))
		    (ll g-lits (cdr ll)))
		   ((equal i (cadr arg)) (cond ((eq 'lbind (car arg))
						(list 'quote (car ll)))
					       (t (car ll)))))
   else arg))

;--- e-cvtas :: convert an EIADR to vax unix assembler fmt and print it
;	- form : an EIADR form
;
(defun e-cvtas (form)
  (If (atom form)
      then (sfilewrite form)
   else (If (eq '* (car form)) then (If (eq '\# (cadr form))
					then (setq form `($ ,(caddr form)))
					else (sfilewrite "*")
					     (setq form (cdr form))))
	(If (numberp (car form))
	    then (sfilewrite (car form))
		 (sfilewrite "(")
		 (sfilewrite (cadr form))
		 (sfilewrite ")")
		 (If (caddr form)
		     then (sfilewrite "[")
			  (sfilewrite (caddr form))
			  (sfilewrite "]"))
	elseif (eq '+ (car form))
	    then (sfilewrite '"(")
		 (sfilewrite (cadr form))
		 (sfilewrite '")+")
	elseif (eq '- (car form))
	    then (sfilewrite '"-(")
		 (sfilewrite (cadr form))
		 (sfilewrite '")")
	elseif (eq '\# (car form))	; 5120 is base of small fixnums
	    then (sfilewrite (concat "$" (+ (* (cadr form) 4) 5120)))
	elseif (eq '$ (car form))
	    then (sfilewrite '"$")
		 (sfilewrite (cadr form)))))
;--- e-cmp :: emit code to compare the two given args
;	- arg1, arg2 : EIADRs
;
(defun e-cmp (arg1 arg2)
  (e-write3 'cmpl arg1 arg2))

;--- e-docomment :: print any comment lines
;
(defun e-docomment nil
  (If g-comments
      then (do ((ll (nreverse g-comments) (cdr ll)))
	       ((null ll))
	       (sfilewrite '"	#")
	       (sfilewrite (car ll))
	       (terpr vp-sfile))
	   (setq g-comments nil)
      else (terpr vp-sfile)))
;--- e-goto :: emit code to jump to the location given
;
(defun e-goto (lbl)
  (e-jump lbl))

;--- e-gotonil :: emit code to jump if nil was last computed
;
(defun e-gotonil (lbl)
  (e-write2  'jeql lbl))

;--- e-gotot :: emit code to jump if t was last computed
(defun e-gotot (lbl)
  (e-write2  'jneq lbl))

;--- e-label :: emit a label
(defun e-label (lbl)
  (setq g-skipcode nil)
  (e-writel lbl))

;--- e-move :: move value from one place to anther
; this corresponds to d-move except the args are EIADRS
;
(defun e-move (from to)
  (If (equal 0 from) then (e-write2 'clrl to)
		     else (e-write3 'movl from to)))

;--- e-pop :: pop the given number of args from the stack
; g-locs is not! fixed
;
(defun e-pop (nargs)
  (If (greaterp nargs 0)
      then (e-dropnp nargs)))


;--- e-pushnil :: push a given number of nils on the stack
;
(defun e-pushnil (nargs)
  (do ((i nargs))
      ((zerop i))
      (If (greaterp i 1) then (e-write2  'clrq np-plus)
			      (setq i (- i 2))
	elseif (equal i 1) then (e-write2 'clrl np-plus)
				(setq i (1- i)))))

;--- e-tst :: test a value, arg is an EIADR
;
(defun e-tst (arg)
  (e-write2 'tstl arg))
;--- e-setupbind :: setup for shallow binding
;
(defun e-setupbind nil
  (e-write3 'movl '#.Bnp-val '#.bNp-reg))

;--- e-unsetupbind :: restore temp value of bnp to real loc
;
(defun e-unsetupbind nil
  (e-write3 'movl '#.bNp-reg '#.Bnp-val))

;--- e-shallowbind :: shallow bind value of variable and initialize it
;	- name : variable name
;	- val : IADR value for variable
;
(defun e-shallowbind (name val)
  (let ((vloc (d-loclit name t)))
       (e-write3 'movl (e-cvt vloc) '(+ #.bNp-reg))	; store old val
       (e-write3 'movl (e-cvt `(lbind ,@(cdr vloc)))
		       '(+ #.bNp-reg))		; now name
       (d-move val vloc)))		

;--- e-unshallowbind :: un shallow bind n variable from top of stack
;
(defun e-unshallowbind (n)
  (e-setupbind)		; set up binding register
  (do ((i 1 (1+ i)))
      ((greaterp i n))
      (e-write3 'movl `(,(* -8 i) ,bNp-reg) `(* ,(+ 4 (* -8 i)) ,bNp-reg)))
  (e-write4 'subl3 `($ ,(* 8 n)) bNp-reg Bnp-val))

;----------- very low level routines
; all output to the assembler file goes through these routines.
; They filter out obviously extraneous instructions as well as 
; combine sequential drops of np.

;--- e-dropnp :: unstack n values from np.
; rather than output the instruction now, we just remember that it
; must be done before any other instructions are done.  This will
; enable us to catch sequential e-dropnp's
;
(defun e-dropnp (n)
  (If (not g-skipcode)
      then (setq g-dropnpcnt (+ n (If g-dropnpcnt thenret else 0)))))

;--- em-checknpdrop :: check if we have a pending npdrop
; and do it if so.
;
(defmacro em-checknpdrop nil
  `(If g-dropnpcnt then (let ((dr g-dropnpcnt))
			     (setq g-dropnpcnt nil)
			     (e-write3 'subl2 `($ ,(* dr 4)) Np-reg))))

;--- em-checkskip :: check if we are skipping this code due to jump
;
(defmacro em-checkskip nil
  '(If g-skipcode then (sfilewrite "# ")))


;--- e-jump :: jump to given label
; and set g-skipcode so that all code following until the next label
; will be skipped.
;
(defun e-jump (l)
  (em-checknpdrop)
  (e-write2 'jbr l)
  (setq g-skipcode t))

;--- e-return :: do return, and dont check for np drop
;
(defun e-return nil
  (setq g-dropnpcnt nil)  ; we dont need to worry about nps
  (e-write1 'ret))


;--- e-writel :: write out a label
;
(defun e-writel (label)
  (setq g-skipcode nil)
  (em-checknpdrop)
  (sfilewrite label)
  (sfilewrite '":")
  (e-docomment))

;--- e-write1 :: write out one litteral
;
(defun e-write1 (lit)
  (em-checkskip)
  (em-checknpdrop)
  (sfilewrite lit)
  (e-docomment))

;--- e-write2 :: write one one litteral, and one operand
;
(defun e-write2 (lit frm)
  (em-checkskip)
  (em-checknpdrop)
  (sfilewrite lit)
  (sfilewrite '"	")
  (e-cvtas frm)
  (e-docomment))

;--- e-write3 :: write one one litteral, and two operands
;
(defun e-write3 (lit frm1 frm2)
  (em-checkskip)
  (em-checknpdrop)
  (sfilewrite lit)
  (sfilewrite '"	")
  (e-cvtas frm1)
  (sfilewrite '",")
  (e-cvtas frm2)
  (e-docomment))

;--- e-write4 :: write one one litteral, and three operands
;
(defun e-write4 (lit frm1 frm2 frm3)
  (em-checkskip)
  (em-checknpdrop)
  (sfilewrite lit)
  (sfilewrite '"	")
  (e-cvtas frm1)
  (sfilewrite '",")
  (e-cvtas frm2)
  (sfilewrite '",")
  (e-cvtas frm3)
  (e-docomment))


;--- e-write5 :: write one one litteral, and four operands
;
(defun e-write5 (lit frm1 frm2 frm3 frm4)
  (em-checkskip)
  (em-checknpdrop)
  (sfilewrite lit)
  (sfilewrite '"	")
  (e-cvtas frm1)
  (sfilewrite '",")
  (e-cvtas frm2)
  (sfilewrite '",")
  (e-cvtas frm3)
  (sfilewrite '",")
  (e-cvtas frm4)
  (e-docomment))
