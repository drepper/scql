"""Toplevel REPL for the query/compute language mockup."""
import compute
import python_syntax_ext


def evaluate(line:str):
  std = python_syntax_ext.to_standard_python(line, compute.get_cn())
  return compute.compute(std)


def main():
  line = ''
  while True:
    try:
      line = input(f'\033[33m[{compute.get_cn()}] >\033[0m ')
    except EOFError:
      print('quit')
      line = 'quit'
    if line == 'quit':
      break
    res = evaluate(line)
    if res is not None:
      print(res)


if __name__ == '__main__':
  main()
