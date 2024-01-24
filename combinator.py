#! /usr/bin/env python3
# Copyright © 2024 Ulrich Drepper <drepper@akkadia.org>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""Evaluation and reduction of lambda expression."""
from __future__ import annotations

import argparse
import functools
import os
import sys
from typing import cast, ClassVar, Dict, List, override, Tuple


# These are the characters accepted and used as variable names.  The list
# could be extended here and everything else should just work.  But it
# should be noted that
# a) uppercase characters are used in the names of known combinators
# b) some greek letters (both lowercase like λ and π and uppercase like
#    Φ and Ψ
# are used for non-variables and conflicts would be fatal.
VARIABLE_NAMES = 'abcdefghijklmnopqrstuvwxyz'


class Naming: # pylint: disable=too-few-public-methods
  """This is an object encapsulating the predictable generation of distinct
  variable names."""
  def __init__(self):
    self.next = VARIABLE_NAMES[0]
    self.known = {}

  def get(self, v: Var) -> str:
    """Get the next variable name."""
    if v.id not in self.known:
      assert self.next in VARIABLE_NAMES, 'too many variables'
      self.known[v.id] = self.next
      try:
        self.next = VARIABLE_NAMES[VARIABLE_NAMES.index(self.next) + 1]
      except IndexError:
        # set to an invalid value
        self.next = '_'
    return self.known[v.id]


# List of combinators, mostly taken from
#   https://www.angelfire.com/tx4/cus/combinator/birds.html
# These correspond to the Smullyan's "To Mock a Mockingbird".
# The list can be extended.  The names are used in translation from the
# input and when printing the result.
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
  """Return argument without enclosing parenthesis or the string itself."""
  return s[1:-1] if s and s[0] == '(' and s[-1] == ')' else s


class Obj:
  """Base class for node in the graph representation of a lambda expression."""
  def is_free(self, v: Var) -> bool: # pylint: disable=unused-argument
    """Test whether this is an object for a free variable.  This is the generic
    implementation."""
    return False

  def __str__(self) -> str:
    raise NotImplementedError('__str__ called for Obj')

  def fmt(self, varmap: Naming) -> str:
    """Format an expression as a string.  This pure virtual version must never
    be called."""
    raise NotImplementedError('fmt called for Obj')

  def replace(self, v: Var, expr: Obj) -> Obj: # pylint: disable=unused-argument
    """Return the expression with the given variable replaced by the expression."""
    return self

  def duplicate(self) -> Obj:
    """Duplicate the object."""
    return self


class Var(Obj):
  """Object to represent a variable in the lambda expression graph.  This implements
  the de Bruijn notation by representing each new variable with a unique number."""
  varcnt: ClassVar[int] = 1

  def __init__(self):
    self.id = Var.varcnt
    Var.varcnt += 1

  @override
  def is_free(self, v: Var) -> bool:
    """Test whether this is a free variable.  If we come here and this is the variable
    we are looking for it is indeed free."""
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
  """Object returned to indicate errors when parsing lambda expressions."""
  @override
  def __str__(self):
    return '{}'

  @override
  def fmt(self, varmap: Naming) -> str:
    # This is a filler.  An object of this type should never really be used.
    return '○'


class Constant(Obj):
  """Object to represent constants in the lambda expression graph."""
  def __init__(self, name: str):
    self.name = name

  @override
  def __str__(self):
    return f'{{const {self.name}}}'

  @override
  def fmt(self, varmap: Naming) -> str:
    return f'{self.name} '


class Application(Obj):
  """Object to represent the application (call) to a function in the lambda
  expression graph."""
  def __init__(self, ls: List[Obj]):
    assert len(ls) >= 2
    self.code = (ls[0].code + ls[1:]) if isinstance(ls[0], Application) else ls
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
    """Perform beta reduction on the given application.  This is called on a freshly
    created object but the reduction cannot be performed in the constructor because
    the result of the beta reduction can be something other than an application."""
    if not isinstance(self.code[0], Lambda):
      return self
    la = cast(Lambda, self.code[0])
    r = la.code.replace(la.params[0], self.code[1])
    if len(la.params) > 1:
      r = newlambda(la.params[1:], r)
    return apply([r] + self.code[2:])


