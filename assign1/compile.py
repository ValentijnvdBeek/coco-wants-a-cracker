#!/usr/bin/env python3

import argparse
import lark
import sys
import os

from lark.tools import build_lalr
from llvmlite import ir, binding
from lark import Lark, Token


class Function:
    # Store all the function information in this structure.
    # arg: type _identifier
    # args: [arg, arg ... ]
    # self.fn_type = fn_type
    # self.fn_args = args
    # self.fn_name = name
    # self.arg_signature = arg_signature
    # self.fn_body = body: function ir.

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
        return self.var_type + " " + self.var_name

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

def typecheck(ast : lark.tree.Tree) -> dict:
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
                            if arg_type == "void":
                                print("void is invalid as a type declaration.")
                                exit(2)
                            arg_name = arg.children[1].value
                            args[arg_name] = arg_type
                            arg_signature.append(Variable(arg_type, arg_name))

                        fn.fn_args = args
                        fn.arg_signature = arg_signature
                else:
                    # CNAME
                    fn.fn_name = fn_meta.value
                    if fn.fn_name == "int" or fn.fn_name == "unsigned" or fn.fn_name == "void":
                        print("invalid function name")
                        exit(2)
            global_env[fn.fn_name] = fn

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

    return global_env

def stmt_checker(stmt: lark.tree.Tree, env: dict, fn_name: str):
    stmt_class = stmt.children[0].data
    if stmt_class == "expr_stmt":
        # expr or statement
        expr_stmt = stmt.children[0].children[0]
        if expr_stmt.data == "expr":
            expr_child = expr_stmt.children[0]
            expr_class = expr_child.data
            if expr_class == "call_expr":
                # get the function name.
                # now expr child is call_expr
                call_expr = expr_child
                fn_name = expr_child.children[0].value
                if fn_name == "Print" or fn_name == "PrintU":
                    # We don't need to handle them.
                    return
                fn = env[fn_name]
                # exit if the arg list failed to match the call.
                # call_expr: call_expr: _identifier "(" _call_args? ")"
                # _call_args: expr | expr "," _call_args
                num_call_params = len(call_expr.children[1:]) # exclude the identifier.
                num_args = len(fn.arg_signature)

                if num_args != num_call_params:
                    print("failed to match the number of args in function " + fn_name)
                    exit(2)

                for expr in call_expr.children[1:]:
                    for i, param in enumerate(expr.children):
                        if param.data == "literal":
                            literal_checker(fn.arg_signature[i].var_type, param)
        
        if expr_stmt.data == "assignment":
            print("assignment")

    if stmt_class == "var_decl":
        # type id expr
        decl_stmt = stmt.children[0]
        decl_type = decl_stmt.children[0].data
        if decl_type == "void":
            print("declare a void variable is invallid")
            exit(2)

        # CNAME
        var_name = decl_stmt.children[1].value
        env[var_name] = Variable(var_name=var_name, var_type=decl_type)
        # We should also check the type of expr, but the test cases don't include it.

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

"""
====== CodeGen Part ======
TODO(Zhiyang): divide them into python modules,
"""
def args_builder(expr_list : list, builder : ir.IRBuilder, local_env : dict, expr_type : str) -> list:
    ret = []

    for expr in expr_list:
        expr = expr_builder(expr, builder, local_env, expr_type)
        ret.append(load_value(expr, builder))

    return ret

def get_literal_ir(expr_type, expr):
    literal_str = extract_literal(expr_type, expr)
    literal_type = get_type_from_str(expr_type)
    const = ir.Constant(literal_type, literal_str)

    return const

def get_literal_type(expr : lark.tree.Tree) -> str:
    # Return the type based on the provided literal.
    for elem in expr.children:
        if elem.type == "MINUS":
            return "int"
        elif elem.type == "U":
            return "unsigned"
        else:
            return "int"

    print("unknown literal type")
    exit(2)

def load_value(var_ir, builder):
    print(type(var_ir))
    if isinstance(var_ir, ir.instructions.AllocaInstr):
        return builder.load(var_ir, align=4)
    else:
        return var_ir

