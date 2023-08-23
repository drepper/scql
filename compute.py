from dataobj import Negative, Add, Subtract, Multiply, Identity, Zero
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

_context = None


def explode_name(name:str) -> (str,str):
  expl = name.split('::')
  return expl[-1], "::".join(expl[:-1])


def read_table(name:str, ns:str):
  # Check whether the table is available and up-to-date
  _context.add(name, ns)
  obj = storage.get(name, ns)
  if not obj.valid_p():
    code = obj.get_code()
    _context.recurse(ns + '::' + name if ns else name, code)
    res = eval(code)
    deps = _context.finish()
    obj = storage.store(name, ns, res, code, deps)
  return obj.get_value()


def write_table(name:str, ns:str, value):
  return storage.store(name, ns, value, _context.current())


def compute(expr:str):
  global _context
  _context = Compute('#top#', expr)
  res = eval(expr)
  _context = None
  return res
