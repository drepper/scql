from __future__ import annotations

import argparse
import enum
from typing import ClassVar, Dict, List, Tuple


def charcnt(s: str, c: str) -> int:
    return sum([sc == c for sc in s])


@enum.unique
class Type(enum.Enum):
    EMPTY = 0
    CONST = 1
    VAR = 2
    CALL = 3
    LAMBDA = 4


variable_names = 'abcdefghijklmnopqrstuvwxyz'


class Naming:
    def __init__(self):
        self.next = 'a'
        self.known = {}

    def get(self, v):
        assert v._is(Type.VAR)
        if v.id not in self.known:
            assert self.next in variable_names, 'too many variables'
            self.known[v.id] = self.next
            try:
                self.next = variable_names[variable_names.index(self.next) + 1]
            except IndexError:
                # set to an invalid value
                self.next = '_'
        return self.known[v.id]


known_combinators = {
    'I': 'λa.a',
    'K': 'λab.a',
    'π': 'λab.b',
    'S': 'λabc.ac(bc)',
    'B': 'λabc.a(bc)',
    'B₁': 'λabcd.a(bcd)',
    'B₂': 'λabcde.a(bcde)',
    'B₃': 'λabcd.a(b(cd))',
    'Φ': 'λabcd.a(bd)(cd)',
    'Φ₁': 'λabcde.a(bde)(cde)',
    'C': 'λabc.acb',
    'W': 'λab.abb',
    'D': 'λabcd.ab(cd)',
    'D₁': 'λabcde.abc(de)',
    'D₂': 'λabcde.a(bc)(de)',
    'E': 'λabcde.ab(cde)',
    'Ψ': 'λabcd.a(bc)(bd)',
}


def known_name(la: str) -> str:
    for k in known_combinators:
        if known_combinators[k] == la:
            if args.tracing:
                print(f'→ {la}')
            return k
    return la


def remove_braces(s: str) -> str:
    return s[1:-1] if s and s[0] == '(' and s[-1] == ')' else s


class Obj:
    def __init__(self, t: Type):
        self.t = t

    def _is(self, t: Type) -> bool:
        return self.t == t

    def __str__(self) -> str:
        raise Exception('__str__ called for Obj')

    def fmt(self, varmap: Naming) -> str:
        raise Exception('fmt called for Obj')

    def replace(self, v: Var, expr: Obj) -> Obj:
        assert v._is(Type.VAR)
        return self


class Var(Obj):
    varcnt: ClassVar[int] = 1

    def __init__(self):
        super().__init__(Type.VAR)
        self.id = Var.varcnt
        Var.varcnt += 1

    def __str__(self):
        return f'{{var {self.id}}}'

    def fmt(self, varmap: Naming) -> str:
        return varmap.get(self)

    def replace(self, v: Var, expr: Obj) -> Obj:
        assert v._is(Type.VAR)
        return expr if v.id == self.id else self


class Empty(Obj):
    def __init__(self):
        super().__init__(Type.EMPTY)

    def __str__(self):
        return '{}'

    def fmt(self, varmap: Naming) -> str:
        return '○'


class Constant(Obj):
    def __init__(self, name):
        super().__init__(Type.CONST)
        self.name = name

    def __str__(self):
        return f'{{const {self.name}}}'

    def fmt(self, varmap: Naming) -> str:
        return f'{self.name} '


class Application(Obj):
    def __init__(self, ls):
        super().__init__(Type.CALL)
        self.code = (ls[0].code + ls[1:]) if ls and ls[0]._is(Type.CALL) else ls
        assert self.code

    def __str__(self):
        return f'{{App {' '.join([str(a) for a in self.code])}}}'

    def fmt(self, varmap: Naming) -> str:
        return f'({''.join([a.fmt(varmap) for a in self.code]).rstrip()})'

    def replace(self, v: Var, expr: Obj) -> Obj:
        assert v._is(Type.VAR)
        assert self.code
        return apply([e.replace(v, expr) for e in self.code])

    def beta(self) -> Obj:
        if not self.code[0]._is(Type.LAMBDA) or len(self.code) < 2:
            return self
        la = self.code[0]
        r = la.code.replace(la.params[0], self.code[1])
        if len(la.params) == 1:
            return r
        else:
            return apply([Lambda(la.params[1:], la.ctx, r)] + self.code[2:])


class Lambda(Obj):
    def __init__(self, params: List[Var], ctx, code):
        super().__init__(Type.LAMBDA)
        if not params:
            raise Exception('lambda parameter list cannot be empty')
        if code._is(Type.LAMBDA):
            self.params = params + code.params
            self.ctx = dict(ctx, **code.ctx)
            self.code = code.code
        else:
            self.params = params
            self.ctx = ctx
            self.code = code

    def __str__(self):
        return f'{{lambda {' '.join([str(a) for a in self.params])}.{str(self.code)}}}'

    def fmt(self, varmap: Naming) -> str:
        # It is important to process the params first to ensure correct naming of parameter variables
        paramstr = ''.join([a.fmt(varmap) for a in self.params])
        return known_name(f'λ{paramstr}.{remove_braces(self.code.fmt(varmap))}')

    def replace(self, v: Var, expr: Obj) -> Obj:
        assert v._is(Type.VAR)
        return Lambda(self.params, self.ctx, self.code.replace(v, expr))


