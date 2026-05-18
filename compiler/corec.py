#!/usr/bin/env python3
import argparse
import io
import json
import os
import sys
import time
from dataclasses import dataclass, field
from typing import Any, Callable, Dict, List, Optional, Tuple


KEYWORDS = {
    'voice', 'keep', 'perhaps', 'otherwise', 'drown', 'bury', 'fade',
    'still', 'gone', 'void',
    'in', 'and', 'or', 'not',
    'echo', 'memory', 'pain', 'shadow',
}

TYPE_NAMES = {'echo', 'memory', 'pain', 'shadow', 'void'}


@dataclass
class Token:
    kind: str
    value: Any
    line: int
    col: int


class LexError(Exception):
    pass


class ParseError(Exception):
    pass


class SilentFault(Exception):
    pass


class FadeSignal(Exception):
    def __init__(self, value: Any = None):
        super().__init__('fade')
        self.value = value


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1
        self.tokens: List[Token] = []

    def at(self, offset: int = 0) -> str:
        i = self.pos + offset
        return self.source[i] if i < len(self.source) else ''

    def step(self) -> str:
        ch = self.source[self.pos]
        self.pos += 1
        if ch == '\n':
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch

    def tokenize(self) -> List[Token]:
        while self.pos < len(self.source):
            ch = self.at()
            if ch in ' \t\r':
                self.step()
                continue
            if ch == '\n':
                line, col = self.line, self.col
                self.step()
                if self.tokens and self.tokens[-1].kind != 'NEWLINE':
                    self.tokens.append(Token('NEWLINE', '\\n', line, col))
                continue
            if ch == '~' and self.at(1) == '~':
                while self.pos < len(self.source) and self.at() != '\n':
                    self.step()
                continue
            line, col = self.line, self.col
            if ch.isdigit():
                self._number(line, col)
                continue
            if ch == '"':
                self._string(line, col)
                continue
            if ch.isalpha() or ch == '_':
                self._ident(line, col)
                continue
            self._operator(line, col)
        if not self.tokens or self.tokens[-1].kind != 'NEWLINE':
            self.tokens.append(Token('NEWLINE', '\\n', self.line, self.col))
        self.tokens.append(Token('EOF', '', self.line, self.col))
        return self.tokens

    def _number(self, line: int, col: int) -> None:
        start = self.pos
        is_float = False
        while self.pos < len(self.source):
            c = self.at()
            if c == '.' and self.at(1) == '.':
                break
            if c == '.':
                if is_float:
                    break
                is_float = True
                self.step()
                continue
            if c.isdigit():
                self.step()
                continue
            break
        text = self.source[start:self.pos]
        if is_float:
            self.tokens.append(Token('NUMBER', float(text), line, col))
        else:
            self.tokens.append(Token('NUMBER', int(text), line, col))

    def _string(self, line: int, col: int) -> None:
        self.step()
        parts: List[Any] = ['']
        while self.pos < len(self.source) and self.at() != '"':
            c = self.at()
            if c == '\\' and self.pos + 1 < len(self.source):
                self.step()
                esc = self.step()
                mapping = {'n': '\n', 't': '\t', 'r': '\r', '"': '"', '\\': '\\', '{': '{', '}': '}'}
                parts[-1] += mapping.get(esc, esc)
                continue
            if c == '{':
                self.step()
                name = ''
                while self.pos < len(self.source) and self.at() != '}':
                    name += self.step()
                if self.pos < len(self.source):
                    self.step()
                parts.append(('var', name.strip()))
                parts.append('')
                continue
            parts[-1] += self.step()
        if self.pos >= len(self.source):
            raise LexError(f"unfinished string at line {line}")
        self.step()
        self.tokens.append(Token('STRING', parts, line, col))

    def _ident(self, line: int, col: int) -> None:
        start = self.pos
        while self.pos < len(self.source) and (self.at().isalnum() or self.at() == '_'):
            self.step()
        text = self.source[start:self.pos]
        if text in KEYWORDS:
            self.tokens.append(Token('KEYWORD', text, line, col))
        else:
            self.tokens.append(Token('IDENT', text, line, col))

    def _operator(self, line: int, col: int) -> None:
        ch = self.step()
        two = ch + self.at()
        if two in ('==', '!=', '<=', '>=', '->', '..'):
            self.step()
            kind = {
                '==': 'OP', '!=': 'OP', '<=': 'OP', '>=': 'OP',
                '->': 'ARROW', '..': 'RANGE',
            }[two]
            self.tokens.append(Token(kind, two, line, col))
            return
        single_map = {
            '+': 'OP', '-': 'OP', '*': 'OP', '/': 'OP', '%': 'OP',
            '<': 'OP', '>': 'OP', '=': 'ASSIGN',
            '(': 'LPAREN', ')': 'RPAREN',
            '{': 'LBRACE', '}': 'RBRACE',
            '[': 'LBRACK', ']': 'RBRACK',
            ',': 'COMMA', ':': 'COLON',
        }
        if ch in single_map:
            self.tokens.append(Token(single_map[ch], ch, line, col))
            return
        raise LexError(f"strange mark '{ch}' at line {line}:{col}")


