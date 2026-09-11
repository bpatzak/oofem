# OOFEM SymbolicTerm Documentation

## 1. Overview

The `SymbolicTerm` class in the OOFEM Multi-Physics Module allows users to define custom terms for weak-form equations directly within the input file using a symbolic mathematical language. 

Under the hood, it employs a lightweight, high-performance scripting virtual machine (VM) specifically optimized for matrix mathematics in Finite Element Analysis (FEA). Expressions are parsed and compiled into bytecode during the initialization phase, and then executed from a pre-allocated memory pool during the performance-critical assembly loops. Each expression also gets an immutable execution environment (symbol table, functor table, and the invariant bindings) built once at initialization; an evaluation copies only the slot pool it writes into, so evaluations are independent of one another.

## 2. Evaluation Mechanics (LHS vs. RHS)

In OOFEM's finite element formulation, a term typically needs to provide both a Left-Hand Side (LHS) matrix and a Right-Hand Side (RHS) vector. `SymbolicTerm` explicitly separates these into two distinct expressions:

*   **`lexpression` (Left-Hand Side):** This expression is evaluated by the `evaluate_lin` method. It is used to compute the tangent/stiffness matrix contribution of the term. The result must evaluate to a 2D matrix (`FloatMatrix`).
*   **`rexpression` (Right-Hand Side):** This expression is evaluated by the `evaluate` method. It is used to compute the internal force or residual vector contribution. The expression evaluates to a column matrix, which the term automatically converts into a 1D array (`FloatArray`).

## 3. Input File Syntax

The `SymbolicTerm` inherits from `GenericCellTerm` and adds specific string fields to define the symbolic math.

**General Syntax:**
```oofem
SymbolicTerm <ID> variable "<var_name>" testvariable "<test_name>" mmode <mode_id> lexpression "<lhs_math>" rexpression "<rhs_math>" [optional arguments]
```

**Attributes:**
*   **`variable`**: The name of the primary unknown field variable (e.g., `"u"`, `"p"`). Must match a defined `Variable` record in the problem.
*   **`testvariable`**: The name of the test/weighting function variable (e.g., `"w"`, `"dp"`).
*   **`mmode`**: An integer identifying the material mode (e.g., `7` for a specific 2D/3D continuum mode).
*   **`lexpression`**: A string enclosed in quotes defining the symbolic math for the LHS matrix.
*   **`rexpression`**: A string enclosed in quotes defining the symbolic math for the RHS vector.
*   *(Optional)* Attributes like `ctype`, `atype`, or `uvmt` can also be used as supported by the parent `GenericCellTerm`.

A `Variable` record that is a test (weighting) function should say so, with `dualto "<name>"`
naming the unknown it weights:

```oofem
Variable name "pw"  interpolation "feilin" type 0 quantity 3 size 1 dofs 1 11
Variable name "dpw" interpolation "feilin" type 0 quantity 3 size 1 dofs 1 11 dualto "pw"
```

Otherwise the two records are indistinguishable — they may even carry different interpolations,
which is what a non-symmetric (Petrov-Galerkin) formulation looks like — and the assembly cannot
tell which field to read nodal unknowns from when pushing state to the material. It is required
only where a deck names a test function as some term's `variable`, which happens for pure source
terms (those with a zero `lexpression`), since they have no unknown to depend on.

## 4. Symbolic Language Syntax

The expression language supports standard mathematical and logical operations. Multiple statements can be chained using semicolons (`;`).

| Feature | Syntax | Example |
| :--- | :--- | :--- |
| **Assignment** | `variable = expression` | `K = MDer(gp, ts, MatResponseMode::TangentStiffness);` |
| **Arithmetic** | `+`, `-`, `*`, unary `-` | `A + B`, `A * 2.0`, `-A` |
| **Matrix Transpose** | `.T` | `Grad_s(w,gp).T` |
| **Scalar Literal** | Standard numbers | `1.0`, `5`, `-2.4e-7` |
| **Matrix Literal** | `[[r1c1, r1c2], [r2c1, r2c2]]` | `I = [[1, 0], [0, 1]];` |
| **Comparisons** | `==`, `!=`, `>`, `<`, `>=`, `<=` | `a > 5` |
| **Logical Operators**| `&&`, `||` | `(a > 5) && (b < 2)` |
| **Ternary If** | `if(condition, true, false)`| `val = if(a > 0, 1, -1);` |
| **Slicing** | `Matrix[row, col]` | `val = K[0, 1]; row_vec = K[0, :];` |

## 5. Context Variables

Within the execution context of a term on an element, the VM automatically exposes the following variables:

*   **`gp`** (`GaussPoint*`): Pointer to the current Gauss integration point.
*   **`ts`** (`TimeStep*`): Pointer to the current analysis time step.
*   **`cell`** (`MPElement*`): Pointer to the current element being evaluated.
*   **Problem Variables**: Any field variables registered in the problem (e.g., `u`, `p`, `w`, `dp`) are exposed by their name as `Variable*` objects.

