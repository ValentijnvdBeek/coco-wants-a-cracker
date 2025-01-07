#!/usr/bin/env python3

import argparse
import lark
import sys
import os

from llvmlite import ir, binding
from lark import Lark

class Function:
    # Store all the function information in this structure.
    # arg: type _identifier
    # args: [arg, arg ... ]
    def __init__(self, fn_type : str, args : dict, name : str, body : lark.tree.Tree, arg_signature: list):
        self.fn_type = fn_type
        self.fn_args = args
        self.fn_name = name
        self.arg_signature = arg_signature
        self.fn_body = body

    def __init__(self):
        self.fn_args = {}
        self.arg_signature = []
        return

    def __str__(self):
        # Just for debugging.
        return self.fn_type + " " + self.fn_name

class Variable:
    def __init__(self, var_type : str, var_name : str):
        self.var_type = var_type
        # self.var_val = var_val
        self.var_name = var_name

    def __str__(self):
        # Another debuging, yeah.
        return self.var_type + " " + self.var_name + " = " + self.var_val

def parse(input_program):
    # Parse grammar file.
    with open(os.path.join(assign1_dir, "fennec.lark"), "r") as f:
        grammar_str = f.read()  # type: str
    l = lark.Lark(grammar_str)
    return l.parse(input_program)

def literal_checker(decl_type: str, literal: lark.tree.Tree):
    literal_tokens = literal.children
    if decl_type == "int":
        # Check if it contains "U"
        for t in literal_tokens:
            if t.type == "U":
                print("literal value is unsigned but declared as int.")
                exit(2)
        if literal_tokens[0].type == "MINUS":
            # negative number
            number = literal_tokens[1].value
            num = int("-" + number)
        if literal_tokens[0].type == "NUMBER":
            number = literal_tokens[0].value
            num = int(number)

        if not (-2147483648 <= num <= 2147483647):
            # Int boundary check
            exit(2)
    elif decl_type == "unsigned":
        if literal_tokens[0].type == "MINUS":
            # Negative number is invalid
            print("literal value is int but declared as unsigned.")
            exit(2)
        for t in literal_tokens:
            if t.type == "NUMBER":
                num = int(t.value)
                if not (0 <= num <= 4294967295):
                    exit(2)
    else:
        exit(2)

def typecheck(ast):
    global_env = {}

    for top_level in ast.find_data("top_level"):
        child = top_level.children[0]
        child_token = child.data
        # Only global or function
        if child_token == "global":
            # Now we know it's a global: type _identifier "=" literal ";"
            var_type = child.children[0].data
            var_name = child.children[1].value
            literal = child.children[2]
            # Check if the literal matches the declared type.
            literal_checker(var_type, literal)
            var = Variable(var_type, var_name)
            global_env[var_name] = var
        else:
            fn = Function()
            for fn_meta in child.children:
                if isinstance(fn_meta, lark.tree.Tree):
                    if fn_meta.data == "int":
                        fn.fn_type = "int"
                    elif fn_meta.data == "compound_stmt":
                        fn.fn_body = fn_meta
                    else:
                        args = {}
                        arg_signature = []
                        for arg in fn_meta.find_data("arg"):
                            arg_type = arg.children[0].data
                            arg_name = arg.children[1].value
                            args[arg_name] = arg_type
                            arg_signature.append(arg_type)

                        fn.fn_args = args
                        fn.arg_signature = arg_signature
                else:
                    # CNAME
                    fn.fn_name = fn_meta.value
            global_env[fn.fn_name] = fn

    print(global_env)
    # After we getting the global environment, we can work on the local env for each funtion.
    for function in ast.find_data("function"):
        # We simply extract the stmts out from the compound stmt.
        func_name = function.children[1].value
        # We need to update the variable type in terms of the variable shadowing.
        fn = global_env[func_name]
        local_env = global_env.copy()
        updated_vars = fn.fn_args
        for k, v in updated_vars.items():
            local_env[k] = Variable(v, k) 
            # print("update")
            # print(k)
            # print(v)
            # print(global_env)
            # print(local_env)
        for fun_body in function.find_data("stmt"):
            # We don't copy the local_env since we need to update the types along the way.
            stmt_checker(fun_body, local_env, func_name)

def stmt_checker(stmt: lark.tree.Tree, env: dict, fn_name: str):
    stmt_class = stmt.children[0].data
    if stmt_class == "expr_stmt":
        # expr or statement
        expr_stmt = stmt.children[0].children[0]
        if expr_stmt.data == "expr":
            return
            expr_child = expr_stmt.children[0]
            expr_class = expr_child.data
            if expr_class == "call_expr":
                # get the function name.
                # now expr child is call_expr
                call_expr = expr_child
                fn_name = expr_child.children[0]
                fn = env[fn_name]
                # exit if the arg list failed to match the call.
                print("test2", call_expr.children[2:])
                num_call_params = len(call_expr.children[1:])
                num_args = len(fn.arg_signature)

                if num_args != num_call_params:
                    print("failed to match the number of args in function " + fn_name)
                    exit(2)

                for param in call_expr.children:
                    print(param)
        
        if expr_stmt.data == "assignment":
            print("assignment")

    if stmt_class == "return_stmt":
        # Since there's no type conversions allowed, 
        # we can just check if any type of the elements match the return type of the function
        fn = env[fn_name]
        return_type = fn.fn_type

        literals = stmt.find_data("literal")
        for literal in literals:
            literal_checker(return_type, literal)

        vars = stmt.find_data("var_ref")
        for var_ref in vars:
            var_name = var_ref.children[0].value
            var_type = env[var_name].var_type
            if var_type != return_type:
                print("variable " + var_name  + " type " + var_type + " doesn't match the return type " + return_type + " in function " + fn_name)
                exit(2)
    return

def expr_checker(expr: lark.tree.Tree) -> str:
    # It checkes the correctness of the expr and return it's type.
    return

    # Type check each function seperately.
    # For top-level, we have both function and variables defined in lark.
    # print(ast.pretty())
    # for function in ast.find_data("function"):
    #     # We assume that our parsed sets always has the form of:
    #     #   function (type, name, body)
    #     print("====", function.children[1].value)
    #     func_type = function.children[0].data
    #     func_name = function.children[1].value
    #     func_body = function.children[2].children[0]

    #     functype_map[func_name] = func_type
    #     print(func_type)
    #     print(func_name)
    #     print(func_body)


# Main logic
if __name__ == "__main__":
    assign1_dir = os.path.dirname(os.path.realpath(__file__))

    # Parse command line arguments.
    parser = argparse.ArgumentParser(description='Fennec frontend')
    parser.add_argument('input', help='Input and/or output file')
    parser.add_argument('output', help='Input and/or output file')
    parser.add_argument('--ast', action="store_true", help='Print the AST')
    parser.add_argument('--ir', action="store_true", help='Print the IR')
    args = parser.parse_args()

    # Read input.
    input_program = None
    with open(args.input, "r") as input_file:
        input_program = input_file.read()

    parsed = None
    try:
        parsed = parse(input_program)
    except lark.exceptions.LarkError:
        from sys import exit
        print("Cannot parse input file '%{input_file}'")
        exit(2)

    # if args.ast:
        # print(list(parsed.find_data("top_level")))

    typecheck(parsed)
    with open(args.output, "w") as output_file:
        output_file.write(str(parsed))