@dataclass
class Node:
    line: int = 0


@dataclass
class Program(Node):
    body: List[Node] = field(default_factory=list)


@dataclass
class FuncDef(Node):
    name: str = ''
    params: List[Tuple[str, Optional[str]]] = field(default_factory=list)
    return_type: Optional[str] = None
    body: List[Node] = field(default_factory=list)


@dataclass
class VarDecl(Node):
    name: str = ''
    type_hint: Optional[str] = None
    value: Optional[Node] = None


@dataclass
class Assign(Node):
    target: Optional[Node] = None
    value: Optional[Node] = None


@dataclass
class IfStmt(Node):
    cond: Optional[Node] = None
    then_body: List[Node] = field(default_factory=list)
    else_body: List[Node] = field(default_factory=list)


@dataclass
class WhileStmt(Node):
    cond: Optional[Node] = None
    body: List[Node] = field(default_factory=list)


@dataclass
class ForStmt(Node):
    var: str = ''
    iterable: Optional[Node] = None
    body: List[Node] = field(default_factory=list)


@dataclass
class FadeStmt(Node):
    value: Optional[Node] = None


@dataclass
class ExprStmt(Node):
    value: Optional[Node] = None


@dataclass
class Binary(Node):
    op: str = ''
    left: Optional[Node] = None
    right: Optional[Node] = None


@dataclass
class Unary(Node):
    op: str = ''
    operand: Optional[Node] = None


@dataclass
class Call(Node):
    callee: Optional[Node] = None
    args: List[Node] = field(default_factory=list)


@dataclass
class Index(Node):
    target: Optional[Node] = None
    index: Optional[Node] = None


@dataclass
class Name(Node):
    text: str = ''


@dataclass
class NumberLit(Node):
    value: Any = 0


@dataclass
class StringLit(Node):
    parts: List[Any] = field(default_factory=list)


@dataclass
class BoolLit(Node):
    value: bool = False


@dataclass
class VoidLit(Node):
    pass


@dataclass
class ArrayLit(Node):
    items: List[Node] = field(default_factory=list)


@dataclass
class RangeLit(Node):
    start: Optional[Node] = None
    end: Optional[Node] = None