## 6. Built-in Functions (Functors)

The following functions are registered in the VM to handle core FEA tensor and matrix operations:

| Function Signature | Description | Returns |
| :--- | :--- | :--- |
| `Grad_s(var, gp)` | Symmetric gradient (strain) operator of a vector field; shape depends on the material mode. | `FloatMatrix` |
| `Grad(var, gp)` | Gradient operator of a scalar field; one row per spatial direction. | `FloatMatrix` |
| `Div(var, gp)` | Divergence operator matrix for a vector field. | `FloatMatrix` (row vector) |
| `N(var, gp)` | Shape function interpolation matrix (N-matrix). | `FloatMatrix` |
| `MDer(gp, ts, mode)` | Characteristic **matrix** of the material for the given `MatResponseMode` (tangent, conductivity, permeability, ...). | `FloatMatrix` |
| `MVec(gp, ts, mode)` | Characteristic **vector** of the material for the given mode (stress, flux, ...). | `FloatMatrix` (column vector) |
| `MProp(gp, ts, mode)` | Characteristic **scalar** of the material for the given mode (capacity, Biot constant, density, ...). | scalar |
| `Sig(var, gp, ts)` | Stress vector; shorthand for `MVec(gp, ts, MatResponseMode::Stress)`. | `FloatMatrix` (column vector) |
| `Sig_dev(var, gp, ts)`| Deviatoric stress vector; shorthand for `MVec(gp, ts, MatResponseMode::DeviatoricStress)`. | `FloatMatrix` (column vector) |
| `ru(var, cell, ts)` | "Reads Unknowns": vector of nodal values of a field on the current element. | `FloatMatrix` (column vector) |
| `rv(var, cell, ts)` | Nodal *velocities* (rates) of a field on the current element. | `FloatMatrix` (column vector) |
| `vcat(a, b, ...)` | Vertical concatenation of matrices/vectors. | `FloatMatrix` |
| `eval(var, gp, ts)` | Value of a field interpolated at the integration point. | scalar |
| `LumpMatrix(m)` | HRZ lumping of a consistent mass matrix. | `FloatMatrix` |
| `print(x)` | Debug aid: prints its argument. | its argument |

Response modes may be named — `MatResponseMode::TangentStiffness`,
`MatResponseMode::Permeability`, and so on for every enumerator — or given as the integer value.
Prefer the names: the integer values are an implementation detail, and a deck that encodes them
would change meaning silently if the enum were ever renumbered.

### The material queries are reads, not evaluations

`MDer`, `MVec`, `MProp`, `Sig` and `Sig_dev` return values the material has **already** computed.
Before any term is evaluated, the solver pushes the generalized state of each integration point to
its material once per equilibrium iteration (`Material::updateTempState`), and the material does all
of its constitutive work there and caches the results.

Two consequences for writing terms:

* A term does not assemble or pass the state. Earlier decks did this by hand, building the state
  with `vcat` and handing it to `MVec`; that is no longer needed, and `MVec` no longer takes a state
  argument. What the material expects, and in what order, is declared by the material itself
  (`giveStateVariableIDs`), not by the deck.
* The result no longer depends on evaluation order. The tangent and the residual are assembled in
  separate sweeps over the integration points; previously only the residual sweep supplied a state,
  so a tangent query consumed whatever the previous sweep had left behind.

The `var` argument of `Sig` and `Sig_dev` is retained for compatibility and is checked against the
field that actually supplies the strain on the cell; it no longer selects what is read.

## 7. Examples

### Example 1: Deviatoric Stress Term (Solid Mechanics)
Computing the standard deviatoric stiffness and internal force vector:
```oofem
SymbolicTerm 1 variable "u" testvariable "w" mmode 7 lexpression "Grad_s(w,gp).T * MDer(gp,ts,MatResponseMode::DeviatoricStiffness) * Grad_s(u,gp)" rexpression "Grad_s(w,gp).T * Sig_dev(u,gp,ts)"
```

### Example 2: Incompressibility Constraint (Mixed u-p Formulation)
Volumetric constraint relating velocity/displacement divergence to pressure:
```oofem
SymbolicTerm 2 variable "p" testvariable "w" mmode 7 lexpression "Div(w,gp).T * N(p,gp)" rexpression "Div(w,gp).T * N(p,gp) * ru(p, cell, ts)"
```

### Example 3: Pressure Stabilization Term
A stabilization term for the pressure field using a very small scaling factor:
```oofem
SymbolicTerm 4 variable "p" testvariable "dp" mmode 7 ctype 27 uvmt 1 lexpression "N(dp,gp).T * 2.4e-7 * N(p,gp)" rexpression "N(dp,gp).T * 2.4e-7 * N(p,gp) * ru(p, cell, ts)"
```