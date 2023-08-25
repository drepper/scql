Mock-up of the Semantic Cloud Query and Compute Language
========================================================

This is a mock-up of the proposed semantic cloud system.  It does not
have anything even close to the actual functionality, it is just meant
to provide an easy to understand, since ready to experiment with, approach
to experiment with an implementation.

The implementation is entirely in Python and the implemented query and
compute language is also an extension of Python.  This does not mean this
is mandatory for the actual implementation to follow.  Using Python as the
basis of the language is not a bad idea because of the wide use of the
language, especially in the data engineering and data science world.


The Language
------------

The implementation theoretically is a superset of the Python language.
The way the implementation currently works this is not true, though.
The implementation so far only allows entering and evaluating pure
expressions.  Assignments and statements (`for`, `if`, etc) are not
supported as are Python-style function and variable definitions.

The additions to the language consist of:

*   extension in the form of new identifiers with a `$` prefix
*   built-in data type for the representation of storable values
*   built-in operations to generate values of the new data type
*   built-in operations to work on values of the new data type
*   a special syntax to use a sequence of operations

### Identifiers

A syntax that is not used in normal Python is used for the new type of
identifiers: `$aa::bb:cc`.  The prefix `$` is invalid in Python and used in the
extended parser to start parsing the new identifier.  The object's name is `cc` and `aa::bb`
is its namespace (reminiscent of C++ symbols).


### Namespaces

To facilitate organizing globally visible data objects they can be organized into namespaces.
In a real implementation namespaces could have attributes like access permissions, retention settings,
location preferences, etc.

An identifier without a leading namespace (i.e., if
it contains no `::`) an implicit current namespace is used which is shown
as part of the CLI prompt.  This can
be changed in the mock-up implementation with `cn('new::ns')`.


### Storage

The new identifiers indicate objects in storage.  In the mock-up implementation
these are nothing other than entries in dictionary.  In a real implementation these will be files/blocks
in permanent storage.  The data items might be small (single numbers, short strings) and so
allocating separate files/blocks for each might not be the best approach.

The implementation should also support retaining subsequent versions and variants of data objects,
perhaps storing them as deltas.  There is currently no syntax to address either different versions
nor variants.  There is no way to indicate that older versions should be retained.

For some use cases co-location of compute and data will be important and replication of data might
be a useful data.  This might mean that writes need to be globally coordinated or some
form of locking needs to be implemented.


### New Data Types

The mock-up implementation defines a new data type `Cell` which allows to store arbitrary values
with the required meta information needed for the semantic cloud functionality.

The values on which the implementation is supposed to operate are of type `Data`.  This is so far
really only a thin wrapper around the Numpy array type.  Values can be created implicitly by assigning
to a storage object, by using `Data` as a constructor, or by using one of the generator functions
(`Identity`, `Zeros`, `Ones`).  The latter match the Numpy functions of similar names.



### `Data` Operations

To operate on values of type `Data` wrappers around Numpy functions are provided.  These are member
functions of Numpy's `array` type.  The semantic cloud implementation needs a functional representation
of the operations, though.  Therefore, the operations are represented as objects with appropriate
constructors.

Element-wise unary operations:
*   `Negative()`
*   `Abs()`
*   `Sqrt()`
*   `Square()`
*   `Exp()`
*   `Log()`
*   `Sin()`
*   `Cos()`
*   `Tan()`

Element-wise unary operations with parameter to specify the axis of operation (if any):
*   `Max(`*N*`)` with optional *N* which in this case is an integer from 0 to dimension-1 to compute
    the maximum value(s)
*   `Min(`*N*`)` similar for minimum values

Element-wise binary operations:
*   `Add(OBJ)` to add the data in `OBJ`
*   `Multiply(OBJ)` to multiply the data in `OBJ`

Element-wise binary operations with optional order:
*   `Subtract(OBJ`*, ATRIGHT*`)` to subtract `OBJ`.  `OBJ` is the right argument if `ATRIGHT` is missing
    or is `True`
*   `Cross(OBJ`*, ATRIGHT*`)` similar for the cross product
*   `Matmul(OBJ`*, ATRIGHT*`)` similar for the matrix multiplication


### Special Syntax

To facilitate the intend of writing compute pipelines a new syntax is created.  Following an object
of type `Data` with one or more groups of `|` and a `Data` operation creates a pipeline.  The
implementation could at this point look at the shape of the `Data` object and the properties and
parameters of the operations and see how the desired action should be best executed.  The mock-up
implementation tries to do nothing like that, it simply uses the respective Numpy operations one
after the other.

An example looks like this:
    Data([[1,-2,3],[-3,4,-5],[5,-6,7]]) | Abs() | Sqrt()

This creates a matrix from the given data, computes for each value the absolute value, and then its
square root.  An optimized implementation could compute the individual values of the result in one
step.

The new syntax does not conflict with existing Python code where the `|` operator is the binary
OR and the pipeline sequence would compute values appropriately, if possible.  Elements of type
`Data` are not supported and therefore sequences of OR operations with a `Data` value on the left
side do not produce a value in an unmodified Python interpreter.


Examples
--------

Using the new data type and pipeline:

    $ python scql.py
    [theuser] > Data([[1,-2,3],[-3,4,-5],[5,-6,7]]) | Abs() | Sqrt()
    [[1.         1.41421356 1.73205081]
     [1.73205081 2.         2.23606798]
     [2.23606798 2.44948974 2.64575131]]
    [theuser] >

This is the example pipeline from the section **Special Syntax**. The CLI has a prompt and the
user can enter the code is one line.  It must be an expression with the exception of assignment
to storage objects.

    $ python scql.py
    [theuser] > $aa = Identity(3) | Subtract(1)
    [[ 0. -1. -1.]
     [-1.  0. -1.]
     [-1. -1.  0.]]
    [theuser] > $aa
    [[ 0. -1. -1.]
     [-1.  0. -1.]
     [-1. -1.  0.]]
    [theuser] > $theuser::aa
    [[ 0. -1. -1.]
     [-1.  0. -1.]
     [-1. -1.  0.]]

This example shows how to assign to a data object.  The name of the object does not have a
namespace component and therefore the default namespace name (`theuser`, derived from the
login name) is used.  The second and third command just recall the stored value, without and
with complete name respectively.

    $ python scql.py
    [theuser] > $aa = Identity(3)
    [[1. 0. 0.]
     [0. 1. 0.]
     [0. 0. 1.]]
    [theuser] > $bb = $aa | Multiply(4)
    [[4. 0. 0.]
     [0. 4. 0.]
     [0. 0. 4.]]
    [theuser] > $bb
    [[4. 0. 0.]
     [0. 4. 0.]
     [0. 0. 4.]]
    [theuser] > $aa = Identity(3) | Subtract(1)
    [[ 0. -1. -1.]
     [-1.  0. -1.]
     [-1. -1.  0.]]
    [theuser] > $bb
    [[ 0. -4. -4.]
     [-4.  0. -4.]
     [-4. -4.  0.]]

In this example a data object `aa` is created and initialized with the 3-by-3 identity matrix.  From
this a new data object `bb` is created by multiplying the values individually by four.

After that the value of the data object `aa` is changed.  When subsequently looking at the current
value of `bb` this change is recognized and an appropriately recomputed value is returned.