class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    def at(self) -> Token:
        return self.tokens[self.pos]

    def peek(self, offset: int = 0) -> Token:
        i = self.pos + offset
        return self.tokens[i] if i < len(self.tokens) else self.tokens[-1]

    def eat(self) -> Token:
        t = self.tokens[self.pos]
        self.pos += 1
        return t

    def check(self, kind: str, value: Any = None) -> bool:
        t = self.at()
        if t.kind != kind:
            return False
        if value is not None and t.value != value:
            return False
        return True

    def accept(self, kind: str, value: Any = None) -> Optional[Token]:
        if self.check(kind, value):
            return self.eat()
        return None

    def expect(self, kind: str, value: Any = None) -> Token:
        t = self.at()
        if not self.check(kind, value):
            want = value if value is not None else kind
            raise ParseError(f"line {t.line}: wanted {want}, found '{t.value}'")
        return self.eat()

    def skip_breaks(self) -> None:
        while self.check('NEWLINE'):
            self.eat()

    def parse(self) -> Program:
        prog = Program()
        self.skip_breaks()
        while not self.check('EOF'):
            prog.body.append(self.top_level())
            self.skip_breaks()
        return prog

    def top_level(self) -> Node:
        if self.check('KEYWORD', 'voice'):
            return self.func_def()
        return self.statement()

    def func_def(self) -> FuncDef:
        line = self.at().line
        self.expect('KEYWORD', 'voice')
        name = self.expect('IDENT').value
        self.expect('LPAREN')
        params: List[Tuple[str, Optional[str]]] = []
        while not self.check('RPAREN'):
            pname = self.expect('IDENT').value
            ptype: Optional[str] = None
            if self.accept('COLON'):
                ptype = self.parse_type()
            params.append((pname, ptype))
            if not self.accept('COMMA'):
                break
        self.expect('RPAREN')
        ret: Optional[str] = None
        if self.accept('ARROW'):
            ret = self.parse_type()
        body = self.block()
        return FuncDef(line=line, name=name, params=params, return_type=ret, body=body)

    def parse_type(self) -> str:
        t = self.at()
        if t.kind == 'KEYWORD' and t.value in TYPE_NAMES:
            self.eat()
            return t.value
        if t.kind == 'IDENT':
            self.eat()
            return t.value
        raise ParseError(f"line {t.line}: expected a type, found '{t.value}'")

    def block(self) -> List[Node]:
        self.skip_breaks()
        self.expect('LBRACE')
        self.skip_breaks()
        stmts: List[Node] = []
        while not self.check('RBRACE') and not self.check('EOF'):
            stmts.append(self.statement())
            self.skip_breaks()
        self.expect('RBRACE')
        return stmts

    def statement(self) -> Node:
        t = self.at()
        if t.kind == 'KEYWORD':
            if t.value == 'keep':
                return self.var_decl()
            if t.value == 'perhaps':
                return self.if_stmt()
            if t.value == 'drown':
                return self.while_stmt()
            if t.value == 'bury':
                return self.for_stmt()
            if t.value == 'fade':
                return self.fade_stmt()
            if t.value == 'voice':
                return self.func_def()
        return self.expr_or_assign()

    def var_decl(self) -> VarDecl:
        line = self.at().line
        self.expect('KEYWORD', 'keep')
        name = self.expect('IDENT').value
        type_hint: Optional[str] = None
        if self.accept('COLON'):
            type_hint = self.parse_type()
        value: Optional[Node] = None
        if self.accept('ASSIGN'):
            value = self.expression()
        return VarDecl(line=line, name=name, type_hint=type_hint, value=value)

    def if_stmt(self) -> IfStmt:
        line = self.at().line
        self.expect('KEYWORD', 'perhaps')
        cond = self.expression()
        then_body = self.block()
        else_body: List[Node] = []
        self.skip_breaks()
        if self.accept('KEYWORD', 'otherwise'):
            if self.check('KEYWORD', 'perhaps'):
                else_body = [self.if_stmt()]
            else:
                else_body = self.block()
        return IfStmt(line=line, cond=cond, then_body=then_body, else_body=else_body)

    def while_stmt(self) -> WhileStmt:
        line = self.at().line
        self.expect('KEYWORD', 'drown')
        cond = self.expression()
        body = self.block()
        return WhileStmt(line=line, cond=cond, body=body)

    def for_stmt(self) -> ForStmt:
        line = self.at().line
        self.expect('KEYWORD', 'bury')
        var = self.expect('IDENT').value
        self.expect('KEYWORD', 'in')
        iterable = self.expression()
        body = self.block()
        return ForStmt(line=line, var=var, iterable=iterable, body=body)

    def fade_stmt(self) -> FadeStmt:
        line = self.at().line
        self.expect('KEYWORD', 'fade')
        value: Optional[Node] = None
        if not self.check('NEWLINE') and not self.check('RBRACE') and not self.check('EOF'):
            value = self.expression()
        return FadeStmt(line=line, value=value)

    def expr_or_assign(self) -> Node:
        line = self.at().line
        target = self.expression()
        if self.accept('ASSIGN'):
            value = self.expression()
            return Assign(line=line, target=target, value=value)
        return ExprStmt(line=line, value=target)

    def expression(self) -> Node:
        return self.range_expr()

    def range_expr(self) -> Node:
        left = self.logic_or()
        if self.accept('RANGE'):
            right = self.logic_or()
            return RangeLit(line=left.line, start=left, end=right)
        return left

    def logic_or(self) -> Node:
        left = self.logic_and()
        while self.check('KEYWORD', 'or'):
            self.eat()
            right = self.logic_and()
            left = Binary(line=left.line, op='or', left=left, right=right)
        return left

    def logic_and(self) -> Node:
        left = self.logic_not()
        while self.check('KEYWORD', 'and'):
            self.eat()
            right = self.logic_not()
            left = Binary(line=left.line, op='and', left=left, right=right)
        return left

    def logic_not(self) -> Node:
        if self.check('KEYWORD', 'not'):
            line = self.at().line
            self.eat()
            operand = self.logic_not()
            return Unary(line=line, op='not', operand=operand)
        return self.comparison()

    def comparison(self) -> Node:
        left = self.addition()
        while self.at().kind == 'OP' and self.at().value in ('==', '!=', '<', '>', '<=', '>='):
            op = self.eat().value
            right = self.addition()
            left = Binary(line=left.line, op=op, left=left, right=right)
        return left

    def addition(self) -> Node:
        left = self.multiplication()
        while self.at().kind == 'OP' and self.at().value in ('+', '-'):
            op = self.eat().value
            right = self.multiplication()
            left = Binary(line=left.line, op=op, left=left, right=right)
        return left

    def multiplication(self) -> Node:
        left = self.unary()
        while self.at().kind == 'OP' and self.at().value in ('*', '/', '%'):
            op = self.eat().value
            right = self.unary()
            left = Binary(line=left.line, op=op, left=left, right=right)
        return left

    def unary(self) -> Node:
        if self.at().kind == 'OP' and self.at().value == '-':
            line = self.at().line
            self.eat()
            operand = self.unary()
            return Unary(line=line, op='-', operand=operand)
        return self.postfix()

    def postfix(self) -> Node:
        node = self.primary()
        while True:
            if self.accept('LPAREN'):
                args: List[Node] = []
                while not self.check('RPAREN'):
                    args.append(self.expression())
                    if not self.accept('COMMA'):
                        break
                self.expect('RPAREN')
                node = Call(line=node.line, callee=node, args=args)
                continue
            if self.accept('LBRACK'):
                idx = self.expression()
                self.expect('RBRACK')
                node = Index(line=node.line, target=node, index=idx)
                continue
            break
        return node

    def primary(self) -> Node:
        t = self.at()
        if t.kind == 'NUMBER':
            self.eat()
            return NumberLit(line=t.line, value=t.value)
        if t.kind == 'STRING':
            self.eat()
            return StringLit(line=t.line, parts=t.value)
        if t.kind == 'KEYWORD' and t.value == 'still':
            self.eat()
            return BoolLit(line=t.line, value=True)
        if t.kind == 'KEYWORD' and t.value == 'gone':
            self.eat()
            return BoolLit(line=t.line, value=False)
        if t.kind == 'KEYWORD' and t.value == 'void':
            self.eat()
            return VoidLit(line=t.line)
        if t.kind == 'IDENT':
            self.eat()
            return Name(line=t.line, text=t.value)
        if t.kind == 'LPAREN':
            self.eat()
            expr = self.expression()
            self.expect('RPAREN')
            return expr
        if t.kind == 'LBRACK':
            self.eat()
            items: List[Node] = []
            self.skip_breaks()
            while not self.check('RBRACK'):
                items.append(self.expression())
                self.skip_breaks()
                if not self.accept('COMMA'):
                    break
                self.skip_breaks()
            self.expect('RBRACK')
            return ArrayLit(line=t.line, items=items)
        raise ParseError(f"line {t.line}: unexpected '{t.value}'")


