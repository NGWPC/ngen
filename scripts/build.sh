#!/bin/bash

export CXX=/bin/g++
export BOOST_ROOT=/ngen-app/opt/boost_1_79_0
export PATH="$PATH:/usr/lib64/openmpi/bin"
source /opt/ngen/python/bin/activate
pip3 install bmipy numpy==1.26.4 pyyaml pandas netCDF4==1.6.3

# cmake_build
cmake -B cmake_build -S . -DNGEN_WITH_MPI=ON -DNGEN_WITH_NETCDF=ON -DNGEN_WITH_SQLITE=ON -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
cmake --build cmake_build --target all

# verify build
./cmake_build/ngen

# cmake_build_no_mpi
cmake -B cmake_build_no_mpi -S . -DNGEN_WITH_MPI=OFF -DNGEN_WITH_NETCDF=ON -DNGEN_WITH_SQLITE=ON -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
cmake --build cmake_build_no_mpi --target all

# verify build
./cmake_build_no_mpi/ngen

# Build ngen submodules
cmake -B extern/LASAM/cmake_build -S extern/LASAM/ -DNGEN=ON
make -C extern/LASAM/cmake_build/

cmake -B extern/snow17/cmake_build -S extern/snow17/
make -C extern/snow17/cmake_build/

cmake -B extern/sac-sma/cmake_build -S extern/sac-sma/
make -C extern/sac-sma/cmake_build/

cmake -B extern/SoilMoistureProfiles/cmake_build -S extern/SoilMoistureProfiles/SoilMoistureProfiles/ -DNGEN=ON
make -C extern/SoilMoistureProfiles/cmake_build/

cmake -B extern/SoilFreezeThaw/cmake_build -S extern/SoilFreezeThaw/SoilFreezeThaw/ -DNGEN=ON
make -C extern/SoilFreezeThaw//cmake_build/

cd extern/t-route/
./compiler.sh no-e
