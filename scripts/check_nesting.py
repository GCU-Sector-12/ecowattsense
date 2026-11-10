#!/usr/bin/env python3
"""Reports control statements nested inside other control statements (the NEVER NESTED rule).

The rule is KB EWS-A-25 and AGENTS.md: no if, for, while, try or switch inside another one.
Who uses it: the CI job "nesting" in .github/workflows/ci.yml runs it on the Python and
JavaScript files that a pull request adds or changes. A developer runs it by hand before a push.

Python files are measured with ast (an elif chain counts as one level).
JavaScript, JSX, TypeScript, Swift and Kotlin files use an indentation heuristic
on if/guard/for/while/switch/repeat/do/try/catch lines, so a line that only
continues an expression is never counted.
A function with more than LIMIT levels of nested control statements is reported.

Usage: python scripts/check_nesting.py [--limit N] [PATH ...]
With no paths, the tracked files of the current directory (git ls-files) are checked,
except dashboard/legacy, dashboard/public/legacy-demo and node_modules.
Exit code 1 when anything is found, 0 otherwise. Needs Python 3.8 or newer, no packages.
"""
import argparse
import ast
import re
import subprocess
import sys
from itertools import accumulate, chain
from pathlib import Path
from typing import List, Tuple

CONTROL = (ast.If, ast.For, ast.While, ast.Try, ast.With, ast.AsyncFor, ast.AsyncWith)
JS_CONTROL = re.compile(r'^\s*(?:\}\s*)?(if|guard|for|while|switch|when|repeat|do|try|catch)\b')
FUNCTIONS = (ast.FunctionDef, ast.AsyncFunctionDef)
SUFFIXES = {'.py', '.js', '.jsx', '.ts', '.tsx', '.swift', '.kt'}
SKIPPED_PARTS = {'.claude-plugin', 'node_modules', 'legacy', 'legacy-demo'}
LIMIT = 1


def is_elif(parent: ast.AST, child: ast.AST) -> bool:
    """Says whether child is the elif branch of parent, which does not add a level."""
    return isinstance(parent, ast.If) and isinstance(child, ast.If) and parent.orelse == [child]


def step(parent: ast.AST, child: ast.AST, depth: int) -> int:
    """Gives the nesting depth of child: one more than depth when it is a control statement."""
    if not isinstance(child, CONTROL):
        return depth
    return depth if is_elif(parent, child) else depth + 1


def depth_of(node: ast.AST, depth: int = 0) -> int:
    """Gives the deepest nesting of control statements under node, starting at depth."""
    depths = [depth_of(child, step(node, child, depth)) for child in ast.iter_child_nodes(node)]
    return max([depth, *depths])


def python_findings(text: str, limit: int) -> List[str]:
    """Lists the Python functions whose nesting depth is above limit.

    Args:
        text: Source of one Python file.
        limit: Allowed depth; 1 means a control statement may not hold another one.

    Returns:
        One "line N, depth D (name)" string per function above the limit.

    Raises:
        SyntaxError: When the file does not parse.
    """
    tree = ast.parse(text)
    funcs = [n for n in ast.walk(tree) if isinstance(n, FUNCTIONS)]
    rows = [(depth_of(f), f.name, f.lineno) for f in funcs]
    return [f'line {line}, depth {depth} ({name})' for depth, name, line in rows if depth > limit]


def js_entries(text: str) -> List[Tuple[int, int]]:
    """Lists (line number, indent) of every line that starts a control statement."""
    lines = enumerate(text.splitlines(), 1)
    return [(number, len(line) - len(line.lstrip())) for number, line in lines if JS_CONTROL.match(line)]


def js_step(state: Tuple[List[int], int], entry: Tuple[int, int]) -> Tuple[List[int], int]:
    """Folds one control line into the indent stack and gives the new stack and its depth."""
    stack, _ = state
    kept = [indent for indent in stack if indent < entry[1]]
    return kept + [entry[1]], len(kept) + 1


def js_findings(text: str, limit: int) -> List[str]:
    """Lists the control lines nested deeper than limit, by indentation.

    Args:
        text: Source of one JavaScript, JSX, TypeScript, Swift or Kotlin file.
        limit: Allowed depth; 1 means a control statement may not hold another one.

    Returns:
        One "line N, depth D" string per control line above the limit.
    """
    entries = js_entries(text)
    states = list(accumulate(entries, js_step, initial=([], 0)))[1:]
    return [f'line {entry[0]}, depth {depth}' for entry, (_, depth) in zip(entries, states) if depth > limit]


def report(path: Path, limit: int) -> List[str]:
    """Reads one file and gives its findings prefixed with the path."""
    text = path.read_text(encoding='utf-8')
    finder = python_findings if path.suffix == '.py' else js_findings
    return [f'{path}: {item}' for item in finder(text, limit)]


def tracked() -> List[Path]:
    """Lists the files git tracks in the current directory."""
    out = subprocess.run(['git', 'ls-files'], capture_output=True, text=True, check=True).stdout
    return [Path(line) for line in out.splitlines()]


def wanted(path: Path) -> bool:
    """Says whether a tracked file has a checked suffix and is outside the skipped folders."""
    return path.suffix in SUFFIXES and not SKIPPED_PARTS & set(path.parts)


def parse_args(argv: List[str]) -> argparse.Namespace:
    """Parses the command line: optional paths and --limit."""
    parser = argparse.ArgumentParser(description='Report nested control statements.')
    parser.add_argument('paths', nargs='*', type=Path)
    parser.add_argument('--limit', type=int, default=LIMIT)
    return parser.parse_args(argv)


def main(argv: List[str]) -> int:
    """Runs the check on the given or tracked files, prints the findings, gives the exit code."""
    args = parse_args(argv)
    paths = args.paths or [path for path in tracked() if wanted(path)]
    findings = list(chain.from_iterable(report(path, args.limit) for path in paths))
    print('\n'.join(findings) or 'no nested control statements found')
    return 1 if findings else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
