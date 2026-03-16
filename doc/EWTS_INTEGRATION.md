# EWTS Integration

## What is EWTS?

EWTS (Error, Warning and Trapping System) is a unified logging and error-handling framework from the [nwm-ewts](https://github.com/NGWPC/nwm-ewts) repository. It provides a consistent logging interface across all languages used in ngen — C, C++, Fortran, and Python — so that every model component writes structured, traceable log output through one system.

ngen core and all BMI submodules link against EWTS. It is a **required** dependency on this branch.

---

## Architecture

EWTS is built as a separate Docker stage (`ewts-build`) and installed to `/opt/ewts` before ngen or any submodule is compiled. This means:

- Changes to ngen source do not re-trigger the EWTS build (Docker layer caching).
- The EWTS version can be pinned independently via build args.

### CMake targets

Each submodule links the EWTS target appropriate for its language:

| CMake target | Language | Used by |
|---|---|---|
| `ewts::ewts_c` | C | cfe, evapotranspiration, topmodel |
| `ewts::ewts_cpp` | C++ | LASAM, SoilFreezeThaw, SoilMoistureProfiles |
| `ewts::ewts_fortran` | Fortran | noah-owp-modular, sac-sma, snow17 |
| `ewts::ewts_ngen_bridge` | C++ (ngen↔EWTS bridge) | ngen core, partitionGenerator |
| EWTS Python wheel | Python | lstm, topoflow-glacier, t-route |

The ngen core links `ewts::ewts_ngen_bridge`, which is built with `-DEWTS_WITH_NGEN=ON`. All submodule source files include `ewts_ngen/logger.hpp` for logging.

---

## Building locally (Docker)

The standard Docker build handles EWTS automatically. To build the image:

```bash
docker build -t ngen .
```

To pin a specific EWTS branch, tag, or commit SHA:

```bash
docker build --build-arg EWTS_REF=v1.2.3 -t ngen .
docker build --build-arg EWTS_REF=abc123def456 -t ngen .
```

To use a fork:

```bash
docker build --build-arg EWTS_ORG=my-org --build-arg EWTS_REF=my-branch -t ngen .
```

---

## Building locally (without Docker)

EWTS must be installed before running CMake for ngen.

**1. Clone and build EWTS:**

```bash
git clone https://github.com/NGWPC/nwm-ewts.git
cd nwm-ewts
cmake -S . -B cmake_build \
  -DCMAKE_BUILD_TYPE=Release \
  -DEWTS_WITH_NGEN=ON \
  -DEWTS_BUILD_SHARED=ON
cmake --build cmake_build -j$(nproc)
cmake --install cmake_build --prefix /opt/ewts
```

**2. Build ngen, pointing CMake at the EWTS install:**

```bash
cmake -B cmake_build -S . \
  -DCMAKE_PREFIX_PATH=/opt/ewts \
  -DNGEN_WITH_MPI=ON \
  -DNGEN_WITH_NETCDF=ON \
  -DNGEN_WITH_SQLITE=ON \
  -DNGEN_WITH_BMI_FORTRAN=ON \
  -DNGEN_WITH_BMI_C=ON \
  -DNGEN_WITH_PYTHON=ON \
  -DNGEN_WITH_TESTS=ON \
  -DNGEN_WITH_ROUTING=ON
cmake --build cmake_build --target all
```

The `-DCMAKE_PREFIX_PATH=/opt/ewts` flag is what allows `find_package(ewts CONFIG REQUIRED)` in [CMakeLists.txt](../CMakeLists.txt) to locate `ewtsConfig.cmake`. Without it, the configure step will fail with:

```
Could not find a package configuration file provided by "ewts"
```

**3. Make EWTS shared libraries discoverable at runtime:**

```bash
export LD_LIBRARY_PATH="/opt/ewts/lib:/opt/ewts/lib64:${LD_LIBRARY_PATH}"
```

**4. Install the EWTS Python wheel** (required for lstm, topoflow-glacier, t-route):

```bash
pip install /opt/ewts/python/dist/ewts-*.whl
```

---

## Adding EWTS to a new submodule

If you are adding a new BMI submodule that should use EWTS:

1. In the submodule's `CMakeLists.txt`, add:
   ```cmake
   find_package(ewts CONFIG REQUIRED)
   target_link_libraries(<your_target> PUBLIC ewts::ewts_c)   # or ewts_cpp / ewts_fortran
   ```
   Use `PUBLIC` linkage so downstream consumers inherit the dependency.

2. In source files, include the logger header:
   ```c
   #include "ewts_ngen/logger.hpp"
   ```

3. In the ngen [Dockerfile](../Dockerfile), add a build step for the new submodule that passes `-DCMAKE_PREFIX_PATH=${EWTS_PREFIX}`:
   ```dockerfile
   RUN --mount=type=cache,target=/root/.cache/cmake,id=cmake-mymodule \
       set -eux && \
       cmake -B extern/mymodule/cmake_build -S extern/mymodule/ \
         -DCMAKE_PREFIX_PATH=${EWTS_PREFIX} -DBOOST_ROOT=/opt/boost && \
       cmake --build extern/mymodule/cmake_build/
   ```

---

## Provenance

The Docker build captures EWTS git metadata (commit hash, branch, build date) alongside ngen and all submodules into a single JSON file at `/ngen-app/ngen_git_info.json` inside the image. This allows the exact EWTS version used in any built image to be traced.