class Environment:
    def __init__(self, parent: Optional['Environment'] = None):
        self.values: Dict[str, Any] = {}
        self.parent = parent

    def get(self, name: str) -> Any:
        if name in self.values:
            return self.values[name]
        if self.parent is not None:
            return self.parent.get(name)
        raise SilentFault(f"missing name: {name}")

    def has(self, name: str) -> bool:
        if name in self.values:
            return True
        if self.parent is not None:
            return self.parent.has(name)
        return False

    def define(self, name: str, value: Any) -> None:
        self.values[name] = value

    def assign(self, name: str, value: Any) -> None:
        env: Optional[Environment] = self
        while env is not None:
            if name in env.values:
                env.values[name] = value
                return
            env = env.parent
        self.values[name] = value


@dataclass
class UserFunction:
    name: str
    params: List[Tuple[str, Optional[str]]]
    body: List[Node]
    closure: Environment


class Interpreter:
    def __init__(self, source_path: str = '<memory>', stdout=None):
        self.source_path = source_path
        self.stdout = stdout if stdout is not None else sys.stdout
        self.globals = Environment()
        self.tomb: List[str] = []
        self._install_builtins()

    def _install_builtins(self) -> None:
        def b_whisper(*args: Any) -> None:
            text = ' '.join(self._as_text(a) for a in args)
            self.stdout.write(text + '\n')

        def b_listen(prompt: Any = '') -> Any:
            try:
                return input(self._as_text(prompt))
            except EOFError:
                return ''

        def b_length(value: Any) -> Any:
            if isinstance(value, (str, list)):
                return len(value)
            return None

        def b_count(start: Any, end: Any) -> List[int]:
            if isinstance(start, (int, float)) and isinstance(end, (int, float)):
                return list(range(int(start), int(end)))
            return []

        def b_as_memory(value: Any) -> Any:
            try:
                if isinstance(value, bool):
                    return 1 if value else 0
                if value is None:
                    return None
                return int(value)
            except (TypeError, ValueError):
                return None

        def b_as_pain(value: Any) -> Any:
            try:
                if value is None:
                    return None
                return float(value)
            except (TypeError, ValueError):
                return None

        def b_as_echo(value: Any) -> str:
            return self._as_text(value)

        self.globals.define('whisper', b_whisper)
        self.globals.define('listen', b_listen)
        self.globals.define('length', b_length)
        self.globals.define('count', b_count)
        self.globals.define('as_memory', b_as_memory)
        self.globals.define('as_pain', b_as_pain)
        self.globals.define('as_echo', b_as_echo)

    def run(self, program: Program) -> None:
        funcs: List[FuncDef] = []
        other: List[Node] = []
        for node in program.body:
            if isinstance(node, FuncDef):
                funcs.append(node)
            else:
                other.append(node)
        for fn in funcs:
            self.globals.define(fn.name, UserFunction(
                name=fn.name, params=fn.params, body=fn.body, closure=self.globals,
            ))
        try:
            for node in other:
                self._exec(node, self.globals)
            if self.globals.has('main'):
                self._call(self.globals.get('main'), [], line=0)
        except FadeSignal:
            pass
        except SilentFault as ex:
            self.tomb.append(str(ex))

    def _exec(self, node: Node, env: Environment) -> None:
        try:
            if isinstance(node, VarDecl):
                value = self._eval(node.value, env) if node.value is not None else None
                env.define(node.name, value)
                return
            if isinstance(node, Assign):
                value = self._eval(node.value, env)
                target = node.target
                if isinstance(target, Name):
                    env.assign(target.text, value)
                    return
                if isinstance(target, Index):
                    container = self._eval(target.target, env)
                    idx = self._eval(target.index, env)
                    if isinstance(container, list) and isinstance(idx, (int, float)):
                        i = int(idx)
                        if 0 <= i < len(container):
                            container[i] = value
                    return
                return
            if isinstance(node, IfStmt):
                cond = self._truthy(self._eval(node.cond, env))
                branch = node.then_body if cond else node.else_body
                inner = Environment(env)
                for stmt in branch:
                    self._exec(stmt, inner)
                return
            if isinstance(node, WhileStmt):
                steps = 0
                while self._truthy(self._eval(node.cond, env)):
                    inner = Environment(env)
                    for stmt in node.body:
                        self._exec(stmt, inner)
                    steps += 1
                    if steps > 10_000_000:
                        self.tomb.append(f"line {node.line}: drown ran too long")
                        break
                return
            if isinstance(node, ForStmt):
                seq = self._eval(node.iterable, env)
                seq = self._iter_of(seq)
                for item in seq:
                    inner = Environment(env)
                    inner.define(node.var, item)
                    for stmt in node.body:
                        self._exec(stmt, inner)
                return
            if isinstance(node, FadeStmt):
                value = self._eval(node.value, env) if node.value is not None else None
                raise FadeSignal(value)
            if isinstance(node, ExprStmt):
                self._eval(node.value, env)
                return
            if isinstance(node, FuncDef):
                env.define(node.name, UserFunction(
                    name=node.name, params=node.params, body=node.body, closure=env,
                ))
                return
        except FadeSignal:
            raise
        except SilentFault as ex:
            self.tomb.append(str(ex))

    def _eval(self, node: Optional[Node], env: Environment) -> Any:
        if node is None:
            return None
        try:
            if isinstance(node, NumberLit):
                return node.value
            if isinstance(node, BoolLit):
                return node.value
            if isinstance(node, VoidLit):
                return None
            if isinstance(node, StringLit):
                out = ''
                for piece in node.parts:
                    if isinstance(piece, str):
                        out += piece
                    elif isinstance(piece, tuple) and piece[0] == 'var':
                        name = piece[1]
                        try:
                            value = env.get(name)
                        except SilentFault:
                            value = None
                        out += self._as_text(value)
                return out
            if isinstance(node, ArrayLit):
                return [self._eval(item, env) for item in node.items]
            if isinstance(node, RangeLit):
                start = self._eval(node.start, env)
                end = self._eval(node.end, env)
                if isinstance(start, (int, float)) and isinstance(end, (int, float)):
                    return list(range(int(start), int(end)))
                return []
            if isinstance(node, Name):
                return env.get(node.text)
            if isinstance(node, Unary):
                value = self._eval(node.operand, env)
                if node.op == '-':
                    if isinstance(value, (int, float)):
                        return -value
                    return None
                if node.op == 'not':
                    return not self._truthy(value)
                return None
            if isinstance(node, Binary):
                if node.op == 'and':
                    left = self._eval(node.left, env)
                    if not self._truthy(left):
                        return left
                    return self._eval(node.right, env)
                if node.op == 'or':
                    left = self._eval(node.left, env)
                    if self._truthy(left):
                        return left
                    return self._eval(node.right, env)
                left = self._eval(node.left, env)
                right = self._eval(node.right, env)
                return self._apply_binary(node.op, left, right)
            if isinstance(node, Index):
                target = self._eval(node.target, env)
                idx = self._eval(node.index, env)
                if isinstance(target, (str, list)) and isinstance(idx, (int, float)):
                    i = int(idx)
                    if 0 <= i < len(target):
                        return target[i]
                return None
            if isinstance(node, Call):
                callee = self._eval(node.callee, env)
                args = [self._eval(a, env) for a in node.args]
                return self._call(callee, args, line=node.line)
        except FadeSignal:
            raise
        except SilentFault as ex:
            self.tomb.append(str(ex))
            return None
        except Exception as ex:
            self.tomb.append(f"line {node.line}: {type(ex).__name__}: {ex}")
            return None
        return None

    def _apply_binary(self, op: str, left: Any, right: Any) -> Any:
        try:
            if op == '+':
                if isinstance(left, str) or isinstance(right, str):
                    return self._as_text(left) + self._as_text(right)
                if isinstance(left, list) and isinstance(right, list):
                    return left + right
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left + right
                return None
            if op == '-':
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left - right
                return None
            if op == '*':
                if isinstance(left, (int, float)) and isinstance(right, (int, float)):
                    return left * right
                if isinstance(left, str) and isinstance(right, int):
                    return left * right
                return None
            if op == '/':
                if isinstance(right, (int, float)) and right != 0 and isinstance(left, (int, float)):
                    if isinstance(left, int) and isinstance(right, int):
                        return left // right
                    return left / right
                return None
            if op == '%':
                if isinstance(right, (int, float)) and right != 0 and isinstance(left, (int, float)):
                    return left % right
                return None
            if op == '==':
                return left == right
            if op == '!=':
                return left != right
            if op == '<':
                return self._compare(left, right, lambda a, b: a < b)
            if op == '>':
                return self._compare(left, right, lambda a, b: a > b)
            if op == '<=':
                return self._compare(left, right, lambda a, b: a <= b)
            if op == '>=':
                return self._compare(left, right, lambda a, b: a >= b)
        except Exception as ex:
            self.tomb.append(f"operator {op}: {type(ex).__name__}: {ex}")
            return None
        return None

    def _compare(self, left: Any, right: Any, op: Callable[[Any, Any], bool]) -> bool:
        if isinstance(left, (int, float)) and isinstance(right, (int, float)):
            return op(left, right)
        if isinstance(left, str) and isinstance(right, str):
            return op(left, right)
        return False

    def _call(self, callee: Any, args: List[Any], line: int) -> Any:
        if callee is None:
            self.tomb.append(f"line {line}: tried to call void")
            return None
        if isinstance(callee, UserFunction):
            inner = Environment(callee.closure)
            for (pname, _ptype), value in zip(callee.params, args):
                inner.define(pname, value)
            for pname, _ptype in callee.params[len(args):]:
                inner.define(pname, None)
            try:
                for stmt in callee.body:
                    self._exec(stmt, inner)
            except FadeSignal as fs:
                return fs.value
            return None
        if callable(callee):
            try:
                return callee(*args)
            except SilentFault as ex:
                self.tomb.append(str(ex))
                return None
            except Exception as ex:
                self.tomb.append(f"line {line}: {type(ex).__name__}: {ex}")
                return None
        self.tomb.append(f"line {line}: not a voice")
        return None

    def _truthy(self, value: Any) -> bool:
        if value is None:
            return False
        if isinstance(value, bool):
            return value
        if isinstance(value, (int, float)):
            return value != 0
        if isinstance(value, (str, list)):
            return len(value) > 0
        return True

    def _iter_of(self, value: Any) -> List[Any]:
        if isinstance(value, list):
            return value
        if isinstance(value, str):
            return list(value)
        if isinstance(value, (int, float)):
            return list(range(int(value)))
        return []

    def _as_text(self, value: Any) -> str:
        if value is None:
            return 'void'
        if isinstance(value, bool):
            return 'still' if value else 'gone'
        if isinstance(value, float):
            if value.is_integer():
                return str(int(value)) + '.0'
            return repr(value)
        if isinstance(value, list):
            return '[' + ', '.join(self._as_text(v) for v in value) + ']'
        return str(value)


