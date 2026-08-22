# AR&MF Reconstruction Baseline

The original Qt/C++ source archive is not present in this workspace. `aramf.py`
is a dependency-free reconstruction of the source-backed core: canonical
`aramf/` layout, append-only event memory, derived state, durable/production
sequence handling, and deterministic validation reports.

Run it with:

```powershell
python .\aramf.py init .
python .\aramf.py event BUILD_VERIFIED "Build verified" .
python .\aramf.py validate .
python -m unittest -v
```

This is a reconstruction baseline, not the exact lost Qt GUI or the unrecovered
Design & Resources/Thesis implementations.
