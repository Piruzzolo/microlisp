/* MICHAEL LAZEAR 2016
Implements McCarthy's 5 LISP primitives in ANSI C:
atom, eq, car, cdr, cons
+ label, lambda, and quote */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include "lisp.h"

object_t* GLOBAL_ENV;

bool is_tagged(object_t* exp, object_t* tag) {
	if (!null(exp) && exp->type == CONS)
		return eq(car(exp), tag);
	return false;
}

object_t* make_lambda(object_t* params, object_t* body) {
	return cons(LAMBDA, cons(params, body));
}

object_t* make_procedure(object_t* params, object_t* body, object_t* env) {
	return cons(PROCEDURE, cons(params, cons(body, cons(env, &nil))));
	//return cons(PROCEDURE, cons(params, cons(body, env)));
}

object_t* evlis(object_t* sexp, object_t* env) {
	if ( null(sexp))
		return &nil;
	object_t* ret = cons(eval(car(sexp), env), evlis(cdr(sexp), env));
	return ret;
}

object_t* eval_sequence(object_t* exps, object_t* env) {
	//print(car(exps));
	if (null(cdr(exps))) {
		return eval(car(exps), env);
	}
	
	eval(car(exps), env);
	return eval_sequence(cdr(exps), env);
}

object_t* extend_env(object_t* var, object_t* val, object_t* env) {
	return cons(cons(var, val), env);
}

object_t* lookup_variable(object_t* var, object_t* env) {
	while (!null(env)) {
		object_t* frame = first_frame(env);
		object_t* vars 	= frame_variables(frame);
		object_t* vals 	= frame_values(frame);
		while(!null(vars)) {
			if (eq(car(vars), var))
				return car(vals);
			vars = cdr(vars);
			vals = cdr(vals);
		}
		env = enclosing_env(env);
	}
	return &UNBOUND;
}

/* set_variable binds var to val in the first frame in which var occurs */
void set_variable(object_t* var, object_t* val, object_t* env) {
	while (!null(env)) {
		object_t* frame = first_frame(env);
		object_t* vars 	= frame_variables(frame);
		object_t* vals 	= frame_values(frame);
		while(!null(vars)) {
			if (eq(car(vars), var)) {
				vals->car = val;
				return;
			}
			vars = cdr(vars);
			vals = cdr(vals);
		}
		env = enclosing_env(env);
	}
}

/* define_variable binds var to val in the *current* frame */
void define_variable(object_t* var, object_t* val, object_t* env) {
	object_t* frame = first_frame(env);
	object_t* vars = frame_variables(frame);
	object_t* vals = frame_values(frame);

	while(!null(vars)) {
		if (eq(var, car(vars))) {
			vals->car = val;
			return;
		}
		vars = cdr(vars);
		vals = cdr(vals);
	}
	add_binding(var, val, frame);
}


/* (define (apply procedure arguments)
  (cond ((primitive-procedure? procedure)
         (apply-primitive-procedure procedure arguments))
        ((compound-procedure? procedure)
         (eval-sequence
           (procedure-body procedure)
           (extend-environment
             (procedure-parameters procedure)
             arguments
             (procedure-environment procedure))))
        (else
         (error
          "Unknown procedure type - APPLY" procedure))))
*/
object_t* apply(object_t* procedure, object_t* arguments) {
	if (procedure->type == PRIM) 
		return procedure->primitive(arguments);
	else if (is_tagged(procedure, PROCEDURE)) {
		object_t* env = extend_env(procedure_params(procedure), arguments, procedure_env(procedure));
		return eval_sequence(procedure_body(procedure), env);
	}
	else {
		print(procedure);
		error("Unknown procedure type");
	}

}


/*
(define (eval exp env)
  (cond ((self-evaluating? exp) exp)
        ((variable? exp) (lookup-variable-value exp env))
        ((quoted? exp) (text-of-quotation exp))
        ((assignment? exp) (eval-assignment exp env))
        ((definition? exp) (eval-definition exp env))
        ((if? exp) (eval-if exp env))
        ((lambda? exp)
         (make-procedure (lambda-parameters exp)
                         (lambda-body exp)
                         env))
        ((begin? exp) 
         (eval-sequence (begin-actions exp) env))
        ((cond? exp) (eval (cond->if exp) env))

        application == pair?
        operator == (car exp)
        operand == (cdr exp)
        ((application? exp)
         (apply (eval (operator exp) env)
                (list-of-values (operands exp) env)))
        (else
         (error "Unknown expression type - EVAL" exp))))
*/

