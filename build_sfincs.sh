#!/usr/bin/env bash
set -euo pipefail

# Build the BMI shared library in the same environment style as the documented
# working SFINCS build.

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$ROOT/extern/SFINCS/source/src"
BUILD_DIR="$ROOT/extern/SFINCS/cmake_build"

# Prefer the active conda/pixi env if present
if [[ -n "${CONDA_PREFIX:-}" ]]; then
  export NETCDF_PREFIX="$CONDA_PREFIX"
  export PKG_CONFIG_PATH="$CONDA_PREFIX/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
fi

# Fall back to nc-config if available
if command -v nc-config >/dev/null 2>&1; then
  NC_INC="$(nc-config --includedir)"
  NC_LIB="$(nc-config --libdir)"
else
  echo "ERROR: nc-config not found on PATH"
  exit 1
fi

# Match the documented SFINCS build flags as closely as possible
export FCFLAGS="${FCFLAGS:-} -fopenmp -O3 -fallow-argument-mismatch -w"
export FFLAGS="${FFLAGS:-} -fopenmp -O3 -fallow-argument-mismatch -w"
export CFLAGS="${CFLAGS:-}"
export CXXFLAGS="${CXXFLAGS:-}"

# Clean rebuild while debugging env issues
rm -rf "$BUILD_DIR"

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="${CC:-gcc}" \
  -DCMAKE_Fortran_COMPILER="${FC:-gfortran}" \
  -DNETCDF_PREFIX="${NETCDF_PREFIX:-}" \
  -DSFINCS_ENABLE_NETCDF=ON \
  -DSFINCS_ENABLE_OPENMP=ON \
  -DSFINCS_BUILD_TESTS=OFF \
  -DCMAKE_Fortran_FLAGS_RELEASE="$FCFLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="-L$NC_LIB -Wl,-rpath,$NC_LIB" \
  -DCMAKE_SHARED_LINKER_FLAGS="-L$NC_LIB -Wl,-rpath,$NC_LIB" \
  -DCMAKE_Fortran_FLAGS="-g -O0 -fcheck=all -fbacktrace -ffpe-trap=invalid,zero,overflow" \
  -DCMAKE_C_FLAGS_RELEASE="${CFLAGS}" \
  -DCMAKE_CXX_FLAGS_RELEASE="${CXXFLAGS}"

cmake --build "$BUILD_DIR" -j1

echo
echo "Built:"
echo "  $BUILD_DIR/libsfincs_bmi.so"
echo
echo "Runtime linkage:"
ldd "$BUILD_DIR/libsfincs_bmi.so" || true