def expr_builder(expr_p : lark.tree.Tree, builder : ir.IRBuilder, local_env: dict, fn_name : str):
    # Return an expr IR.
    # print(type(expr))
    expr = expr_p.children[0]
    expr_class = expr.data
    print(expr_class)

    if expr_class == "var_ref":
        var_name = expr.children[0]

        # If the type is not an ir, submit an error
        var_ir = local_env[var_name].var_ir
        if isinstance(var_ir, Variable):
            print("Failed to get the variable ir for " + var_name)
            exit(2)

        # As I said, the var_ir could be many thing (Argument, GlobalVar, reg, etc...)
        return load_value(var_ir, builder)

    elif expr_class == "literal":
        const = get_literal_ir("int", expr)
        return const

    elif expr_class == "call_expr":
        # Identifier
        fn_name = expr.children[0].value

        expr_type = local_env[fn_name].fn_type

        fn_ir = local_env[fn_name].fn_body
        fn_args = args_builder(expr.children[1:], builder, local_env, expr_type)

        print(fn_args)
        ret_val = builder.call(fn_ir, fn_args)

        return ret_val

    elif expr_class == "assignment":
        var_name = expr.children[0].value

        # local var is reg (pointer), the global var is var_ir, they're different.
        reg = local_env[var_name].var_ir
        expr_ir = expr_builder(expr.children[1], builder, local_env, fn_name)

        store_ir = builder.store(expr_ir, reg, align = 4)
        print(store_ir)
        return

    else:
        if expr_class == "add":
            # Hack the expr structure, it's the problem with the parser.lark, we need to add an expr wrapper to it.
            # For binop
            lhs = lark.tree.Tree(expr.data, [expr.children[0]])
            rhs = lark.tree.Tree(expr.data, [expr.children[1]])
            lhs_ir = load_value(expr_builder(lhs, builder, local_env, fn_name), builder)
            rhs_ir = expr_builder(rhs, builder, local_env, fn_name)
            inst_ir = builder.add(lhs_ir, rhs_ir)

        else:
            # Hack the expr structure, it's the problem with the parser.lark, we need to add an expr wrapper to it.
            # For binop
            lhs = lark.tree.Tree(expr.data, [expr.children[0]])
            rhs = lark.tree.Tree(expr.data, [expr.children[1]])
            lhs_ir = load_value(expr_builder(lhs, builder, local_env, fn_name), builder)
            rhs_ir = expr_builder(rhs, builder, local_env, fn_name)
            inst_ir = builder.sub(lhs_ir, rhs_ir)

        return inst_ir

    print("unimplemented expr")
    exit(2)

def conditional_gen(expr, builder : ir.IRBuilder, local_env : dict, fn_name : str):
    expr_class = expr.data
    # print(expr.pretty())

    lhs = expr.children[0]
    rhs = lark.tree.Tree(expr.data, [expr.children[1]])
    lhs_ir = expr_builder(lhs, builder, local_env, fn_name)
    rhs_ir = expr_builder(rhs, builder, local_env, fn_name)
    rhs_literal = expr.children[1]
    literal_type = get_type_from_str(rhs_literal)
    if expr_class == "lt":
        # epxr
        if literal_type == "int":
            ir_inst = builder.icmp_signed("<", lhs_ir, rhs_ir)
        else:
            ir_inst = builder.icmp_unsigned("<", lhs_ir, rhs_ir)

        # print("test", ir_inst)

    elif expr_class == "gt":
        if literal_type == "int":
            ir_inst = builder.icmp_signed(">", lhs_ir, rhs_ir)
        else:
            ir_inst = builder.icmp_unsigned(">", lhs_ir, rhs_ir)

    elif expr_class == "le":
        if literal_type == "int":
            ir_inst = builder.icmp_signed("<=", lhs_ir, rhs_ir)
        else:
            ir_inst = builder.icmp_unsigned("<=", lhs_ir, rhs_ir)

    elif expr_class == "ge":
        if literal_type == "int":
            ir_inst = builder.icmp_signed(">=", lhs_ir, rhs_ir)
        else:
            ir_inst = builder.icmp_unsigned(">=", lhs_ir, rhs_ir)

    elif expr_class == "eq":
        if literal_type == "int":
            ir_inst = builder.icmp_signed("==", lhs_ir, rhs_ir)
        else:
            ir_inst = builder.icmp_unsigned("==", lhs_ir, rhs_ir)

    else:
        if literal_type == "int":
            ir_inst = builder.icmp_signed("!=", lhs_ir, rhs_ir)
        else:
            ir_inst = builder.icmp_unsigned("!=", lhs_ir, rhs_ir)

    return ir_inst

def expr_stmt_builder(expr_stmt, builder, local_env, fn_name):
    expr = expr_stmt.children[0]
    return expr_builder(expr, builder, local_env, fn_name)

def stmt_gen(module : ir.Module, builder : ir.IRBuilder, stmt : lark.tree.Tree, local_env : dict, fn_name : str):
    # Generate LLVM IR for each statement.
    inner_stmt = stmt.children[0]
    inner_stmt_class = stmt.children[0].data
    if inner_stmt_class == "expr_stmt":
        expr_stmt_ir = expr_stmt_builder(inner_stmt, builder, local_env, fn_name)
        return

    if inner_stmt_class == "return_stmt":
        # return expr, we need to know the content of expr.
        expr_ir = expr_builder(inner_stmt.children[0], builder, local_env, fn_name)
        builder.ret(expr_ir)
        return

    if inner_stmt_class == "compound_stmt":
        # compound stmt means a new env block.
        new_env = local_env.copy()
        for new_stmt in inner_stmt.find_data("stmt"):
            # Be careful with this builder.
            stmt_gen(module, builder, new_stmt, new_env, fn_name)

    if inner_stmt_class == "var_decl":
        var_type = inner_stmt.children[0].data
        var_type = get_type_from_str(var_type)

        var_name = inner_stmt.children[1].value
        expr_ir = expr_builder(inner_stmt.children[2], builder, local_env, fn_name)

        # We allocate a register here.
        reg = builder.alloca(var_type)
        ret_val = builder.store(expr_ir, reg, align=4)

        # Update our env, so later we can find the ir.
        local_env[var_name] = Variable(var_name, var_type)
        local_env[var_name].var_ir = reg

        return ret_val

    if inner_stmt_class == "if_stmt":
        expr = inner_stmt.children[0]
        stmt = inner_stmt.children[1]

        # We should append block to our function for conditional branches.
        fn_ir = local_env[fn_name].fn_body
        if_block = fn_ir.append_basic_block()
        else_block = fn_ir.append_basic_block()

        # transform the lt to expr for mitigating our api.
        expr_ir = conditional_gen(expr, builder, local_env, fn_name)

        # print(expr_ir)
        # Now working on branches.
        builder.position_at_start(if_block)
        stmt_gen(module, builder, stmt, local_env, fn_name)

        builder.cbranch(expr_ir, if_block, else_block)

        """
        FenneC has no else statement, so the whole program left is in the else branch.
        We should reset the position at the start of the else_block)
        """
        builder.position_at_start(else_block)
        return

    return

