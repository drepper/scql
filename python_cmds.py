"""Demonstration of extending the Python syntax to provide easy references to
special object types, e.g., named data objects in a namespace."""

import ast
import uuid

class Idmap:
  """Simple class to create and provide access to randomly generated aliases for input names."""
  def __init__(self):
    self.idmap = {}
    self.rmap = {}
  def get(self, name:str):
    """Get existing or new random alias for a given name."""
    if len(name) == 0:
      raise RuntimeError('invalid empty ID')
    if not name in self.idmap:
      rand = f'T{uuid.uuid4().hex}'
      self.idmap[name] = rand
      self.rmap[rand] = name
    return self.idmap[name]
  def has(self, name:str):
    """Check whether given alias is known."""
    return name in self.rmap
  def rget(self, name:str):
    """Get original name for given alias."""
    return self.rmap[name]
  def __iter__(self):
    return self.idmap.__iter__()

class Rewrite(ast.NodeTransformer):
  """Extension of the standard library NodeTransformer class which replaces read references to the aliases identifiers
  with calls to ReadTable and write references with calls to WriteTable."""
  def __init__(self, idmap:Idmap):
    self.idmap = idmap
  def visit_Name(self, node): # pylint: disable=invalid-name ; this is an overloaded function
    """Called for each ast.Name instance.  Detect alias uses."""
    match node:
      case ast.Name(ident, ast.Load()) if self.idmap.has(ident):
        return ast.Call(ast.Name('ReadTable', ast.Load()), self.format_ident(ident), [])
      case _:
        return ast.NodeTransformer.generic_visit(self, node)
  def visit_Assign(self, node): # pylint: disable=invalid-name ; this is an overloaded function
    """Called for each ast.Assign instance.  Detect assignment to an aliased identifier.  Recognize sequences of binary OR
    expressions starting with a data object as a pipeline of computations."""
    match node:
      case ast.Assign([ ast.Name(ident, ast.Store()) ], ast.BinOp(left, ast.BitOr(), right)) if self.idmap.has(ident) and self.head_data_object(left):
        return ast.Call(ast.Name('WriteTable', ast.Load()), self.format_ident(ident) + [ self.get_sequence(left, right) ], [])
      case ast.Assign([ ast.Name(ident, ast.Store()) ], value) if self.idmap.has(ident):
        return ast.Call(ast.Name('WriteTable', ast.Load()), self.format_ident(ident) + [ self.visit(value) ], [])
      case _:
        return ast.NodeTransformer.generic_visit(self, node)
  def visit_Expr(self, node): # pylint: disable=invalid-name ; this is an overloaded function
    """Called for each ast.Expr instance.  Recognize sequences of binary OR
    expressions starting with a data object as a pipeline of computations."""
    match node:
      case ast.Expr(ast.BinOp(left, ast.BitOr(), right)) if self.head_data_object(left):
        return self.get_sequence(left, right)
      case _:
        return ast.NodeTransformer.generic_visit(self, node)
  def visit_Return(self, node): # pylint: disable=invalid-name ; this is an overloaded function
    """Called for each ast.Return instance.  Recognize sequences of binary OR
    expressions starting with a data object as a pipeline of computations."""
    match node:
      case ast.Return(ast.BinOp(left, ast.BitOr(), right)) if self.head_data_object(left):
        return self.get_sequence(left, right)
      case _:
        return ast.NodeTransformer.generic_visit(self, node)
  def head_data_object(self, tree:ast.AST):
    """We only transform BitOr sequences to data object sequences if the first expression in the
    sequence is a data object."""
    match tree:
      case ast.Name(ident, ast.Load()):
        return self.idmap.has(ident)
      case ast.BinOp(left, ast.BitOr(), _):
        return self.head_data_object(left)
      case _:
        return False
  def get_sequence(self, tree:ast.AST, right:ast.AST):
    """Recursion start to transform AST to create computation pipelines."""
    head, args = self.get_sequence_rec(tree)
    args.append(self.visit(right))
    return ast.Call(ast.Attribute(head, 'sequence', ast.Load()), args, [])
  def get_sequence_rec(self, tree:ast.AST):
    """Recursion to transform AST to create computation pipelines."""
    match tree:
      case ast.BinOp(left, ast.BitOr(), right):
        head, args = self.get_sequence_rec(left)
        args.append(self.visit(right))
        return (head, args)
      case _:
        return (self.visit(tree), [])
  def format_ident(self, alias:str):
    orig = self.idmap.rget(alias).split('::')
    return [ ast.Constant(orig[-1]), ast.Constant('::'.join(orig[:-1])) ]

def parse(source:str):
  """Custom parser for extended Python syntax to access external data objects and create copmute pipelines."""
  idmap = Idmap()
  while True:
    try:
      tree = ast.parse(source)
      # print(ast.dump(tree, indent='· '))
      return ast.fix_missing_locations(Rewrite(idmap).visit(tree))
    except SyntaxError as excp:
      if excp.args[0] == 'invalid syntax':
        _,lineno,offset,text,end_lineno,end_offset = excp.args[1]
        lines = text.splitlines()
        line = lines[lineno-1]
        if lineno == end_lineno and offset + 1 <= end_offset and line[offset-1:end_offset-1] == '$' and line[offset].isalpha():
          off = offset + 1
          while off < len(line):
            if line[off].isalnum():
              off += 1
            elif line[off] == ':' and off + 1 < len(line) and line[off+1] == ':':
              off += 2
            else:
              break;
          alias = idmap.get(line[offset:off])
          nline = line[:offset-1] + alias + line[off:]
          lines[lineno-1] = nline
          source = '\n'.join(lines)
          continue
      raise excp


if __name__ == '__main__':
  import sys
  INPUT = sys.argv[1] if len(sys.argv) > 1 and len(sys.argv[1]) > 0 else '$a=$a+1+$b+f(a)'
  try:
    t = parse(INPUT)
    if t:
      print(ast.dump(t, indent='  '))
      print(f'{INPUT} -> {ast.unparse(t)}')
  except SyntaxError as e:
    print(e)
