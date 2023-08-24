"""Toplevel REPL for the query/compute language mockup."""
import python_syntax_ext
import compute


def evaluate(line:str):
  std = python_syntax_ext.to_standard_python(line)
  return compute.compute(std)


def main():
  line = ''
  while True:
    try:
      line = input('> ')
    except EOFError:
      print('quit')
      line = 'quit'
    if line == 'quit':
      break
    print(evaluate(line))


if __name__ == '__main__':
  main()