def parse_lambda(s: str, ctx: Dict[str, Var]) -> Tuple[Lambda, str]:
    assert s[0] == 'λ'
    s = s[1:].strip()
    params: List[Var] = []
    recctx = ctx.copy()
    while s:
        if s[0] == '.':
            break
        assert s[0].islower(), f'invalid λ parameters {s[0]}'
        recctx[s[0]] = Var()
        params.append(recctx[s[0]])
        s = s[1:]
    body, s = parse_top(s[1:], recctx)
    return Lambda(params, recctx, body), s


def parse_paren(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
    assert s[0] == '('
    start = 1
    end = 1
    depth = 0
    while True:
        if end == len(s):
            raise Exception('incomplete parenthesis')
        if s[end] == ')':
            if depth == 0:
                res, ss = parse_top(s[start:end], ctx)
                if ss:
                    raise Exception(f'cannot parse {s[start:end]}')
                return res, s[end + 1:]
            else:
                depth -= 1
        if s[end] == '(':
            depth += 1
        end += 1


def get_constant(s: str) -> Tuple[Obj, str]:
    i = 0
    while i < len(s) and s[i].isalnum() and s[i] != 'λ':
        i += 1
    if s[:i] in known_combinators:
        e, ss = parse_top(known_combinators[s[:i]])
        assert not ss, f'cannot parse {known_combinators[s[:i]]}'
    else:
        e = Constant(s[:i])
    return e, s[i:]


def parse_one(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
    match s[0]:
        case 'λ':
            return parse_lambda(s, ctx)
        case '(':
            return parse_paren(s, ctx)
        case c if c in variable_names:
            res = []
            while s and s[0] in variable_names:
                if s[0] in ctx:
                    res.append(ctx[s[0]])
                    s = s[1:]
                else:
                    raise Exception(f'unknown variable {s[0]} in {ctx}')
            if len(res) == 1:
                return res[0], s
            else:
                return apply(res), s
        case c if c.isalpha():
            return get_constant(s)
        case _:
            raise Exception(f'cannot parse {s}')


def apply(li: List[Obj]) -> Obj:
    if len(li) < 2:
        res = li[0] if li else Empty()
    else:
        res = Application(li).beta()
    if args.tracing:
        print(f'apply {' '.join([str(e) for e in li])} -> {str(res)}')
    return res


def parse_top(s: str, ctx: Dict[str, Var] = {}) -> Tuple[Obj, str]:
    s = s.strip()
    res: List[Obj] = []
    while s:
        e, s = parse_one(s, ctx)
        res.append(e)
        s = s.strip()
    return apply(res), s


def evalstr(s: str) -> Obj:
    expr, ss = parse_top(s)
    if ss:
        raise Exception(f'cannot parse {s}: left over {ss}')
    return expr


def handle(al: List[str]) -> int:
    ec = 0
    for a in al:
        print('\u2501' * 48 + '\n' + a)
        try:
            expr = evalstr(a)
            print(f'⇒ {expr.fmt(Naming())}')
        except Exception as e:
            print(f'eval("{a}") failed: {e.args[0]}')
            ec = 1
    return ec


def check() -> int:
    checks = [
        ['K', 'K'],
        ['S K K', 'I'],
        ['K I', 'π'],
        ['K (S K K)', 'π'],
        ['B B', 'D'],
        ['B D', 'D₁'],
        ['B (B B)', 'D₁'],
        ['D D', 'D₂'],
        ['B B D', 'D₂'],
        ['D (B B)', 'D₂'],
        ['B B (B B)', 'D₂'],
        ['B B₁', 'E'],
        ['B (D B)', 'E'],
        ['B (B B B)', 'E'],
        ['D B', 'B₁'],
        ['D B₁', 'B₂'],
        ['B D B', 'B₃'],
        # ['S(K S)K', 'B'],
        # ['B₁ S B', 'Φ'],
        # ['B (Φ B S) K K', 'Ψ'],
    ]
    ec = 0
    for c in checks:
        res = evalstr(c[0]).fmt(Naming())
        if res != c[1]:
            if c[1] in known_combinators:
                print(f'❌ {c[0]} → {res} but {c[1]} = {known_combinators[c[1]]} expected')
            else:
                print(f'❌ {c[0]} → {res} but {c[1]} expected')
            ec = 1
        else:
            print(f'✅ {c[0]} → {res}')
    return ec


def main(al: List[str]) -> None:
    if args.check:
        # Overwrite eventual user setting
        args.tracing = False
        ec = check()
    else:
        ec = handle(al)
    exit(ec)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--tracing', dest='tracing', action='store_true')
    parser.add_argument('--check', dest='check', action='store_true')
    parser.add_argument('expression', metavar='expression', type=str, nargs='*')
    args = parser.parse_args()
    main(args.expression)