object_t* eval(object_t* exp, object_t* env) {

tail:
	printf("%zu\n", (unsigned long long) env);
	if (null(exp)) 
		return &nil;
 	if (exp->type == INT || exp->type == STRING) 
 		return exp;
 	else if (exp->type == SYM) 
 		return lookup_variable(exp, env);
 	else if (is_tagged(exp, QUOTE))
 		return cadr(exp);
 	else if (is_tagged(exp, SET)) {
 		set_variable(cadr(exp), eval(caddr(exp), env), env);
 		exp = cadr(exp);
 		return OK;
 	} else if (is_tagged(exp, DEFINE)) {
		if ((atom(cadr(exp)))) {
			define_variable(cadr(exp), eval(caddr(exp), env), env);
		} else {
			object_t* closure = eval(make_lambda(cdr(cadr(exp)), cddr(exp)), env);
			define_variable(car(cadr(exp)), closure, env);
		}
		return OK;
 	} 
 	else if (is_tagged(exp, new_sym("print"))) {
 		print((object_t*) cadr(exp)->integer );
 		return OK;
 	}
 	else if (is_tagged(exp, IF))  {
  		object_t* predicate = eval(cadr(exp), env);
 		object_t* consequent = caddr(exp);
 		object_t* alternative = cadddr(exp);
 		if (!eq(predicate, FALSE)) 
 			exp = consequent;
 		else 
 			exp = alternative;
 		goto tail;
 	}
 	else if (is_tagged(exp, BEGIN)) {
 		object_t* args = cdr(exp);
 		for (; !null(cdr(args)); args = cdr(args))
 			eval(car(args), env);
 		exp = car(args);
 		goto tail;
  	}
 	else if (is_tagged(exp, LAMBDA)) 
 		return make_procedure(cadr(exp), cddr(exp), env);
 	else if (is_tagged(exp, COND)) 
 		return new_sym("COND");
 	else if (!atom(exp)) {
 		object_t* proc = eval(car(exp), env);
		object_t* args = evlis(cdr(exp), env);
		
			if (proc->type == PRIM) {
				// printf("primitive:");
				// print_object((cdr(exp)));
				// print(args);

				return proc->primitive(args);
			}
			if (is_tagged(proc, PROCEDURE)) {
				env = extend_env(procedure_params(proc), args, procedure_env(proc));
	 			//exp = cons(BEGIN, procedure_body(proc));
	 			exp = procedure_body(proc);
	 			while(!null(cdr(exp))){
	 				printf("loop one");
	 				print(eval(car(exp), env));

	 				exp = cdr(exp);
	 			}
	 			printf("no loop\n");
	 			exp = car(exp);
	 			print(env);
	 			goto tail;
	 		}
		return apply(proc, args);		
 	}
 	else {
 		error("Unknown eval!");
 	}
}

/*
(define factorial (lambda(n) (if (< n 1) 1 (* n (factorial (- n 1))))))
(define (fact n) (if (= n 0) 1 (* n (factorial (- n 1)))))
(define (fact n) (define (product min max) (if (= min n) max (product (+ 1 min) (* min max))))(product 1 n))
      
(define (fact-iter product counter max-count) (if (> counter max-count) product (fact-iter (* counter product) (+ counter 1) max-count)))
      */

int main(int argc, char** argv) {
	char* input;
	/* Initialize static keywords */
	LAMBDA 		= new_sym("lambda");
	QUOTE 		= new_sym("quote");
	OK 			= new_sym("ok");
	PROCEDURE 	= new_sym("procedure");
	//PROCEDURE->type = PROC;
	BEGIN 		= new_sym("begin");
	IF 			= new_sym("if");
	COND 		= new_sym("cond");
	SET 		= new_sym("set!");
	DEFINE 		= new_sym("define");
	/* Initialize initial environment */
	GLOBAL_ENV = extend_env(&nil, &nil, &nil);
	init_prim(GLOBAL_ENV);
	eval(scan("(define #t true)"), GLOBAL_ENV);
	eval(scan("(define #f false)"), GLOBAL_ENV);
	eval(scan("(define = eq)"), GLOBAL_ENV);
	eval(scan("(define factorial (lambda(n) (if (< n 1) 1 (* n (factorial (- n 1))))))"), GLOBAL_ENV);
	eval(scan("(define (sum-of-squares num-list) (define sos-helper (lambda (remaining sum-so-far) (if (null? remaining) sum-so-far (sos-helper (cdr remaining) (+ (* (car remaining) (car remaining)) sum-so-far))))) (sos-helper num-list 0))"), GLOBAL_ENV);
	//print(eval(scan("(factorial 5)"), GLOBAL_ENV));
	printf("LITH ITH LITHENING...\n");
	size_t sz;
	do {
		printf("user> ");
		//input = read(stdin);
		getline(&input, &sz, stdin);
		if (*input == '!')
			break;
		object_t* in = scan(input);
		//print(in);
		//free(input);
		printf("=> ");
		print(eval(in, GLOBAL_ENV));
	} while(input);
	return 0;
}