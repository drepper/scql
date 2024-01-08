"""Evaluation and reduction of lambda expression."""
from __future__ import annotations

import argparse
import enum
import sys
from typing import ClassVar, Dict, List, override, Tuple


def charcnt(s: str, c: str) -> int:
  return sum(sc == c for sc in s)


@enum.unique
class Type(enum.Enum):
  EMPTY = 0
  CONST = 1
  VAR = 2
  CALL = 3
  LAMBDA = 4


VARIABLE_NAMES = 'abcdefghijklmnopqrstuvwxyz'


class Naming: # pylint: disable=too-few-public-methods
  def __init__(self):
    self.next = VARIABLE_NAMES[0]
    self.known = {}

  def get(self, v: Var) -> str:
    if v.id not in self.known:
      assert self.next in VARIABLE_NAMES, 'too many variables'
      self.known[v.id] = self.next
      try:
        self.next = VARIABLE_NAMES[VARIABLE_NAMES.index(self.next) + 1]
      except IndexError:
        # set to an invalid value
        self.next = '_'
    return self.known[v.id]


KNOWN_COMBINATORS = {
  'B': 'λabc.a(bc)',
  'B₁': 'λabcd.a(bcd)',
  'B₂': 'λabcde.a(bcde)',
  'B₃': 'λabcd.a(b(cd))',
  'C': 'λabc.acb',
  'C*': 'λabcd.abdc',
  'C**': 'λabcde.abced',
  'D': 'λabcd.ab(cd)',
  'D₁': 'λabcde.abc(de)',
  'D₂': 'λabcde.a(bc)(de)',
  'E': 'λabcde.ab(cde)',
  'Ê': 'λabcdefg.a(bcd)(efg)',
  'F': 'λabc.cba',
  'G': 'λabcd.ad(bc)',
  'H': 'λabc.abcb',
  'I': 'λa.a',
  'I*': 'λab.ab',
  'J': 'λabcd.ab(adc)',
  'K': 'λab.a',
  'L': 'λab.a(bb)',
  'M': 'λa.aa',
  'M₂': 'λab.ab(ab)',
  'O': 'λab.b(ab)',
  'π': 'λab.b',
  'Φ': 'λabcd.a(bd)(cd)',
  'Φ₁': 'λabcde.a(bde)(cde)',
  'Ψ': 'λabcd.a(bc)(bd)',
  'Q': 'λabc.b(ac)',
  'Q₁': 'λabc.a(cb)',
  'Q₂': 'λabc.b(ca)',
  'Q₃': 'λabc.c(ab)',
  'R': 'λabc.bca',
  'S': 'λabc.ac(bc)',
  'T': 'λab.ba',
  'U': 'λab.b(aab)',
  'V': 'λabc.cab',
  'W': 'λab.abb',
  'W₁': 'λab.baa',
  'W*': 'λabc.abcc',
  'W**': 'λabcd.abcdd',
}


def remove_braces(s: str) -> str:
  return s[1:-1] if s and s[0] == '(' and s[-1] == ')' else s


def known_name(la: str) -> str:
  lar = remove_braces(la)
  for name, lstr in KNOWN_COMBINATORS.items():
    if lstr == lar:
      if args.tracing: # pylint: disable=used-before-assignment
        print(f'→ {lar}')
      return name
  return la


class Obj:
  def __init__(self, t: Type):
    self.t = t

  def is_a(self, t: Type) -> bool:
    return self.t == t

  def is_free(self, v: Var) -> bool: # pylint: disable=unused-argument
    return False

  def __str__(self) -> str:
    raise NotImplementedError('__str__ called for Obj')

  def fmt(self, varmap: Naming) -> str:
    raise NotImplementedError('fmt called for Obj')

  def replace(self, v: Var, expr: Obj) -> Obj: # pylint: disable=unused-argument
    return self

  def duplicate(self) -> Obj:
    return self


