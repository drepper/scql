"""Mockup of a data storage system."""

__all__ = ['get', 'store', 'Cell']


_TICK = 0


class Cell: # pylint: disable=too-many-instance-attributes
  """Data structure for a storage object.  The type of the object is unknown.  Record a list
  of dependencies, keep a reference counter, and a list of data objects which depend on this
  object."""
  def __init__(self, name:str, ns:str, obj, deps:list[tuple[str,str]]):
    global _TICK # pylint: disable=global-statement
    self.name = name
    self.ns = ns
    self.value = obj
    self.deps = deps
    self.reqs = []
    self.refcnt = 1
    self.permanent = False
    self.ts = _TICK
    _TICK += 1
    for dep in deps:
      _store_dep(*dep, (self.name, ns))
  def valid_p(self):
    """Check whether any of the data objects this object depends of has changed."""
    for depnam in self.deps:
      try:
        if _get_ts(*depnam) >= self.ts:
          return False
      except LookupError as exc:
        raise AssertionError("dependency not present") from exc
    return True
  def get(self, addref:bool):
    """Return a reference to the this object unless any of its dependencies changed."""
    if self.valid_p():
      if addref:
        self.refcnt += 1
      return self
    return None
  def get_ts(self):
    """Get timestamp of the object."""
    return self.ts
  def store_dep(self, source:tuple[str, str]):
    """Add referrer to this object to the reqs list."""
    self.reqs.append(source)
  def make_permanent(self):
    """Mark object as non-deletable."""
    self.permanent = True
  def release(self):
    """Release the object if allowed and it is not referenced anymore."""
    assert self.refcnt > 0
    self.refcnt -= 1
    if not self.permanent and self.refcnt == 0 and len(self.reqs) == 0:
      for dep in self.deps:
        get(*dep, False)._release(self.name, self.ns) # pylint: disable=protected-access
      _remove(self.name, self.ns)
  def _release(self, name:str, ns:str):
    assert (name,ns) in self.reqs
    self.reqs = [req for req in self.reqs if req != (name,ns)]
  def __str__(self):
    return f'{self.ns + "::" + self.name} : value={self.value}, deps={self.deps}, reqs={self.reqs}, refcnt={self.refcnt}, ts={self.ts}'


class Namespace:
  """Wrapper around a dictionary to implement a namespace with Cell type objects."""
  objs: dict[str, Cell] = {}
  def __init__(self, name:str):
    self.name = name
  def lookup(self, name):
    """Return named Cell objects."""
    return self.objs[name]
  def store(self, name:str, obj, deps:list[str]):
    """Store given object of undetermined type in the namespace, overwriting possible
    older values."""
    self.objs[name] = (res := Cell(name, self.name, obj, deps))
    return res
  def get(self, name:str, addref:bool):
    """To get reference to the data object call the appropriate object's get function."""
    return self.objs[name].get(addref)
  def get_ts(self, name:str):
    """Get timestamp of an object by calling its get_ts function."""
    return self.objs[name].get_ts()
  def store_dep(self, rname:str, source:tuple[str,str]):
    """Add referrer to data object by calling its store_dep function."""
    self.objs[rname].store_dep(source)
  def _remove(self, name:str):
    assert name in self.objs
    del self.objs[name]


class Storage:
  """Mockup implementation of a storage class with data objects in individual namespaces."""
  all: dict[str, Namespace] = {}
  def __init__(self):
    pass
  def lookup(self, name:str):
    """Return reference to named namespace."""
    return self.all[name]
  def has(self, name:str):
    """Check whether namespace with given name exists"""
    return name in self.all
  def create(self, name:str):
    """Create new empty namespace with given name."""
    self.all[name] = Namespace(name)
  def store(self, name:str, ns, obj, deps:list[tuple[str,str]]):
    """Store data object with given full name and its dependency list."""
    match ns:
      case str(_):
        if not self.has(ns):
          self.create(ns)
        return self.all[ns].store(name, obj, deps)
      case Namespace(_):
        return ns.store(name, obj, deps)
      case _:
        raise TypeError(f"invalid namespace of type {type(ns)}")
  def get(self, name:str, ns, addref:bool):
    """Get reference to the named object by calling the get function of the appropriate namespace."""
    match ns:
      case str(_):
        return self.all[ns].get(name, addref)
      case Namespace(_):
        return ns.get(name, addref)
      case _:
        raise TypeError(f"invalid namespace of type {type(ns)}")
  def get_ts(self, name:str, ns:str):
    """Get a data objects timestamp by calling the namespace's get_ts function."""
    return self.all[ns].get_ts(name)
  def store_dep(self, rname:str, rns:str, source:tuple[str,str]):
    """Add referrer to a data object by calling the namespace's store_dep function."""
    self.all[rns].store_dep(rname, source)
  def _remove(self, name:str, ns:str):
    self.all[ns]._remove(name) # pylint: disable=protected-access


_workspace = Storage()


def store(name:str, ns, obj, deps:list[tuple[str,str]] = []): # pylint: disable=dangerous-default-value
  """Store data object with the given name."""
  return _workspace.store(name, ns, obj, deps)


def get(name:str, ns, addref:bool = True):
  """Get reference to data object with the given name."""
  return _workspace.get(name, ns, addref)


def _get_ts(name:str, ns:str):
  return _workspace.get_ts(name, ns)


def _store_dep(rname:str, rns:str, source:tuple[str,str]):
  return _workspace.store_dep(rname, rns, source)

def _remove(name:str, ns:str):
  return _workspace._remove(name, ns) # pylint: disable=protected-access