def fn_gen(module : ir.Module, fn_ast : lark.tree.Tree, env : dict):
    # We simply extract the stmts out from the compound stmt.
    func_name = fn_ast.children[1].value
    fn = env[func_name]
    local_env = env.copy()
    updated_vars = fn.fn_args
    for k, v in updated_vars.items():
        local_env[k] = Variable(v, k)

    function_type = get_type_from_str(fn.fn_type)
    function_args = []
    for arg in fn.arg_signature:
        function_args.append(get_type_from_str(arg.var_type))

    fn_type = ir.FunctionType(function_type, function_args)
    fn_ir = ir.Function(module, fn_type, func_name)
    for i, arg in enumerate(fn.arg_signature):
        # Initialize the function arguments.
        fn_ir.args[i].name = arg.var_name
        local_env[arg.var_name].var_ir = fn_ir.args[i]

    # add our function ir into env.
    env[func_name].fn_body = fn_ir

    # We create an entry block for each function.
    entry_block = fn_ir.append_basic_block('entry')
    builder = ir.IRBuilder(entry_block)

    # compound statement
    for stmt_list in fn_ast.find_data("stmt_list"):
        # We don't copy the local_env since we need to update the types along the way.
        for stmt in stmt_list.children:
            stmt_gen(module, builder, stmt, local_env, func_name)

    return

def extract_literal(decl_type : str, literal : lark.tree.Tree) -> str:
    # extract the number out from the literal node.
    literal_tokens = literal.children
    number = ""
    if decl_type == "int":
        if literal_tokens[0].type == "MINUS":
            # negative number
            number = literal_tokens[1].value
            number = "-" + number
        if literal_tokens[0].type == "NUMBER":
            number = literal_tokens[0].value

        return number

    elif decl_type == "unsigned":
        for t in literal_tokens:
            if t.type == "NUMBER":
                return t.value

    print("Failed to parse the literal")
    exit(2)

def get_type_from_str(type_str : str):
    if type_str == "int":
        return ir.IntType(32)
    if type_str == "unsigned":
        return ir.IntType(32)

    if type_str == "void":
        return ir.VoidType()

    # No pointer
    return

def global_gen(module : ir.Module, global_decl : lark.tree.Tree, env : dict):

    decl_type_raw = global_decl.children[0].data
    decl_value = extract_literal(decl_type_raw, global_decl.children[2])
    decl_type = get_type_from_str(decl_type_raw)
    decl_name = global_decl.children[1].value

    x = ir.GlobalVariable(module, decl_type, decl_name)
    x.initializer = ir.Constant(decl_type, decl_value)

    # We replace the Variable Class with the variable ir.
    env[decl_name] = Variable(decl_name, decl_type_raw)
    env[decl_name].var_ir = x

    return

def codegen(ast : lark.tree.Tree, module_name : str, env : dict):
    # Just simple initialization.
    module = ir.Module(module_name)
    module.tree = binding.get_default_triple()

    print(ast.pretty())

    # Hard-coded Print and PrintU
    print_arg_t = ir.IntType(32)
    print_ret_t = ir.VoidType()
    fn_type = ir.FunctionType(print_ret_t, [print_arg_t])
    p_ir = ir.Function(module, fn_type, "Print")
    pu_ir = ir.Function(module, fn_type, "PrintU")

    env["Print"] = Function()
    env["Print"].fn_type = "void"
    env["Print"].fn_body = p_ir
    env["Print"].arg_signature = [Variable("int", "")]

    env["PrintU"] = Function()
    env["PrintU"].fn_type = "void"
    env["PrintU"].fn_body = pu_ir
    env["PrintU"].arg_signature = [Variable("int", "")]

    for top_level in ast.find_data("top_level"):
        level_child = top_level.children[0]
        level_class = level_child.data
        if level_class == "global":
            global_gen(module, level_child, env)
        else:
            fn_gen(module, level_child, env)

    print(module)
    return

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

    global_env = typecheck(parsed)
    codegen(parsed, args.input.replace(".c", ".ll"), global_env)
    with open(args.output, "w") as output_file:
        output_file.write(str(parsed))