class Var(Obj):
  varcnt: ClassVar[int] = 1

  def __init__(self):
    super().__init__(Type.VAR)
    self.id = Var.varcnt
    Var.varcnt += 1

  @override
  def is_free(self, v: Var) -> bool:
    return self.id == v.id

  @override
  def __str__(self):
    return f'{{var {self.id}}}'

  @override
  def fmt(self, varmap: Naming) -> str:
    return varmap.get(self)

  @override
  def replace(self, v: Var, expr: Obj) -> Obj:
    return expr.duplicate() if v.id == self.id else self


class Empty(Obj):
  def __init__(self):
    super().__init__(Type.EMPTY)

  @override
  def __str__(self):
    return '{}'

  @override
  def fmt(self, varmap: Naming) -> str:
    return '○'


class Constant(Obj):
  def __init__(self, name: str):
    super().__init__(Type.CONST)
    self.name = name

  @override
  def __str__(self):
    return f'{{const {self.name}}}'

  @override
  def fmt(self, varmap: Naming) -> str:
    return f'{self.name} '


class Application(Obj):
  def __init__(self, ls: List[Obj]):
    super().__init__(Type.CALL)
    assert len(ls) >= 2
    self.code = (ls[0].code + ls[1:]) if ls and ls[0].is_a(Type.CALL) else ls
    assert self.code

  @override
  def is_free(self, v: Var) -> bool:
    return all(e.is_free(v) for e in self.code)

  @override
  def __str__(self):
    return f'{{App {' '.join([str(a) for a in self.code])}}}'

  @override
  def fmt(self, varmap: Naming) -> str:
    return f'({''.join([a.fmt(varmap) for a in self.code]).rstrip()})'

  @override
  def replace(self, v: Var, expr: Obj) -> Obj:
    return apply([e.replace(v, expr) for e in self.code])

  @override
  def duplicate(self) -> Obj:
    return Application([e.duplicate() for e in self.code])

  def beta(self) -> Obj:
    if not self.code[0].is_a(Type.LAMBDA):
      return self
    la: Lambda = self.code[0]
    r = la.code.replace(la.params[0], self.code[1])
    if len(la.params) > 1:
      r = newlambda(la.params[1:], la.ctx, r)
    return apply([r] + self.code[2:])


class Lambda(Obj):
  def __init__(self, params: List[Var], ctx: Dict[str, Var], code: Obj):
    super().__init__(Type.LAMBDA)
    if not params:
      raise SyntaxError('lambda parameter list cannot be empty')
    if code.is_a(Type.LAMBDA):
      self.params = params + code.params
      self.ctx = dict(ctx, **code.ctx)
      self.code = code.code
    else:
      self.params = params
      self.ctx = ctx
      self.code = code

  @override
  def is_free(self, v: Var) -> bool:
    return all(e.is_free(v) for e in self.code)

  @override
  def __str__(self):
    return f'{{lambda {' '.join([str(a) for a in self.params])}.{str(self.code)}}}'

  @override
  def fmt(self, varmap: Naming) -> str:
    # It is important to process the params first to ensure correct naming of parameter variables
    paramstr = ''.join([a.fmt(varmap) for a in self.params])
    return known_name(f'(λ{paramstr}.{remove_braces(self.code.fmt(varmap))})')

  @override
  def replace(self, v: Var, expr: Obj) -> Obj:
    return newlambda(self.params, self.ctx, self.code.replace(v, expr))

  @override
  def duplicate(self) -> Obj:
    newparams = [Var() for _ in self.params]
    newcode = self.code
    for o,n in zip(self.params, newparams):
      newcode = newcode.replace(o, n)
    return newlambda(newparams, self.ctx, newcode)


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
  return newlambda(params, recctx, body), s