class Lambda(Obj):
  """Object to represent a lambda expression in the lambda expression graph."""
  def __init__(self, params: List[Var], code: Obj):
    if not params:
      raise SyntaxError('lambda parameter list cannot be empty')
    if isinstance(code, Lambda):
      self.params = params + code.params
      self.code = code.code
    else:
      self.params = params
      self.code = code

  @override
  def is_free(self, v: Var) -> bool:
    return v not in self.params and self.code.is_free(v)

  @override
  def __str__(self):
    return f'{{lambda {' '.join([str(a) for a in self.params])}.{str(self.code)}}}'

  @override
  def fmt(self, varmap: Naming) -> str:
    # It is important to process the params first to ensure correct naming of parameter variables
    paramstr = ''.join([a.fmt(varmap) for a in self.params])
    return Lambda.known_name(f'(λ{paramstr}.{remove_braces(self.code.fmt(varmap))})')

  @override
  def replace(self, v: Var, expr: Obj) -> Obj:
    return newlambda(self.params, self.code.replace(v, expr))

  @override
  def duplicate(self) -> Obj:
    newparams = [Var() for _ in self.params]
    newcode = functools.reduce(lambda p, o: p.replace(*o), zip(self.params, newparams), self.code)
    return newlambda(newparams, newcode)

  @staticmethod
  def known_name(la: str) -> str:
    """Determine whether the formatted expressions corresponds to a known combinator
    and return that name.  Otherwise return the original expression."""
    lar = remove_braces(la)
    try:
      la = next(k for k, v in KNOWN_COMBINATORS.items() if v == lar)
      if args.tracing: # pylint: disable=used-before-assignment
        print(f'→ {lar}')
    except StopIteration:
      pass
    return la


