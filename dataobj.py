import numpy as np


__all__ = ['Negative', 'Add', 'Subtract', 'Multiply', 'Identity', 'Zeros', 'Ones', 'known_generator_p']


class Op:
  def __init__(self):
    pass

class UnaryOp(Op):
  def __init__(self, func:np.ufunc):
    self.func = func
  def apply(self, obj:'Data') -> 'Data':
    return self.func(obj.data)

class Negative(UnaryOp):
  def __init__(self):
    super().__init__(np.negative)

class BinaryOp(Op):
  def __init__(self, func:np.ufunc, robj:'Data'):
    self.func = func
    match robj:
      case int() | float():
        self.robj = Data(robj)
      case _:
        self.robj = robj
  def apply(self, obj:'Data') -> 'Data':
    return self.func(obj.data, self.robj if type(self.robj) == str else self.robj.data)

class Add(BinaryOp):
  def __init__(self, robj:'Data'):
    super().__init__(np.add, robj)

class Subtract(BinaryOp):
  def __init__(self, robj:'Data'):
    super().__init__(np.subtract, robj)

class Multiply(BinaryOp):
  def __init__(self, robj:'Data'):
    super().__init__(np.multiply, robj)


class Data:
  def __init__(self, nums):
    self.data = np.array(nums)
  def sequence(self, *ops:Op) -> 'Data':
    res = self.data
    for op in ops:
      res = op.apply(res)
    return Data(res)
  def __str__(self):
    return str(self.data)

class Identity(Data):
  def __init__(self, n:int, dtype=float):
    self.data = np.identity(n, dtype)

class Zeros(Data):
  def __init__(self, n:[int,tuple[int,int]], dtype=float):
    self.data = np.zeros(n, dtype)

class Ones(Data):
  def __init__(self, n:[int,tuple[int,int]], dtype=float):
    self.data = np.ones(n, dtype)


def known_generator_p(ident:str) -> bool:
  return ident in ['Identity', 'Zeros' ,'Ones']


if __name__ == '__main__':
  a = Data([[1,2,3],[2,3,4],[3,4,5]])
  plus1 = Add(Data(1))
  r = a.sequence([plus1])
  e = np.array([[2, 3, 4], [3, 4, 5], [4, 5, 6]])
  assert np.array_equal(r.data, e)
  print('OK')