def parse_paren(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  assert s[0] == '('
  start = 1
  end = 1
  depth = 0
  while True:
    if end == len(s):
      raise SyntaxError('incomplete parenthesis')
    if s[end] == ')':
      if depth == 0:
        res, ss = parse_top(s[start:end], ctx)
        if ss:
          raise SyntaxError(f'cannot parse {s[start:end]}')
        return res, s[end + 1:]
      depth -= 1
    if s[end] == '(':
      depth += 1
    end += 1


def get_constant(s: str) -> Tuple[Obj, str]:
  i = 0
  while i < len(s) and s[i] != ')' and s[i] != '(' and s[i] != '.' and not s[i].isspace() and s[i] != 'λ':
    i += 1
  if s[:i] in KNOWN_COMBINATORS:
    e, ss = parse_top(KNOWN_COMBINATORS[s[:i]], {})
    assert not ss, f'cannot parse {KNOWN_COMBINATORS[s[:i]]}'
  else:
    e = Constant(s[:i])
  return e, s[i:]


def parse_one(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  match s[0]:
    case 'λ':
      return parse_lambda(s, ctx)
    case '(':
      return parse_paren(s, ctx)
    case c if c in VARIABLE_NAMES:
      if s[0] in ctx:
        return ctx[s[0]], s[1:]
      raise SyntaxError(f'unknown variable {s[0]} in {ctx}')
    case c if c.isalpha():
      return get_constant(s)
    case _:
      raise SyntaxError(f'cannot parse {s}')


def newlambda(params: List[Var], ctx: Dict[str, Var], code: Obj):
  if code.is_a(Type.CALL) and len(params) + 1 == len(code.code) and params == code.code[1:] and all(not code.code[0].is_free(e) for e in params):
    return code.code[0]
  return Lambda(params, ctx, code)


def apply(li: List[Obj]) -> Obj:
  if len(li) < 2:
    res = li[0] if li else Empty()
  else:
    res = Application(li).beta()
  if args.tracing:
    print(f'apply {' '.join([str(e) for e in li])} -> {str(res)}')
  return res


def parse_top(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  s = s.strip()
  res: List[Obj] = []
  while s:
    e, s = parse_one(s, ctx)
    res.append(e)
    s = s.strip()
  return apply(res), s


def from_string(s: str) -> Obj:
  expr, ss = parse_top(s, {})
  if ss:
    raise SyntaxError(f'cannot parse {s}: left over {ss}')
  return expr


def to_string(expr: Obj) -> str:
  return remove_braces(expr.fmt(Naming())).rstrip()


def handle(al: List[str]) -> int:
  ec = 0
  for a in al:
    print('\u2501' * 48 + '\n' + a)
    try:
      expr = from_string(a)
      print(f'⇒ {to_string(expr)}')
    except SyntaxError as e:
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
    ['S(B B S)(K K)', 'C'],
    ['C I', 'T'],
    ['S(K S)K', 'B'],
    ['B₁ S B', 'Φ'],
    ['B Φ Φ', 'Φ₁'],
    ['B (Φ B S) K K', 'C'],
    ['B(S Φ C B)B', 'Ψ'],
    ['λx.NotX x', 'NotX'],
    ['B B C', 'G'],
    ['E T T E T', 'F'],
    ['B W (B C)', 'H'],
    ['B(B C)(W(B C(B(B B B))))', 'J'],
    ['C B M', 'L'],
    ['B M', 'M₂'],
    ['S I', 'O'],
    ['C B', 'Q'],
    ['B C B', 'Q₁'],
    ['C(B C B)', 'Q₂'],
    ['B T', 'Q₃'],
    ['B B T', 'R'],
    ['L O', 'U'],
    ['B C T', 'V'],
    ['C(B M R)', 'W'],
    ['C W', 'W₁'],
    ['S(S K)', 'I*'],
    ['B W', 'W*'],
    ['B C', 'C*'],
    ['B(B W)', 'W**'],
    ['B C*', 'C**'],
    ['B(B B B)(B(B B B))', 'Ê'],
    ['λabcd.MMM abcd', 'MMM'],
    ['λabcd.MMM abdc', 'λabcd.MMM abdc'],
  ]
  ec = 0
  for c in checks:
    res = to_string(from_string(c[0]))
    if res != c[1]:
      if c[1] in KNOWN_COMBINATORS:
        print(f'❌ {c[0]} → {res} but {c[1]} = {KNOWN_COMBINATORS[c[1]]} expected')
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
  sys.exit(ec)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-t', '--tracing', dest='tracing', action='store_true')
  parser.add_argument('--check', dest='check', action='store_true')
  parser.add_argument('expression', metavar='expression', type=str, nargs='*')
  args = parser.parse_args()
  main(args.expression)
