Sensirion Gas Index Algorithm (VOC), vendored unmodified.

- Source: https://github.com/Sensirion/gas-index-algorithm, commit 2ef9f13d225e8a0dedd3ff42dd229af4dbb1aae4 (v3.2.0),
  directory `sensirion_gas_index_algorithm/` (the floating-point variant).
- License: BSD 3-Clause, Copyright (c) 2021, Sensirion AG (see `LICENSE`; keep it with the sources and reproduce it
  in anything shipped as binary). The Sensirion name must not be used to endorse the product.
- Used by `sensor/core/vocindex.cpp`, which feeds it a humidity-corrected ln(gas resistance) of the BME688. The
  algorithm was written for the SGP40/SGP41, so the result is a relative index, not a concentration.
