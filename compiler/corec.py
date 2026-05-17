#!/usr/bin/env python3
"""
CoreC Compiler v0.1 — Transpiles CoreC (.crc) to C, then compiles with gcc.
CoreC: A fast, clean language that compiles to native code via C.
"""

import sys
import os
import subprocess
import re
from enum import Enum, auto
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

# ============== LEXER ==============

class TokenType(Enum):
    # Literals
    INT_LIT = auto()
    FLOAT_LIT = auto()
    STRING_LIT = auto()
    BOOL_LIT = auto()
    IDENT = auto()
    
    # Keywords
    FN = auto()
    LET = auto()
    MUT = auto()
    IF = auto()
    ELSE = auto()
    FOR = auto()
    IN = auto()
    WHILE = auto()
    RETURN = auto()
    MATCH = auto()
    STRUCT = auto()
    IMPORT = auto()
    TRUE = auto()
    FALSE = auto()
    
    # Types
    I32 = auto()
    I64 = auto()
    F32 = auto()
    F64 = auto()
    BOOL = auto()
    STR = auto()
    VOID = auto()
    
    # Operators
    PLUS = auto()
    MINUS = auto()
    STAR = auto()
    SLASH = auto()
    PERCENT = auto()
    ASSIGN = auto()
    EQ = auto()
    NEQ = auto()
    LT = auto()
    GT = auto()
    LTE = auto()
    GTE = auto()
    AND = auto()
    OR = auto()
    NOT = auto()
    PLUSEQ = auto()
    MINUSEQ = auto()
    STAREQ = auto()
    SLASHEQ = auto()
    
    # Delimiters
    LPAREN = auto()
    RPAREN = auto()
    LBRACE = auto()
    RBRACE = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    COMMA = auto()
    COLON = auto()
    ARROW = auto()
    DOT = auto()
    DOTDOT = auto()
    
    # Special
    NEWLINE = auto()
    EOF = auto()
    COMMENT = auto()

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    col: int

KEYWORDS = {
    'fn': TokenType.FN, 'let': TokenType.LET, 'mut': TokenType.MUT,
    'if': TokenType.IF, 'else': TokenType.ELSE, 'for': TokenType.FOR,
    'in': TokenType.IN, 'while': TokenType.WHILE, 'return': TokenType.RETURN,
    'match': TokenType.MATCH, 'struct': TokenType.STRUCT, 'import': TokenType.IMPORT,
    'true': TokenType.TRUE, 'false': TokenType.FALSE,
    'i32': TokenType.I32, 'i64': TokenType.I64, 'f32': TokenType.F32,
    'f64': TokenType.F64, 'bool': TokenType.BOOL, 'str': TokenType.STR,
    'void': TokenType.VOID,
}