def parse_lambda(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  """Parse the representation of a lambda definition.  Return the graph
  representation and the remainder of the string not part of the just
  parsed part."""
  assert s[0] == 'λ'
  s = s[1:].strip()
  params: List[Var] = []
  recctx = ctx.copy()
  while s:
    if s[0] == '.':
      break
    if s[0] not in VARIABLE_NAMES:
      raise SyntaxError(f'invalid λ parameters {s[0]}')
    recctx[s[0]] = Var()
    params.append(recctx[s[0]])
    s = s[1:]
  # The following is basically the loop from parse_top but with a special
  # handling of whitespaces: they terminate the body.  This is nothing
  # mandatory from general lambda expression parsing point-of-view.  It is
  # just an expectation of people using the λ… notation.  A whitespace to
  # terminate a constant is ignored.  So, an expression using a whitespace
  # in a lambda body can simply use parenthesis.
  s = s[1:].strip()
  body: List[Obj] = []
  while s:
    if s[0].isspace():
      break
    e, s = parse_one(s, recctx)
    body.append(e)
  return newlambda(params, apply(body)), s


def parse_paren(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  """Parse an expression in parenthesis.  Return the graph
  representation and the remainder of the string not part of the just
  parsed part."""
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
  """Parse a constant name.  Unlike a variable, a constant can be longer than a
  single character and therefore has to be terminated by either of the special
  characters using the string representation (λ, (, ), or .) or a whitespace.
  Return the graph representation and the remainder of the string not part of
  the just parsed part."""
  i = 0
  while i < len(s) and s[i] != ')' and s[i] != '(' and s[i] != '.' and not s[i].isspace() and s[i] != 'λ':
    i += 1
  if s[:i] in KNOWN_COMBINATORS:
    e, ss = parse_top(KNOWN_COMBINATORS[s[:i]], {})
    if ss:
      raise SyntaxError(f'cannot parse {KNOWN_COMBINATORS[s[:i]]}')
  else:
    e = Constant(s[:i])
  return e, s[i:].lstrip()


def parse_one(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  """Toplevel function to parse a string representation of a lambda expression.
  Return the graph representation and the remainder of the string not part of
  the just parsed part."""
  match s[0]:
    case 'λ':
      return parse_lambda(s, ctx)
    case '(':
      return parse_paren(s, ctx)
    case c if c in VARIABLE_NAMES:
      return ctx[s[0]] if s[0] in ctx else Var(), s[1:]
    case c if c.isalpha():
      return get_constant(s)
    case _:
      raise SyntaxError(f'cannot parse "{s}"')


def newlambda(params: List[Var], code: Obj) -> Obj:
  """Create a new lambda expression using the given parametesr and body of code.
  But the function also performs η-reduction, i.e., it returns just the function
  expression (first of the application values) in case the resulting lambda would
  just apply the required parameter to the application value in order."""
  if isinstance(code, Application) and len(params) + 1 == len(code.code) and params == code.code[1:] and all(not code.code[0].is_free(e) for e in params):
    return code.code[0]
  return Lambda(params, code)


def apply(li: List[Obj]) -> Obj:
  """Create an application expression given the list of objects.  If only a
  singular expression is given it is returned, no need to wrap it into an
  application expression."""
  if len(li) < 2:
    res = li[0] if li else Empty()
  else:
    res = Application(li).beta()
  if args.tracing:
    print(f'apply {' '.join([str(e) for e in li])} -> {str(res)}')
  return res


def parse_top(s: str, ctx: Dict[str, Var]) -> Tuple[Obj, str]:
  """Parse a string to a lambda expression, taking one part at a time
  an creating an application expression from the parts.  Return the graph
  representation and the remainder of the string not part of the just
  parsed part."""
  s = s.strip()
  res: List[Obj] = []
  while s:
    e, s = parse_one(s, ctx)
    res.append(e)
    s = s.strip()
  return apply(res), s


def from_string(s: str) -> Obj:
  """Parse a string to a lambda expression, taking one part at a time
  an creating an application expression from the parts.  Return the graph
  representation.  In case not all of the expression is parsed raise a
  syntax error exception."""
  expr, ss = parse_top(s, {})
  if ss:
    raise SyntaxError(f'cannot parse {s}: left over {ss}')
  return expr


def to_string(expr: Obj) -> str:
  """Return a string representation for the lambda expression graph."""
  return remove_braces(expr.fmt(Naming())).rstrip()


def handle(al: List[str], echo: bool) -> int:
  """Loop over given list of strings, parse, simplify, and print the lambda
  expression."""
  ec = 0
  for a in al:
    if echo:
      print(a)
    try:
      print(f'⇒ {to_string(from_string(a))}')
    except SyntaxError as e:
      print(f'eval("{a}") failed: {e.args[0]}')
      ec = 1
    print('\u2501' * os.get_terminal_size()[0])
  return ec


def repl() -> int:
  """This is the REPL."""
  ec = 0
  try:
    while True:
      s = input('» ')
      if not s:
        break
      ec = ec | handle([s], False)
  except EOFError:
    print('')
  return ec


def check() -> int:
  """Sanity checks.  Return error code that is used as the exit code of the process."""
  checks = [
    ('K', 'K'),
    ('S K K', 'I'),
    ('K I', 'π'),
    ('K (S K K)', 'π'),
    ('B B', 'D'),
    ('B D', 'D₁'),
    ('B (B B)', 'D₁'),
    ('D D', 'D₂'),
    ('B B D', 'D₂'),
    ('D (B B)', 'D₂'),
    ('B B (B B)', 'D₂'),
    ('B B₁', 'E'),
    ('B (D B)', 'E'),
    ('B (B B B)', 'E'),
    ('D B', 'B₁'),
    ('D B₁', 'B₂'),
    ('B D B', 'B₃'),
    ('S(B B S)(K K)', 'C'),
    ('C I', 'T'),
    ('S(K S)K', 'B'),
    ('B₁ S B', 'Φ'),
    ('B Φ Φ', 'Φ₁'),
    ('B (Φ B S) K K', 'C'),
    ('B(S Φ C B)B', 'Ψ'),
    ('λx.NotX x', 'NotX'),
    ('B B C', 'G'),
    ('E T T E T', 'F'),
    ('B W (B C)', 'H'),
    ('B(B C)(W(B C(B(B B B))))', 'J'),
    ('C B M', 'L'),
    ('B M', 'M₂'),
    ('S I', 'O'),
    ('C B', 'Q'),
    ('B C B', 'Q₁'),
    ('C(B C B)', 'Q₂'),
    ('B T', 'Q₃'),
    ('B B T', 'R'),
    ('L O', 'U'),
    ('B C T', 'V'),
    ('C(B M R)', 'W'),
    ('C W', 'W₁'),
    ('S(S K)', 'I*'),
    ('B W', 'W*'),
    ('B C', 'C*'),
    ('B(B W)', 'W**'),
    ('B C*', 'C**'),
    ('B(B B B)(B(B B B))', 'Ê'),
    ('λabcd.MMM abcd', 'MMM'),
    ('λabcd.MMM abdc', 'λabcd.MMM abdc'),
    ('λabcd.MMM abc', 'λabcd.MMM abc'),
  ]
  ec = 0
  for testinput, expected in checks:
    res = to_string(from_string(testinput))
    if res != expected:
      if expected in KNOWN_COMBINATORS:
        print(f'❌ {testinput} → {res} but {expected} = {KNOWN_COMBINATORS[expected]} expected')
      else:
        print(f'❌ {testinput} → {res} but {expected} expected')
      ec = 1
    else:
      print(f'✅ {testinput} → {res}')
  return ec


def main(al: List[str]) -> None:
  """Called as main function of the program."""
  if args.check:
    # Overwrite eventual user setting
    args.tracing = False
    ec = check()
  elif al:
    ec = handle(al, True)
  else:
    ec = repl()
  sys.exit(ec)


if __name__ == '__main__':
  parser = argparse.ArgumentParser()
  parser.add_argument('-t', '--tracing', dest='tracing', action='store_true')
  parser.add_argument('--check', dest='check', action='store_true')
  parser.add_argument('expression', metavar='expression', type=str, nargs='*')
  args = parser.parse_args()
  main(args.expression)
