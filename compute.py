"""Evaluate extended Python expression including recomputation of
out-of-date data objects."""

from dataobj import Negative, Abs, Sqrt, Square, Exp, Log, Sin, Cos, Tan, Add, Subtract, Multiply, Identity, Zeros, Ones # pylint: disable=unused-import
import storage


class Compute:

  def __init__(self, topname, code):
    self.stack:list[tuple[str,str,list[(str,str)]]] = [(topname, code, [])]
  def recurse(self, name, code):
    self.stack.append((name, code, []))
  def add(self, name, ns):
    self.stack[-1][2].append((name, ns))
  def finish(self):
    return self.stack.pop(-1)[2]
  def current(self):
    return self.stack[-1][1]

_CONTEXT = None


def explode_name(name:str) -> (str,str):
  expl = name.split('::')
  return expl[-1], "::".join(expl[:-1])


def read_table(name:str, ns:str):
  """Check whether the table is available and up-to-date and return its value."""
  _CONTEXT.add(name, ns)
  obj = storage.get(name, ns)
  if not obj.valid_p():
    code = obj.get_code()
    _CONTEXT.recurse(ns + '::' + name if ns else name, code)
    res = eval(code) # pylint: disable=eval-used
    deps = _CONTEXT.finish()
    obj = storage.store(name, ns, res, code, deps)
  return obj.get_value()


def write_table(name:str, ns:str, value):
  return storage.store(name, ns, value, _CONTEXT.current())


def compute(expr:str):
  global _CONTEXT # pylint: disable=global-statement
  _CONTEXT = Compute('#top#', expr)
  res = eval(expr) # pylint: disable=eval-used
  _CONTEXT = None
  return res
