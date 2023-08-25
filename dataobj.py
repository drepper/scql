"""Implement operations of the language on top of standard Python."""
import numpy as np


__all__ = ['Negative', 'Abs', 'Sqrt', 'Square', 'Exp', 'Log', 'Sin', 'Cos', 'Tan', 'Max', 'Min', 'Add', 'Subtract', 'Multiply', 'Identity', 'Zeros', 'Ones', 'known_generator_p']


class Op: # pylint: disable=too-few-public-methods
  def __init__(self):
    pass

class UnaryOp(Op): # pylint: disable=too-few-public-methods
  def __init__(self, func):
    self.func = func
  def apply(self, obj:'Data') -> 'Data':
    return self.func(obj.data)

class Negative(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.negative)

class Abs(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.abs)

class Sqrt(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.sqrt)

class Square(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.square)

class Exp(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.exp)

class Log(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.log)

class Sin(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.sin)

class Cos(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.cos)

class Tan(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self):
    super().__init__(np.tan)

class Max(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, axis=None):
    super().__init__(lambda a: np.max(a, axis=axis))

class Min(UnaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, axis=None):
    super().__init__(lambda a: np.min(a, axis=axis))

class BinaryOp(Op): # pylint: disable=too-few-public-methods
  def __init__(self, func:np.ufunc, robj:'Data', atright:bool):
    self.func = func
    self.atright = atright
    match robj:
      case int() | float():
        self.robj = Data(robj)
      case _:
        self.robj = robj
  def apply(self, obj:'Data') -> 'Data':
    if self.atright:
      return self.func(obj.data, self.robj if isinstance(self.robj, str) else self.robj.get_value())
    return self.func(self.robj if isinstance(self.robj, str) else self.robj.get_value(), obj.data)

class Add(BinaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, robj:'Data'):
    super().__init__(np.add, robj, True)

class Subtract(BinaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, robj:'Data', atright:bool=True):
    super().__init__(np.subtract, robj, atright)

class Multiply(BinaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, robj:'Data'):
    super().__init__(np.multiply, robj, True)

class Cross(BinaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, robj:'Data', atright:bool=True):
    super().__init__(np.cross, robj, atright)

class Matmul(BinaryOp): # pylint: disable=too-few-public-methods
  def __init__(self, robj:'Data', atright:bool=True):
    super().__init__(np.matmul, robj, atright)


class Data:
  def __init__(self, nums):
    self.data = np.array(nums)
  def sequence(self, *ops:Op) -> 'Data':
    res = self.get_value()
    for op in ops:
      res = op.apply(res)
    return Data(res)
  def get_value(self):
    return self.data
  def __str__(self):
    return str(self.get_value())

class Identity(Data):
  def __init__(self, n:int, dtype=float): # pylint: disable=super-init-not-called
    self.data = np.identity(n, dtype)

class Zeros(Data):
  def __init__(self, n:[int,tuple[int,int]], dtype=float): # pylint: disable=super-init-not-called
    self.data = np.zeros(n, dtype)

class Ones(Data):
  def __init__(self, n:[int,tuple[int,int]], dtype=float): # pylint: disable=super-init-not-called
    self.data = np.ones(n, dtype)


def known_generator_p(ident:str) -> bool:
  return ident in ['Data', 'Identity', 'Zeros' ,'Ones']


if __name__ == '__main__':
  a = Data([[1,2,3],[2,3,4],[3,4,5]])
  plus1 = Add(Data(1))
  r = a.sequence(plus1)
  e = np.array([[2, 3, 4], [3, 4, 5], [4, 5, 6]])
  assert np.array_equal(r.data, e)
  print('OK')
