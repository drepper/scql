"""Mockup of a data storage system."""

__all__ = ['get', 'store', 'Cell']


_TICK:int = 0


class Cell: # pylint: disable=too-many-instance-attributes
  """Data structure for a storage object.  The type of the object is unknown.  Record a list
  of dependencies, keep a reference counter, and a list of data objects which depend on this
  object."""
  def __init__(self, name:str, ns:str, obj, code:[None,str], deps:list[tuple[str,str]]):
    global _TICK # pylint: disable=global-statement
    self.name = name
    self.ns = ns
    self.value = obj
    if not code and deps:
      raise ValueError('cannot create a cell value with dependencies but without code')
    self.code = code
    self.deps = deps
    self.reqs = []
    self.refcnt = 1
    self.permanent = False
    self.ts = _TICK
    _TICK += 1
    for dep in deps:
      _store_dep(*dep, (self.name, ns))
  def get_value(self):
    """Return the value stored in this data object."""
    return self.value
  def valid_p(self) -> bool:
    """Check whether any of the data objects this object depends of has changed."""
    for depnam in self.deps:
      try:
        if _get_ts(*depnam) >= self.ts:
          return False
      except LookupError as exc:
        raise AssertionError("dependency not present") from exc
    return True
  def recomputable_p(self) -> bool:
    """Return true if the value can be recomputed."""
    return self.code != None
  def get_code(self) -> str:
    """Return the code used."""
    if self.code:
      return self.code
    raise RuntimeError("cannot recompute cell value")
  def get(self, addref:bool) -> 'Cell':
    """Return a reference to the this object."""
    if addref:
      self.refcnt += 1
    return self
  def get_ts(self) -> int:
    """Get timestamp of the object."""
    return self.ts
  def store_dep(self, source:tuple[str, str]) -> None:
    """Add referrer to this object to the reqs list."""
    self.reqs.append(source)
  def make_permanent(self) -> None:
    """Mark object as non-deletable."""
    self.permanent = True
  def release(self) -> None:
    """Release the object if allowed and it is not referenced anymore."""
    assert self.refcnt > 0
    self.refcnt -= 1
    if not self.permanent and self.refcnt == 0 and len(self.reqs) == 0:
      for dep in self.deps:
        get(*dep, False).release2(self.name, self.ns)
      _remove(self.name, self.ns)
  def release2(self, name:str, ns:str) -> None:
    assert (name,ns) in self.reqs
    self.reqs = [req for req in self.reqs if req != (name,ns)]
  def __str__(self):
    # return f'{self.ns + "::" + self.name} : value={self.value}, deps={self.deps}, reqs={self.reqs}, refcnt={self.refcnt}, ts={self.ts}'
    return str(self.value)


class Namespace:
  """Wrapper around a dictionary to implement a namespace with Cell type objects."""
  def __init__(self, name:str):
    self.name = name
    self.objs: dict[str, Cell] = {}
  def store(self, name:str, obj, code:[None,str], deps:list[str]) -> Cell:
    """Store given object of undetermined type in the namespace, overwriting possible
    older values."""
    self.objs[name] = (res := Cell(name, self.name, obj, code, deps))
    return res
  def get(self, name:str, addref:bool) -> Cell:
    """To get reference to the data object call the appropriate object's get function."""
    return self.objs[name].get(addref)
  def get_ts(self, name:str) -> int:
    """Get timestamp of an object by calling its get_ts function."""
    return self.objs[name].get_ts()
  def store_dep(self, rname:str, source:tuple[str,str]) -> None:
    """Add referrer to data object by calling its store_dep function."""
    self.objs[rname].store_dep(source)
  def remove(self, name:str) -> None:
    assert name in self.objs
    del self.objs[name]


class Storage:
  """Mockup implementation of a storage class with data objects in individual namespaces."""
  def __init__(self):
    self.all: dict[str, Namespace] = {}
  def has(self, name:str) -> bool:
    """Check whether namespace with given name exists"""
    return name in self.all
  def create(self, name:str) -> None:
    """Create new empty namespace with given name."""
    self.all[name] = Namespace(name)
  def store(self, name:str, ns:[str,Namespace], obj, code:[None,str], deps:list[tuple[str,str]]) -> Cell:
    """Store data object with given full name and its dependency list."""
    match ns:
      case str(_):
        if not self.has(ns):
          self.create(ns)
        return self.all[ns].store(name, obj, code, deps)
      case Namespace(_):
        return ns.store(name, obj, code, deps)
      case _:
        raise TypeError(f"invalid namespace of type {type(ns)}")
  def get(self, name:str, ns, addref:bool) -> Cell:
    """Get reference to the named object by calling the get function of the appropriate namespace."""
    match ns:
      case str(_):
        return self.all[ns].get(name, addref)
      case Namespace(_):
        return ns.get(name, addref)
      case _:
        raise TypeError(f"invalid namespace of type {type(ns)}")
  def get_ts(self, name:str, ns:str) -> int:
    """Get a data objects timestamp by calling the namespace's get_ts function."""
    return self.all[ns].get_ts(name)
  def store_dep(self, rname:str, rns:str, source:tuple[str,str]) -> None:
    """Add referrer to a data object by calling the namespace's store_dep function."""
    self.all[rns].store_dep(rname, source)
  def remove(self, name:str, ns:str) -> None:
    self.all[ns].remove(name)


_workspace = Storage()


def store(name:str, ns, obj, code:[None,str], deps:list[tuple[str,str]] = []) -> Cell: # pylint: disable=dangerous-default-value
  """Store data object with the given name."""
  return _workspace.store(name, ns, obj, code, deps)


def get(name:str, ns:[str,Namespace], addref:bool = True) -> Cell:
  """Get reference to data object with the given name."""
  return _workspace.get(name, ns, addref)


def _get_ts(name:str, ns:str) -> int:
  return _workspace.get_ts(name, ns)


def _store_dep(rname:str, rns:str, source:tuple[str,str]) -> None:
  _workspace.store_dep(rname, rns, source)


def _remove(name:str, ns:str) -> None:
  _workspace.remove(name, ns)


if __name__ == '__main__':
  o1 = store('obj1','ns1', (1,2,3), None)
  assert len(_workspace.all['ns1'].objs) == 1
  o2 = store('obj2','ns1', (2,3,4), "something()", [('obj1','ns1')])
  assert len(_workspace.all['ns1'].objs) == 2
  o2.release()
  assert len(_workspace.all['ns1'].objs) == 1
  assert o1.get_value() == (1,2,3)
  print('OK')