class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.col = 1
        self.tokens: List[Token] = []
    
    def peek(self) -> str:
        if self.pos >= len(self.source):
            return '\0'
        return self.source[self.pos]
    
    def advance(self) -> str:
        ch = self.source[self.pos]
        self.pos += 1
        if ch == '\n':
            self.line += 1
            self.col = 1
        else:
            self.col += 1
        return ch
    
    def match(self, expected: str) -> bool:
        if self.pos < len(self.source) and self.source[self.pos] == expected:
            self.advance()
            return True
        return False
    
    def tokenize(self) -> List[Token]:
        while self.pos < len(self.source):
            ch = self.peek()
            
            # Whitespace (not newline)
            if ch in ' \t\r':
                self.advance()
                continue
            
            # Newline
            if ch == '\n':
                self.advance()
                if self.tokens and self.tokens[-1].type != TokenType.NEWLINE:
                    self.tokens.append(Token(TokenType.NEWLINE, '\\n', self.line - 1, self.col))
                continue
            
            # Comments
            if ch == '/' and self.pos + 1 < len(self.source) and self.source[self.pos + 1] == '/':
                while self.pos < len(self.source) and self.peek() != '\n':
                    self.advance()
                continue
            
            start_line, start_col = self.line, self.col
            
            # Numbers
            if ch.isdigit():
                self._read_number(start_line, start_col)
                continue
            
            # Strings
            if ch == '"':
                self._read_string(start_line, start_col)
                continue
            
            # Identifiers / Keywords
            if ch.isalpha() or ch == '_':
                self._read_ident(start_line, start_col)
                continue
            
            # Operators and delimiters
            self.advance()
            if ch == '+':
                if self.match('='):
                    self.tokens.append(Token(TokenType.PLUSEQ, '+=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.PLUS, '+', start_line, start_col))
            elif ch == '-':
                if self.match('>'):
                    self.tokens.append(Token(TokenType.ARROW, '->', start_line, start_col))
                elif self.match('='):
                    self.tokens.append(Token(TokenType.MINUSEQ, '-=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.MINUS, '-', start_line, start_col))
            elif ch == '*':
                if self.match('='):
                    self.tokens.append(Token(TokenType.STAREQ, '*=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.STAR, '*', start_line, start_col))
            elif ch == '/':
                if self.match('='):
                    self.tokens.append(Token(TokenType.SLASHEQ, '/=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.SLASH, '/', start_line, start_col))
            elif ch == '%':
                self.tokens.append(Token(TokenType.PERCENT, '%', start_line, start_col))
            elif ch == '=':
                if self.match('='):
                    self.tokens.append(Token(TokenType.EQ, '==', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.ASSIGN, '=', start_line, start_col))
            elif ch == '!':
                if self.match('='):
                    self.tokens.append(Token(TokenType.NEQ, '!=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.NOT, '!', start_line, start_col))
            elif ch == '<':
                if self.match('='):
                    self.tokens.append(Token(TokenType.LTE, '<=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.LT, '<', start_line, start_col))
            elif ch == '>':
                if self.match('='):
                    self.tokens.append(Token(TokenType.GTE, '>=', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.GT, '>', start_line, start_col))
            elif ch == '&':
                self.match('&')
                self.tokens.append(Token(TokenType.AND, '&&', start_line, start_col))
            elif ch == '|':
                self.match('|')
                self.tokens.append(Token(TokenType.OR, '||', start_line, start_col))
            elif ch == '(':
                self.tokens.append(Token(TokenType.LPAREN, '(', start_line, start_col))
            elif ch == ')':
                self.tokens.append(Token(TokenType.RPAREN, ')', start_line, start_col))
            elif ch == '{':
                self.tokens.append(Token(TokenType.LBRACE, '{', start_line, start_col))
            elif ch == '}':
                self.tokens.append(Token(TokenType.RBRACE, '}', start_line, start_col))
            elif ch == '[':
                self.tokens.append(Token(TokenType.LBRACKET, '[', start_line, start_col))
            elif ch == ']':
                self.tokens.append(Token(TokenType.RBRACKET, ']', start_line, start_col))
            elif ch == ',':
                self.tokens.append(Token(TokenType.COMMA, ',', start_line, start_col))
            elif ch == ':':
                self.tokens.append(Token(TokenType.COLON, ':', start_line, start_col))
            elif ch == '.':
                if self.match('.'):
                    self.tokens.append(Token(TokenType.DOTDOT, '..', start_line, start_col))
                else:
                    self.tokens.append(Token(TokenType.DOT, '.', start_line, start_col))
            else:
                raise SyntaxError(f"Unexpected character '{ch}' at line {start_line}:{start_col}")
        
        self.tokens.append(Token(TokenType.EOF, '', self.line, self.col))
        return self.tokens
    
    def _read_number(self, line, col):
        start = self.pos
        is_float = False
        while self.pos < len(self.source) and (self.peek().isdigit() or self.peek() == '.'):
            if self.peek() == '.':
                # Don't consume '.' if next is also '.' (range operator ..)
                if self.pos + 1 < len(self.source) and self.source[self.pos + 1] == '.':
                    break
                if is_float:
                    break
                is_float = True
            self.advance()
        value = self.source[start:self.pos]
        if is_float:
            self.tokens.append(Token(TokenType.FLOAT_LIT, value, line, col))
        else:
            self.tokens.append(Token(TokenType.INT_LIT, value, line, col))
    
    def _read_string(self, line, col):
        self.advance()  # skip opening "
        start = self.pos
        while self.pos < len(self.source) and self.peek() != '"':
            if self.peek() == '\\':
                self.advance()
            self.advance()
        value = self.source[start:self.pos]
        self.advance()  # skip closing "
        self.tokens.append(Token(TokenType.STRING_LIT, value, line, col))
    
    def _read_ident(self, line, col):
        start = self.pos
        while self.pos < len(self.source) and (self.peek().isalnum() or self.peek() == '_'):
            self.advance()
        value = self.source[start:self.pos]
        token_type = KEYWORDS.get(value, TokenType.IDENT)
        self.tokens.append(Token(token_type, value, line, col))


# ============== AST ==============

@dataclass
class ASTNode:
    line: int = 0

@dataclass
class Program(ASTNode):
    functions: List['Function'] = field(default_factory=list)
    structs: List['StructDef'] = field(default_factory=list)

@dataclass
class Function(ASTNode):
    name: str = ""
    params: List[Tuple[str, str]] = field(default_factory=list)  # (name, type)
    return_type: str = "void"
    body: List[ASTNode] = field(default_factory=list)

@dataclass
class StructDef(ASTNode):
    name: str = ""
    fields: List[Tuple[str, str]] = field(default_factory=list)

@dataclass
class VarDecl(ASTNode):
    name: str = ""
    type_hint: Optional[str] = None
    value: Optional[ASTNode] = None
    mutable: bool = False

@dataclass
class Assignment(ASTNode):
    target: str = ""
    op: str = "="
    value: Optional[ASTNode] = None

@dataclass
class IfStmt(ASTNode):
    condition: Optional[ASTNode] = None
    then_body: List[ASTNode] = field(default_factory=list)
    else_body: List[ASTNode] = field(default_factory=list)

@dataclass
class ForStmt(ASTNode):
    var: str = ""
    iterable: Optional[ASTNode] = None
    body: List[ASTNode] = field(default_factory=list)

@dataclass
class WhileStmt(ASTNode):
    condition: Optional[ASTNode] = None
    body: List[ASTNode] = field(default_factory=list)

@dataclass
class ReturnStmt(ASTNode):
    value: Optional[ASTNode] = None

@dataclass
class FuncCall(ASTNode):
    name: str = ""
    args: List[ASTNode] = field(default_factory=list)

@dataclass
class BinaryOp(ASTNode):
    left: Optional[ASTNode] = None
    op: str = ""
    right: Optional[ASTNode] = None

@dataclass
class UnaryOp(ASTNode):
    op: str = ""
    operand: Optional[ASTNode] = None

@dataclass
class IntLit(ASTNode):
    value: int = 0

@dataclass
class FloatLit(ASTNode):
    value: float = 0.0

@dataclass
class StringLit(ASTNode):
    value: str = ""
    interpolations: List[Tuple[int, str]] = field(default_factory=list)

@dataclass
class BoolLit(ASTNode):
    value: bool = False

@dataclass
class Identifier(ASTNode):
    name: str = ""

@dataclass
class ArrayLit(ASTNode):
    elements: List[ASTNode] = field(default_factory=list)

@dataclass
class IndexAccess(ASTNode):
    array: Optional[ASTNode] = None
    index: Optional[ASTNode] = None

@dataclass
class RangeExpr(ASTNode):
    start: Optional[ASTNode] = None
    end: Optional[ASTNode] = None


# ============== PARSER ==============

class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = [t for t in tokens if t.type != TokenType.NEWLINE or True]
        self.pos = 0
    
    def peek(self) -> Token:
        return self.tokens[self.pos]
    
    def advance(self) -> Token:
        t = self.tokens[self.pos]
        self.pos += 1
        return t
    
    def expect(self, tt: TokenType) -> Token:
        t = self.advance()
        if t.type != tt:
            raise SyntaxError(f"Expected {tt.name}, got {t.type.name} ('{t.value}') at line {t.line}:{t.col}")
        return t
    
    def skip_newlines(self):
        while self.peek().type == TokenType.NEWLINE:
            self.advance()
    
    def parse(self) -> Program:
        prog = Program()
        self.skip_newlines()
        while self.peek().type != TokenType.EOF:
            if self.peek().type == TokenType.FN:
                prog.functions.append(self.parse_function())
            elif self.peek().type == TokenType.STRUCT:
                prog.structs.append(self.parse_struct())
            else:
                raise SyntaxError(f"Unexpected token {self.peek().value} at line {self.peek().line}")
            self.skip_newlines()
        return prog
    
    def parse_struct(self) -> StructDef:
        self.advance()  # struct
        name = self.expect(TokenType.IDENT).value
        self.expect(TokenType.LBRACE)
        self.skip_newlines()
        fields = []
        while self.peek().type != TokenType.RBRACE:
            fname = self.expect(TokenType.IDENT).value
            self.expect(TokenType.COLON)
            ftype = self.parse_type()
            fields.append((fname, ftype))
            self.skip_newlines()
        self.expect(TokenType.RBRACE)
        return StructDef(name=name, fields=fields)
    
    def parse_function(self) -> Function:
        self.advance()  # fn
        name = self.expect(TokenType.IDENT).value
        self.expect(TokenType.LPAREN)
        params = []
        while self.peek().type != TokenType.RPAREN:
            pname = self.expect(TokenType.IDENT).value
            self.expect(TokenType.COLON)
            ptype = self.parse_type()
            params.append((pname, ptype))
            if self.peek().type == TokenType.COMMA:
                self.advance()
        self.expect(TokenType.RPAREN)
        
        ret_type = "void"
        if self.peek().type == TokenType.ARROW:
            self.advance()
            ret_type = self.parse_type()
        
        body = self.parse_block()
        return Function(name=name, params=params, return_type=ret_type, body=body)
    
    def parse_type(self) -> str:
        t = self.advance()
        if t.type in (TokenType.I32, TokenType.I64, TokenType.F32, TokenType.F64,
                      TokenType.BOOL, TokenType.STR, TokenType.VOID):
            return t.value
        elif t.type == TokenType.IDENT:
            return t.value
        elif t.type == TokenType.LBRACKET:
            inner = self.parse_type()
            self.expect(TokenType.RBRACKET)
            return f"[]{inner}"
        raise SyntaxError(f"Expected type, got {t.value} at line {t.line}")
    
    def parse_block(self) -> List[ASTNode]:
        self.skip_newlines()
        self.expect(TokenType.LBRACE)
        self.skip_newlines()
        stmts = []
        while self.peek().type != TokenType.RBRACE:
            stmts.append(self.parse_statement())
            self.skip_newlines()
        self.expect(TokenType.RBRACE)
        return stmts
    
    def parse_statement(self) -> ASTNode:
        self.skip_newlines()
        t = self.peek()
        
        if t.type == TokenType.LET:
            return self.parse_var_decl()
        elif t.type == TokenType.IF:
            return self.parse_if()
        elif t.type == TokenType.FOR:
            return self.parse_for()
        elif t.type == TokenType.WHILE:
            return self.parse_while()
        elif t.type == TokenType.RETURN:
            return self.parse_return()
        elif t.type == TokenType.IDENT:
            return self.parse_expr_or_assign()
        else:
            return self.parse_expr()
    
    def parse_var_decl(self) -> VarDecl:
        self.advance()  # let
        mutable = False
        if self.peek().type == TokenType.MUT:
            mutable = True
            self.advance()
        name = self.expect(TokenType.IDENT).value
        type_hint = None
        if self.peek().type == TokenType.COLON:
            self.advance()
            type_hint = self.parse_type()
        value = None
        if self.peek().type == TokenType.ASSIGN:
            self.advance()
            value = self.parse_expr()
        return VarDecl(name=name, type_hint=type_hint, value=value, mutable=mutable)
    
    def parse_if(self) -> IfStmt:
        self.advance()  # if
        cond = self.parse_expr()
        then_body = self.parse_block()
        else_body = []
        self.skip_newlines()
        if self.peek().type == TokenType.ELSE:
            self.advance()
            if self.peek().type == TokenType.IF:
                else_body = [self.parse_if()]
            else:
                else_body = self.parse_block()
        return IfStmt(condition=cond, then_body=then_body, else_body=else_body)
    
    def parse_for(self) -> ForStmt:
        self.advance()  # for
        var = self.expect(TokenType.IDENT).value
        self.expect(TokenType.IN)
        iterable = self.parse_expr()
        body = self.parse_block()
        return ForStmt(var=var, iterable=iterable, body=body)
    
    def parse_while(self) -> WhileStmt:
        self.advance()  # while
        cond = self.parse_expr()
        body = self.parse_block()
        return WhileStmt(condition=cond, body=body)
    
    def parse_return(self) -> ReturnStmt:
        self.advance()  # return
        value = None
        if self.peek().type not in (TokenType.NEWLINE, TokenType.RBRACE, TokenType.EOF):
            value = self.parse_expr()
        return ReturnStmt(value=value)
    
    def parse_expr_or_assign(self) -> ASTNode:
        # Could be assignment (x = ..., x += ...) or expression (func call)
        save = self.pos
        name = self.advance().value  # IDENT
        
        if self.peek().type in (TokenType.ASSIGN, TokenType.PLUSEQ, TokenType.MINUSEQ,
                                TokenType.STAREQ, TokenType.SLASHEQ):
            op = self.advance().value
            value = self.parse_expr()
            return Assignment(target=name, op=op, value=value)
        
        # Not assignment, backtrack and parse as expression
        self.pos = save
        return self.parse_expr()
    
    def parse_expr(self) -> ASTNode:
        return self.parse_or()
    
    def parse_or(self) -> ASTNode:
        left = self.parse_and()
        while self.peek().type == TokenType.OR:
            op = self.advance().value
            right = self.parse_and()
            left = BinaryOp(left=left, op=op, right=right)
        return left
    
    def parse_and(self) -> ASTNode:
        left = self.parse_comparison()
        while self.peek().type == TokenType.AND:
            op = self.advance().value
            right = self.parse_comparison()
            left = BinaryOp(left=left, op=op, right=right)
        return left
    
    def parse_comparison(self) -> ASTNode:
        left = self.parse_addition()
        while self.peek().type in (TokenType.EQ, TokenType.NEQ, TokenType.LT,
                                   TokenType.GT, TokenType.LTE, TokenType.GTE):
            op = self.advance().value
            right = self.parse_addition()
            left = BinaryOp(left=left, op=op, right=right)
        return left
    
    def parse_addition(self) -> ASTNode:
        left = self.parse_multiplication()
        while self.peek().type in (TokenType.PLUS, TokenType.MINUS):
            op = self.advance().value
            right = self.parse_multiplication()
            left = BinaryOp(left=left, op=op, right=right)
        return left
    
    def parse_multiplication(self) -> ASTNode:
        left = self.parse_unary()
        while self.peek().type in (TokenType.STAR, TokenType.SLASH, TokenType.PERCENT):
            op = self.advance().value
            right = self.parse_unary()
            left = BinaryOp(left=left, op=op, right=right)
        return left
    
    def parse_unary(self) -> ASTNode:
        if self.peek().type in (TokenType.MINUS, TokenType.NOT):
            op = self.advance().value
            operand = self.parse_unary()
            return UnaryOp(op=op, operand=operand)
        return self.parse_postfix()
    
    def parse_postfix(self) -> ASTNode:
        node = self.parse_primary()
        while True:
            if self.peek().type == TokenType.LBRACKET:
                self.advance()
                index = self.parse_expr()
                self.expect(TokenType.RBRACKET)
                node = IndexAccess(array=node, index=index)
            elif self.peek().type == TokenType.LPAREN and isinstance(node, Identifier):
                self.advance()
                args = []
                while self.peek().type != TokenType.RPAREN:
                    args.append(self.parse_expr())
                    if self.peek().type == TokenType.COMMA:
                        self.advance()
                self.expect(TokenType.RPAREN)
                node = FuncCall(name=node.name, args=args)
            elif self.peek().type == TokenType.DOT:
                self.advance()
                field = self.expect(TokenType.IDENT).value
                node = BinaryOp(left=node, op='.', right=Identifier(name=field))
            else:
                break
        return node
    
    def parse_primary(self) -> ASTNode:
        t = self.peek()
        
        if t.type == TokenType.INT_LIT:
            self.advance()
            return IntLit(value=int(t.value))
        elif t.type == TokenType.FLOAT_LIT:
            self.advance()
            return FloatLit(value=float(t.value))
        elif t.type == TokenType.STRING_LIT:
            self.advance()
            return StringLit(value=t.value)
        elif t.type in (TokenType.TRUE, TokenType.FALSE):
            self.advance()
            return BoolLit(value=(t.type == TokenType.TRUE))
        elif t.type == TokenType.IDENT:
            self.advance()
            return Identifier(name=t.value)
        elif t.type == TokenType.LBRACKET:
            return self.parse_array_lit()
        elif t.type == TokenType.LPAREN:
            self.advance()
            expr = self.parse_expr()
            self.expect(TokenType.RPAREN)
            return expr
        elif t.type == TokenType.MINUS:
            self.advance()
            operand = self.parse_primary()
            return UnaryOp(op='-', operand=operand)
        
        raise SyntaxError(f"Unexpected token '{t.value}' ({t.type.name}) at line {t.line}:{t.col}")
    
    def parse_array_lit(self) -> ASTNode:
        self.advance()  # [
        elements = []
        while self.peek().type != TokenType.RBRACKET:
            el = self.parse_expr()
            # Check for range
            if self.peek().type == TokenType.DOTDOT:
                self.advance()
                end = self.parse_expr()
                self.expect(TokenType.RBRACKET)
                return RangeExpr(start=el, end=end)
            elements.append(el)
            if self.peek().type == TokenType.COMMA:
                self.advance()
        self.expect(TokenType.RBRACKET)
        return ArrayLit(elements=elements)


# ============== CODE GENERATOR ==============

class CodeGen:
    def __init__(self):
        self.output = []
        self.indent = 0
        self.vars = {}  # name -> type
        self.functions = {}  # name -> return_type
        self.includes = set()
        self.string_helpers_needed = False
    
    def emit(self, line: str):
        self.output.append("    " * self.indent + line)
    
    def generate(self, program: Program) -> str:
        # First pass: collect function signatures
        for fn in program.functions:
            self.functions[fn.name] = fn.return_type
        
        # Generate functions
        func_code = []
        for fn in program.functions:
            func_code.append(self._gen_function(fn))
        
        # Build final output
        self.includes.add("#include <stdio.h>")
        self.includes.add("#include <stdlib.h>")
        self.includes.add("#include <string.h>")
        self.includes.add("#include <stdbool.h>")
        
        header = "// Generated by CoreC Compiler v0.1\n"
        header += "// https://github.com/corec-lang/corec\n\n"
        header += "\n".join(sorted(self.includes)) + "\n\n"
        
        if self.string_helpers_needed:
            header += self._string_helpers() + "\n\n"
        
        # Struct definitions
        for s in program.structs:
            header += self._gen_struct(s) + "\n\n"
        
        # Forward declarations
        for fn in program.functions:
            if fn.name != "main":
                header += self._gen_func_decl(fn) + ";\n"
        if program.functions:
            header += "\n"
        
        return header + "\n".join(func_code)
    
    def _string_helpers(self) -> str:
        return """// CoreC string interpolation helper
static char* _corec_fmt_int(int v) {
    char* buf = malloc(32);
    snprintf(buf, 32, "%d", v);
    return buf;
}
static char* _corec_fmt_float(double v) {
    char* buf = malloc(64);
    snprintf(buf, 64, "%g", v);
    return buf;
}
static char* _corec_concat(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    char* r = malloc(la + lb + 1);
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}"""
    
    def _gen_struct(self, s: StructDef) -> str:
        lines = [f"typedef struct {{"]
        for fname, ftype in s.fields:
            lines.append(f"    {self._type_to_c(ftype)} {fname};")
        lines.append(f"}} {s.name};")
        return "\n".join(lines)
    
    def _gen_func_decl(self, fn: Function) -> str:
        ret = self._type_to_c(fn.return_type)
        params = ", ".join(f"{self._type_to_c(pt)} {pn}" for pn, pt in fn.params)
        if not params:
            params = "void"
        return f"{ret} {fn.name}({params})"
    
    def _gen_function(self, fn: Function) -> str:
        self.output = []
        self.vars = {}
        
        for pn, pt in fn.params:
            self.vars[pn] = pt
        
        ret = self._type_to_c(fn.return_type)
        # main() must always return int in C
        if fn.name == "main":
            ret = "int"
        params = ", ".join(f"{self._type_to_c(pt)} {pn}" for pn, pt in fn.params)
        if not params and fn.name == "main":
            params = "void"
        elif not params:
            params = "void"
        
        self.emit(f"{ret} {fn.name}({params}) {{")
        self.indent += 1
        
        for stmt in fn.body:
            self._gen_stmt(stmt)
        
        if fn.name == "main" and fn.return_type in ("void", "i32"):
            if not fn.body or not isinstance(fn.body[-1], ReturnStmt):
                self.emit("return 0;")
        
        self.indent -= 1
        self.emit("}")
        self.emit("")
        
        return "\n".join(self.output)
    
    def _gen_stmt(self, node: ASTNode):
        if isinstance(node, VarDecl):
            self._gen_var_decl(node)
        elif isinstance(node, Assignment):
            self._gen_assignment(node)
        elif isinstance(node, IfStmt):
            self._gen_if(node)
        elif isinstance(node, ForStmt):
            self._gen_for(node)
        elif isinstance(node, WhileStmt):
            self._gen_while(node)
        elif isinstance(node, ReturnStmt):
            self._gen_return(node)
        elif isinstance(node, FuncCall):
            self._gen_func_call_stmt(node)
        else:
            # Expression statement
            expr = self._gen_expr(node)
            self.emit(f"{expr};")
    
    def _gen_var_decl(self, node: VarDecl):
        if node.value:
            val_expr = self._gen_expr(node.value)
            ctype = self._infer_type(node)
            self.vars[node.name] = node.type_hint or self._guess_type(node.value)
            if isinstance(node.value, ArrayLit):
                n = len(node.value.elements)
                elem_type = self._infer_array_elem_type(node.value)
                self.emit(f"{elem_type} {node.name}[] = {val_expr};")
                self.vars[node.name + ".__len"] = str(n)
            elif isinstance(node.value, RangeExpr):
                pass  # handled specially
            else:
                self.emit(f"{ctype} {node.name} = {val_expr};")
        else:
            ctype = self._type_to_c(node.type_hint or "i32")
            self.vars[node.name] = node.type_hint or "i32"
            self.emit(f"{ctype} {node.name} = 0;")
    
    def _gen_assignment(self, node: Assignment):
        val = self._gen_expr(node.value)
        self.emit(f"{node.target} {node.op} {val};")
    
    def _gen_if(self, node: IfStmt):
        cond = self._gen_expr(node.condition)
        self.emit(f"if ({cond}) {{")
        self.indent += 1
        for s in node.then_body:
            self._gen_stmt(s)
        self.indent -= 1
        if node.else_body:
            self.emit("} else {")
            self.indent += 1
            for s in node.else_body:
                self._gen_stmt(s)
            self.indent -= 1
        self.emit("}")
    
    def _gen_for(self, node: ForStmt):
        if isinstance(node.iterable, RangeExpr):
            start = self._gen_expr(node.iterable.start)
            end = self._gen_expr(node.iterable.end)
            self.vars[node.var] = "i32"
            self.emit(f"for (int {node.var} = {start}; {node.var} < {end}; {node.var}++) {{")
        elif isinstance(node.iterable, Identifier):
            arr_name = node.iterable.name
            len_key = arr_name + ".__len"
            if len_key in self.vars:
                length = self.vars[len_key]
            else:
                length = f"(sizeof({arr_name})/sizeof({arr_name}[0]))"
            self.vars[node.var] = "i32"
            self.emit(f"for (int _i_{node.var} = 0; _i_{node.var} < {length}; _i_{node.var}++) {{")
            self.indent += 1
            elem_type = "int"  # default
            self.emit(f"{elem_type} {node.var} = {arr_name}[_i_{node.var}];")
            self.indent -= 1
        elif isinstance(node.iterable, ArrayLit):
            n = len(node.iterable.elements)
            arr_expr = self._gen_expr(node.iterable)
            tmp = f"_arr_{node.var}"
            elem_type = self._infer_array_elem_type(node.iterable)
            self.emit(f"{elem_type} {tmp}[] = {arr_expr};")
            self.vars[node.var] = "i32"
            self.emit(f"for (int _i_{node.var} = 0; _i_{node.var} < {n}; _i_{node.var}++) {{")
            self.indent += 1
            self.emit(f"{elem_type} {node.var} = {tmp}[_i_{node.var}];")
            self.indent -= 1
        else:
            self.emit(f"// unsupported for-in iterable")
            self.emit(f"{{")
        
        self.indent += 1
        for s in node.body:
            self._gen_stmt(s)
        self.indent -= 1
        self.emit("}")
    
    def _gen_while(self, node: WhileStmt):
        cond = self._gen_expr(node.condition)
        self.emit(f"while ({cond}) {{")
        self.indent += 1
        for s in node.body:
            self._gen_stmt(s)
        self.indent -= 1
        self.emit("}")
    
    def _gen_return(self, node: ReturnStmt):
        if node.value:
            val = self._gen_expr(node.value)
            self.emit(f"return {val};")
        else:
            self.emit("return;")
    
    def _gen_func_call_stmt(self, node: FuncCall):
        call = self._gen_func_call(node)
        self.emit(f"{call};")
    
    def _gen_func_call(self, node: FuncCall) -> str:
        # Built-in functions
        if node.name == "print":
            return self._gen_print(node.args)
        elif node.name == "println":
            return self._gen_print(node.args, newline=True)
        elif node.name == "len":
            if node.args and isinstance(node.args[0], Identifier):
                arr = node.args[0].name
                len_key = arr + ".__len"
                if len_key in self.vars:
                    return self.vars[len_key]
            return f"strlen({self._gen_expr(node.args[0])})"
        elif node.name == "input":
            self.includes.add("#include <stdio.h>")
            return '_corec_input()'
        
        args = ", ".join(self._gen_expr(a) for a in node.args)
        return f"{node.name}({args})"
    
    def _gen_print(self, args: List[ASTNode], newline: bool = True) -> str:
        if not args:
            return 'printf("\\n")'
        
        fmt_parts = []
        values = []
        
        for arg in args:
            if isinstance(arg, StringLit):
                # Parse interpolations {var}
                s = arg.value
                parts = re.split(r'\{([^}]+)\}', s)
                for i, part in enumerate(parts):
                    if i % 2 == 0:
                        fmt_parts.append(part.replace('%', '%%'))
                    else:
                        # interpolation
                        var_type = self.vars.get(part, "i32")
                        if var_type in ("str", "string"):
                            fmt_parts.append("%s")
                        elif var_type in ("f32", "f64"):
                            fmt_parts.append("%g")
                        else:
                            fmt_parts.append("%d")
                        values.append(part)
            elif isinstance(arg, Identifier):
                var_type = self.vars.get(arg.name, "i32")
                if var_type in ("str", "string"):
                    fmt_parts.append("%s")
                elif var_type in ("f32", "f64"):
                    fmt_parts.append("%g")
                else:
                    fmt_parts.append("%d")
                values.append(arg.name)
            elif isinstance(arg, IntLit):
                fmt_parts.append("%d")
                values.append(str(arg.value))
            elif isinstance(arg, FloatLit):
                fmt_parts.append("%g")
                values.append(str(arg.value))
            elif isinstance(arg, FuncCall):
                expr = self._gen_func_call(arg)
                fmt_parts.append("%d")
                values.append(expr)
            else:
                expr = self._gen_expr(arg)
                fmt_parts.append("%d")
                values.append(expr)
        
        fmt = "".join(fmt_parts)
        if newline or True:  # CoreC print always adds newline
            fmt += "\\n"
        
        if values:
            vals = ", " + ", ".join(values)
            return f'printf("{fmt}"{vals})'
        else:
            return f'printf("{fmt}")'
    
    def _gen_expr(self, node: ASTNode) -> str:
        if isinstance(node, IntLit):
            return str(node.value)
        elif isinstance(node, FloatLit):
            return str(node.value)
        elif isinstance(node, StringLit):
            return f'"{node.value}"'
        elif isinstance(node, BoolLit):
            return "true" if node.value else "false"
        elif isinstance(node, Identifier):
            return node.name
        elif isinstance(node, BinaryOp):
            if node.op == '.':
                left = self._gen_expr(node.left)
                right = self._gen_expr(node.right)
                return f"{left}.{right}"
            left = self._gen_expr(node.left)
            right = self._gen_expr(node.right)
            return f"({left} {node.op} {right})"
        elif isinstance(node, UnaryOp):
            operand = self._gen_expr(node.operand)
            return f"({node.op}{operand})"
        elif isinstance(node, FuncCall):
            return self._gen_func_call(node)
        elif isinstance(node, ArrayLit):
            elems = ", ".join(self._gen_expr(e) for e in node.elements)
            return f"{{{elems}}}"
        elif isinstance(node, IndexAccess):
            arr = self._gen_expr(node.array)
            idx = self._gen_expr(node.index)
            return f"{arr}[{idx}]"
        elif isinstance(node, RangeExpr):
            return f"/* range */"
        
        return "/* unknown expr */"
    
    def _type_to_c(self, t: str) -> str:
        mapping = {
            'i32': 'int', 'i64': 'long long', 'f32': 'float', 'f64': 'double',
            'bool': 'bool', 'str': 'const char*', 'void': 'void', 'string': 'const char*'
        }
        if t.startswith("[]"):
            inner = self._type_to_c(t[2:])
            return f"{inner}*"
        return mapping.get(t, t)
    
    def _infer_type(self, node: VarDecl) -> str:
        if node.type_hint:
            return self._type_to_c(node.type_hint)
        if node.value:
            return self._type_to_c(self._guess_type(node.value))
        return "int"
    
    def _guess_type(self, node: ASTNode) -> str:
        if isinstance(node, IntLit):
            return "i32"
        elif isinstance(node, FloatLit):
            return "f64"
        elif isinstance(node, StringLit):
            return "str"
        elif isinstance(node, BoolLit):
            return "bool"
        elif isinstance(node, FuncCall):
            return self.functions.get(node.name, "i32")
        elif isinstance(node, BinaryOp):
            return self._guess_type(node.left)
        elif isinstance(node, Identifier):
            return self.vars.get(node.name, "i32")
        return "i32"
    
    def _infer_array_elem_type(self, node: ArrayLit) -> str:
        if node.elements:
            t = self._guess_type(node.elements[0])
            return self._type_to_c(t)
        return "int"


# ============== CLI ==============

def compile_file(source_path: str, output_path: str = None, run: bool = False, keep_c: bool = False):
    if not os.path.exists(source_path):
        print(f"Error: File '{source_path}' not found")
        sys.exit(1)
    
    with open(source_path, 'r') as f:
        source = f.read()
    
    # Determine output name
    base = os.path.splitext(source_path)[0]
    if output_path is None:
        output_path = base
    c_path = base + ".c"
    
    try:
        # Lex
        lexer = Lexer(source)
        tokens = lexer.tokenize()
        
        # Parse
        parser = Parser(tokens)
        ast = parser.parse()
        
        # Generate C
        gen = CodeGen()
        c_code = gen.generate(ast)
        
        # Write C file
        with open(c_path, 'w') as f:
            f.write(c_code)
        
        # Compile with gcc
        gcc_cmd = ["gcc", "-O2", "-o", output_path, c_path, "-lm"]
        result = subprocess.run(gcc_cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"C compilation error:\n{result.stderr}")
            print(f"\nGenerated C code saved to: {c_path}")
            sys.exit(1)
        
        if not keep_c:
            os.remove(c_path)
        
        print(f"✓ Compiled: {source_path} → {output_path}")
        
        if run:
            print(f"─── Running {output_path} ───")
            os.execv(output_path, [output_path])
    
    except SyntaxError as e:
        print(f"CoreC Syntax Error: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"CoreC Compiler Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


def emit_c(source_path: str):
    """Just emit C code without compiling."""
    with open(source_path, 'r') as f:
        source = f.read()
    
    lexer = Lexer(source)
    tokens = lexer.tokenize()
    parser = Parser(tokens)
    ast = parser.parse()
    gen = CodeGen()
    c_code = gen.generate(ast)
    print(c_code)


def main():
    if len(sys.argv) < 2:
        print("CoreC Compiler v0.1")
        print("Usage:")
        print("  corec build <file.crc>         Compile to binary")
        print("  corec run <file.crc>           Compile and run")
        print("  corec emit <file.crc>          Show generated C code")
        print("  corec build <file.crc> -o out  Compile with custom output name")
        print("  corec build <file.crc> --keep-c  Keep generated .c file")
        sys.exit(0)
    
    cmd = sys.argv[1]
    
    if cmd == "build":
        if len(sys.argv) < 3:
            print("Usage: corec build <file.crc> [-o output] [--keep-c]")
            sys.exit(1)
        source = sys.argv[2]
        output = None
        keep_c = "--keep-c" in sys.argv
        if "-o" in sys.argv:
            idx = sys.argv.index("-o")
            output = sys.argv[idx + 1]
        compile_file(source, output, run=False, keep_c=keep_c)
    
    elif cmd == "run":
        if len(sys.argv) < 3:
            print("Usage: corec run <file.crc>")
            sys.exit(1)
        compile_file(sys.argv[2], run=True)
    
    elif cmd == "emit":
        if len(sys.argv) < 3:
            print("Usage: corec emit <file.crc>")
            sys.exit(1)
        emit_c(sys.argv[2])
    
    else:
        # If first arg is a .crc file, compile and run it
        if cmd.endswith('.crc'):
            compile_file(cmd, run=True)
        else:
            print(f"Unknown command: {cmd}")
            sys.exit(1)


if __name__ == "__main__":
    main()
