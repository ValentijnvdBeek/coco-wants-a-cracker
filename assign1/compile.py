#!/usr/bin/env python3

import argparse
import lark
import sys
import os

from llvmlite import ir, binding
from lark import Lark


def parse(input_program):
    # Parse grammar file.
    with open(os.path.join(assign1_dir, "fennec.lark"), "r") as f:
        grammar_str = f.read()  # type: str
    l = lark.Lark(grammar_str)
    return l.parse(input_program)


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

    if args.ast:
        print(parsed.pretty())
    with open(args.output, "w") as output_file:
        output_file.write(str(parsed))