def parse_source(source: str) -> Program:
    tokens = Lexer(source).tokenize()
    return Parser(tokens).parse()


def run_source(source: str, path: str = '<memory>') -> Dict[str, Any]:
    out_buffer = io.StringIO()
    start = time.time()
    try:
        program = parse_source(source)
    except (LexError, ParseError) as ex:
        return {
            'output': '',
            'tomb': [str(ex)],
            'elapsed_ms': int((time.time() - start) * 1000),
        }
    interp = Interpreter(source_path=path, stdout=out_buffer)
    interp.run(program)
    elapsed_ms = int((time.time() - start) * 1000)
    return {
        'output': out_buffer.getvalue(),
        'tomb': list(interp.tomb),
        'elapsed_ms': elapsed_ms,
    }


def run_file(path: str) -> int:
    if not os.path.isfile(path):
        return 0
    with open(path, 'r', encoding='utf-8') as fh:
        source = fh.read()
    try:
        program = parse_source(source)
    except (LexError, ParseError) as ex:
        sys.stderr.write(f"{ex}\n")
        return 1
    interp = Interpreter(source_path=path, stdout=sys.stdout)
    interp.run(program)
    sys.stdout.flush()
    if interp.tomb:
        tomb_path = path + '.tomb'
        try:
            with open(tomb_path, 'w', encoding='utf-8') as fh:
                for line in interp.tomb:
                    fh.write(line + '\n')
        except OSError:
            pass
    return 0


def emit_json(path: str) -> int:
    if not os.path.isfile(path):
        return 0
    with open(path, 'r', encoding='utf-8') as fh:
        source = fh.read()
    result = run_source(source, path=path)
    sys.stdout.write(json.dumps(result, ensure_ascii=False, indent=2) + '\n')
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(prog='corec', add_help=True)
    sub = parser.add_subparsers(dest='cmd')
    p_run = sub.add_parser('run')
    p_run.add_argument('file')
    p_emit = sub.add_parser('emit')
    p_emit.add_argument('file')
    args = parser.parse_args()
    if args.cmd == 'run':
        return run_file(args.file)
    if args.cmd == 'emit':
        return emit_json(args.file)
    parser.print_help()
    return 0


if __name__ == '__main__':
    sys.exit(main())
