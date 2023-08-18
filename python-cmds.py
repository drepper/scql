import ast
import uuid

class idmap:
  def __init__(self):
    self.map = dict()
    self.rmap = dict()
  def get(self, s:str):
    if len(s) == 0:
      raise RuntimeError(f'invalid empty ID')
    if not s in self.map:
      n = f'T{uuid.uuid4().hex}'
      self.map[s] = n
      self.rmap[n] = s
    return self.map[s]
  def has(self, s:str):
    return s in self.rmap
  def rget(self, s:str):
    return self.rmap[s]
  def __iter__(self):
    return self.map.__iter__()

class Rewrite(ast.NodeTransformer):
  def __init__(self, map:idmap):
    self.map = map
  def visit_Name(self, node):
    match node:
      case ast.Name(id, ast.Load()) if self.map.has(id):
        return ast.Call(ast.Name('ReadTable', ast.Load()), [ ast.Name(self.map.rget(id), ast.Load()) ], [])
      case _:
        return node
  def visit_Assign(self, node):
    match node:
      case ast.Assign([ ast.Name(id, ast.Store()) ], ast.BinOp(left, ast.BitOr(), right)) if self.map.has(id):
        return ast.Call(ast.Name('WriteTable', ast.Load()), [ ast.Name(self.map.rget(id), ast.Load()), self.getSequence(left, right) ], [])
      case ast.Assign([ ast.Name(id, ast.Store()) ], value) if self.map.has(id):
        return ast.Call(ast.Name('WriteTable', ast.Load()), [ ast.Name(self.map.rget(id), ast.Load()), self.visit(value) ], [])
      case _:
        return node
  def getSequence(self, t:ast.AST, right:ast.AST):
    head, args = self.getSequenceRec(t)
    args.append(self.visit(right))
    return ast.Call(ast.Attribute(head, 'sequence', ast.Load()), args, [])
  def getSequenceRec(self, t:ast.AST):
    match t:
      case ast.BinOp(left, ast.BitOr(), right):
        head, args = self.getSequenceRec(left)
        args.append(self.visit(right))
        return (head, args)
      case _:
        return (self.visit(t), [])

def parse(s:str):
  map = idmap()
  while True:
    try:
      return ast.fix_missing_locations(Rewrite(map).visit(ast.parse(s)))
    except SyntaxError as e:
      if e.args[0] == 'invalid syntax':
        filename,lineno,offset,text,end_lineno,end_offset = e.args[1]
        lines = text.splitlines()
        line = lines[lineno-1]
        if lineno == end_lineno and offset + 1 <= end_offset and line[offset-1:end_offset-1] == '$' and line[offset].isalpha():
          o = offset + 1
          while o < len(line) and line[o].isalnum():
            o += 1
          r = map.get(line[offset:o])
          nline = line[:offset-1] + r + line[o:]
          lines[lineno-1] = nline
          s = '\n'.join(lines)
          continue
      raise e


if __name__ == '__main__':
  import sys
  input = sys.argv[1] if len(sys.argv) > 1 and len(sys.argv[1]) > 0 else '$a=$a+1+$b+f(a)'
  try:
    t = parse(input)
    if t:
      print(ast.dump(t, indent='  '))
      print(f'{input} -> {ast.unparse(t)}')
  except SyntaxError as e:
    print(e)
