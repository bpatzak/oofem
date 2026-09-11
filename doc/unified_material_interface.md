# Unified material interface: push/pull and state caching

Design note for [discussion #254](https://github.com/oofem/oofem/discussions/254).

## 1. What this changes

The material interface is split into two halves with distinct jobs.

**The push.** Once per integration point per equilibrium iteration, the element hands the material
its generalized state and the material does all of its constitutive work — internal iterations,
return mapping, phase-change evaluation, derived properties — caching the results in its status:

```cpp
virtual void updateTempState(const FloatArray &stateVector, GaussPoint *gp, TimeStep *tStep);
```

**The pull.** Terms then query whatever they need, as cheap reads of that cache, selected by
`MatResponseMode` rather than by physics-specific method names:

```cpp
virtual void   giveCharacteristicVector(FloatArray &answer,  MatResponseMode, GaussPoint*, TimeStep*) const;
virtual void   giveCharacteristicMatrix(FloatMatrix &answer, MatResponseMode, GaussPoint*, TimeStep*) const;
virtual double giveCharacteristicValue (              MatResponseMode, GaussPoint*, TimeStep*) const;
```

The contract: exactly one push per (point, iteration) before any query; `initTempStatus` precedes
the first push of a step; queries are side-effect-free.

## 2. Why — three reasons, in increasing order of importance

### 2.1 Redundant work

Every term evaluated at a point calls back into the material. Counting material entry points per
integration point per iteration in the `CMT/` benchmark decks:

| deck | terms | material calls | state pushes |
|---|---|---|---|
| `Task_5.1` Liakopoulos | 14 | 26 | 1 |
| `Task_5.2` Aboustit | 22 | 47 | 1 |
| `Task_5.3` reactive transport | 31 | 69 | 2 |

The push count stays at one or two while the query count reaches 69 — the more physics is added,
the more terms depend on a single state. In those materials the cached quantity is the van
Genuchten retention state, and `getSaturations` was called from 8 to 15 separate branches. Porting
`Liakopoulos01Material.py` reduced that to one evaluation per point per iteration, with every query
becoming a dictionary read.

### 2.2 An undeclared ordering dependency

`giveCharacteristicMatrix` takes no state argument, so it was only ever correct if some earlier call
had pushed state — and nothing said so. In the symbolic path the tangent and the residual are
assembled in **separate sweeps** over the integration points (`integrateTerm_dw` evaluates only
`evaluate_lin`, `integrateTerm_c` only `evaluate`), and state was written solely as a side effect
inside the residual path. A tangent sweep therefore consumed whatever the previous sweep had left
behind, and on the first sweep of an analysis, nothing at all.

This was live, not hypothetical. In `CMT/Task_5.1` the push was hand-assembled inside one term's
residual expression:

```
flux = vcat(eps, pw, pa);  sig = MVec(gp, ts, 3, flux);  Grad_s(du,gp).T * sig
```

and all five `CMT/` python materials carried the same workaround for queries that arrived first:

```python
def getSaturations(self, pw, pa):
    if pw is None:                       # only reachable if a query precedes any push
        pc = 0.0; Sw = 1.0; Sa = 0.0     # silently: fully saturated
```

That identical guard appearing independently in five materials is the clearest evidence that the
missing push was a framework gap users were routing around. It is deleted by the port.

The same defect is latent in the other physics families: `tm`'s `computeTangent3D` simply happens
to ignore state today, as the `@todo` at `src/tm/Materials/isoheatmat.C:84` anticipates.

### 2.3 Multi-output constitutive laws

Four structural elements bypass the cross-section entirely because the interface cannot carry extra
arguments:

```cpp
virtual void giveRealStressVectorGradientDamage(FloatArray &answer1, double &answer2,
        GaussPoint *gp, const FloatArray &totalStrain,
        double nonlocalDamageDrivningVariable, TimeStep *tStep);
```

Two inputs, two outputs. Under push/pull the extra inputs become part of the declared state layout
and the extra outputs become additional pulls — and the shapes already line up, a `double` output
mapping to `giveCharacteristicValue`. The whole `*MaterialExtensionInterface` pattern exists only to
bolt arguments onto a signature that cannot carry them.

## 3. The state layout is declared by the material

The generalized state was previously packed by convention — documented in a comment, or assembled by
hand in the input deck — and every caller had to know the offsets. Instead each material declares
its layout:

```cpp
virtual IntArray giveStateVariableIDs(MaterialMode mmode) const;
```

The entries are `InternalStateType` values, in packing order. No new enum was introduced:
`InternalStateType` is already the vocabulary for "a named quantity at an integration point" —
what `giveIPValue`/`setIPValue` speak — and already contained `IST_Pressure_2`,
`IST_PressureGradient`, `IST_FirstPKStressTensor`, `IST_InterfaceTraction` and the rest. Only two
entries were missing (`IST_TemperatureGradient`, `IST_MassConcentration_2`).

`IST_*` is the right choice over a field-only vocabulary because it names the quantity *and* the
operator applied to it — `IST_Pressure` versus `IST_PressureGradient` — which a u-p material needs,
since it consumes both.

Declared layouts:

| material | layout |
|---|---|
| `StructuralMaterial` | `{IST_StrainTensor}` |
| `TransportMaterial` | `{IST_TemperatureGradient, IST_Temperature}` |
| `UPSimpleMaterial` | `{IST_StrainTensor, IST_PressureGradient, IST_Pressure}` |
| `TMSimpleMaterial` | `{IST_StrainTensor, IST_TemperatureGradient, IST_Temperature}` |
| Liakopoulos (python) | `{IST_StrainTensor, IST_Pressure, IST_Pressure_2}` |

**Known wart, worth fixing before the interface spreads.** Fusing quantity and operator into one id
combines badly: every new field needs a new entry per operator, which already cost two additions and
left the coupled heat+mass layout unimplemented for want of a humidity-gradient entry. Reporting the
field and the operator *separately* — `{DofIDItem, operator}` pairs, the operator being one of
value / gradient / symmetric gradient / divergence — would make the sizes computable generically and
delete both the encode and the decode: `MPElement::giveStateQuantityDofID` maps the fused id back to
a `DofIDItem` immediately after it was encoded.

### Resolving where a quantity comes from

Per cell, from the integrals whose set contains it, recorded during `Integral::initialize` next to
the DOFs and integration rules that same walk already creates. Per cell rather than per problem
because it is a property of the region: a domain may carry several materials with different layouts,
each fed by its own fields.

Resolution is by `DofIDItem` (`D_u`, `T_f`, `P_f`, `P_f2`, `C_1`, `C_2`), which `Variable` already
stores — **not** by `Variable::q`, which is a degenerate placeholder in practice: every scalar field
in every `CMT/` deck is declared `quantity 3`, including temperature and both concentrations.

Only genuine unknowns qualify, because nodal unknowns must be read through the unknown field's own
interpolation and a deck may give an unknown and its weighting function different interpolations (a
non-symmetric formulation). Nothing recorded that distinction and it cannot be inferred: a term may
legitimately name one variable as both field and test field (the classic `up` formulation's main
term is `BTSigTerm(getU(),getU())`), while a pure source term names a test function as its variable
because it has no unknown to depend on. So the deck states it, with `dualto` on the test variable,
filling in the long-declared but never-set `Variable::dualVar`.

## 4. Where the push is driven from

Once per equilibrium iteration, which is the rate at which the solution actually changes —
`initForNewIteration`, plus once before the external-force assembly, since rhs terms may query
material properties too. Not from `updateComponent`, which is called several times per iteration.

A material whose declared layout the cell cannot fully supply is **skipped**, not rejected: a
thermo-mechanical material used for the thermal sub-problem alone has no displacement field, and a
material record may sit on a cell whose terms only ever ask for hardwired constants. A query that
does need the missing value reports it rather than returning stale data.

## 5. Migration

`sm`, `tm`: bridged, not rewritten. `StructuralMaterial::updateTempState` delegates to
`giveRealStressVector` and `TransportMaterial::updateTempState` to `giveFluxVector` — each family
already had a compute-and-cache method paired with a stateless tangent, under four different names:

| family | de-facto push | stateless tangent |
|---|---|---|
| `StructuralMaterial` | `giveRealStressVector_*` | `giveStiffnessMatrix` |
| `TransportMaterial` | `giveFluxVector` → `computeFlux3D/2D/1D` | `computeTangent3D/2D/1D` |
| `FluidDynamicMaterial` | `computeDeviatoricStress3D/2D/Axi` | `computeTangent{s}*` |
| `StructuralInterfaceMaterial` | `giveEngTraction_*` | `give{1,2,3}dStiffnessMatrix_*` |

So this is consolidation of an existing pattern rather than a new one.

One asymmetry to know about: the reduced-mode structural routines expand to the 3d form and delegate
to `giveRealStressVector_3d`, so the status holds six components whatever the mode; the query reduces
back via `giveReducedSymVectorForm` to the size the caller's operator matrix expects.

**Python materials** are the one place that cannot be source-compatible, since the state is no longer
passed to `giveCharacteristicVector`. The port is mechanical — move the deposit block into
`updateTempState` and declare the layout — and a material lacking `updateTempState` is reported at
initialization, with what to change, rather than failing as a `TypeError` inside the assembly loop.

**Not migrated.** `FluidDynamicMaterial` and `StructuralInterfaceMaterial` do not implement the
generic triple at all, and `src/fm/` has ~40 direct element→material calls with no cross-section
shield. `sm` elements were left alone too: `StructuralElement::updateInternalState` looks like an
obvious candidate — its body already *is* the push — but it routes through the cross-section, and
`LayeredCrossSection::giveRealStress_*` iterates the layers with a material per layer. Pushing from
the element would collapse a layered section to one material; doing it properly means a
cross-section-level push, which is a change of its own size.

## 6. Open questions

1. **Temp versus equilibrated on the pull.** The pull returns temp (current-iteration) state. Two
   callers want equilibrated: the `useUpdatedGpRecord` branch of
   `StructuralElement::giveInternalForcesVector` — a hand-rolled pull via a three-way `dynamic_cast`
   chain behind an `#ifdef __LM_MODULE`, and the highest-leverage cleanup available — and
   `PythonMaterial::printOutputAt`. Does the pull grow a `ValueModeType`, or does a second accessor
   sit beside it?
2. **The name collision.** `giveCharacteristicMatrix`/`Vector` exist as both
   `Element(…, CharType, TimeStep*)` and `Material(…, MatResponseMode, GaussPoint*, TimeStep*)`;
   likewise `updateInternalState` on both `Element(TimeStep*)` and `TransportMaterial(const
   FloatArray&, GaussPoint*, TimeStep*)`. Unrelated concepts, same names, adjacent layers. Worth
   renaming the material-level pull while it is still confined to a couple of dozen call sites.
3. **The (field, operator) split** of section 3.

## 7. Status

Landed on `feature/unified_material_interface`, full suite green throughout (412/412, with and
without python bindings):

- begin-of-step `initTempStatus` for mpm, which was never reached from that solver — a standalone
  bug that had left `UPMaterialStatus::initTempStatus` and `TMMaterialStatus::initTempStatus` dead
  in a normal run;
- the push, the declared layout, and the `sm`/`tm`/python bridges;
- interpolation operators moved onto `MPElement`, shared by the symbolic functors and the state
  assembler;
- the push driven per iteration from the symbolic solvers;
- `flux` removed from the pull, and `UPSimpleMaterial`/`TMSimpleMaterial` made native, each holding
  its generalized state once with the strain read back as its leading block;
- a shared immutable VM environment per symbolic expression, which removed per-point setup (builds
  fell from one per evaluation to one per expression: 252 → 8, 87 → 3, 248 → 4 on the symbolic
  regression decks) and made evaluation thread-safe;
- `_1dUP` added to `mmodeIs1D`, where its absence had left it classified as neither 1d, 2d nor 3d.

`CMT/Task_5.1` is ported and runs; its material was checked to produce values identical to the
pre-port formulas across six states and nine quantities. The remaining four `CMT/` materials still
use the old contract and need the same mechanical port.
