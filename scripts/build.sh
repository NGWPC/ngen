#!/bin/bash

dnf install -y epel-release
dnf config-manager --set-enabled powertools

dnf install -y git gcc-toolset-10 gcc-toolset-10-libasan-devel libasan6 udunits2 udunits2-devel bzip2 bzip2-devel zlib zlib-devel openmpi openmpi-devel sqlite sqlite-devel openssl-devel curl cmake gcc-c++ gcc-gfortran libcurl-devel m4 libffi-devel

mkdir -p /ngen-app/opt
cd /ngen-app/opt
curl -L -O https://boostorg.jfrog.io/artifactory/main/release/1.79.0/source/boost_1_79_0.tar.bz2
tar -xjf boost_1_79_0.tar.bz2

mkdir -p /opt
cd /opt
curl -L -O https://github.com/HDFGroup/hdf5/archive/refs/tags/hdf5-1_10_11.tar.gz
tar xzf hdf5-1_10_11.tar.gz

scl enable gcc-toolset-10 bash <<EOF
  cd hdf5-hdf5-1_10_11
  ./configure --prefix=/opt/
  make check -j 4
  make install
EOF

curl -L -O https://github.com/Unidata/netcdf-c/archive/refs/tags/v4.7.4.tar.gz
tar xzf v4.7.4.tar.gz

scl enable gcc-toolset-10 bash <<EOF
  cd netcdf-c-4.7.4
  CPPFLAGS=-I/opt/include/ LDFLAGS="-Wl,-L/opt/lib/,-rpath,/opt/lib/" ./configure --prefix=/opt/
  make -j 4
  make install
EOF

curl -L -O https://github.com/Unidata/netcdf-fortran/archive/refs/tags/v4.5.4.tar.gz
tar xzf v4.5.4.tar.gz

scl enable gcc-toolset-10 bash <<EOF
  cd netcdf-fortran-4.5.4
  CPPFLAGS=-I/opt/include/ LDFLAGS="-Wl,-L/opt/lib/,-rpath,/opt/lib/" ./configure --prefix=/opt/
  make check -j 4
  make install
EOF

curl -L -O https://www.python.org/ftp/python/3.10.14/Python-3.10.14.tgz
tar xzf Python-3.10.14.tgz

scl enable gcc-toolset-10 bash <<EOF
  cd Python-3.10.14
  ./configure --enable-optimizations --with-lto --enable-shared --prefix=/opt/
  make -s -j 4
  make install
EOF

export BOOST_ROOT=/ngen-app/opt/boost_1_79_0
export CXX=/bin/g++
export PATH="/opt/:/opt/bin/:/ngen-app/opt/:/usr/lib64/openmpi/bin/:$PATH"
export LD_LIBRARY_PATH="/opt/:/ngen-app/opt/:/opt/lib/:/ngen-app/opt/lib/:$LD_LIBRARY_PATH"
export NETCDFALTERNATIVE="/usr/lib64/openmpi/"
export NETCDF="/opt/include/"

mkdir -p ../ngen-python
python3.10 -m venv ../ngen-python
source ../ngen-python/bin/activate
pip3 install bmipy numpy==1.26.4 pyyaml pandas netCDF4==1.6.3 Cython==3.0.3 wheel

git config --global --add safe.directory /ngen-app/ngen
cd /ngen-app/ngen

# cmake_build
cmake -B cmake_build -S . -DNGEN_WITH_MPI=ON -DNGEN_WITH_NETCDF=ON -DNGEN_WITH_SQLITE=ON -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
cmake --build cmake_build --target all

# verify build
./cmake_build/ngen

# cmake_build_no_mpi
#cmake -B cmake_build_no_mpi -S . -DNGEN_WITH_MPI=OFF -DNGEN_WITH_NETCDF=ON -DNGEN_WITH_SQLITE=ON -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
#cmake --build cmake_build_no_mpi --target all

# verify build
#./cmake_build_no_mpi/ngen

# cmake_build_no_netcdf
#cmake -B cmake_build_no_netcdf -S . -DNGEN_WITH_MPI=ON -DNGEN_WITH_NETCDF=OFF -DNGEN_WITH_SQLITE=ON -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
#cmake --build cmake_build_no_netcdf --target all

# verify build
#./cmake_build_no_netcdf/ngen

# cmake_build_no_mpi_netcdf
#cmake -B cmake_build_no_mpi_netcdf -S . -DNGEN_WITH_MPI=OFF -DNGEN_WITH_NETCDF=OFF -DNGEN_WITH_SQLITE=ON -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
#cmake --build cmake_build_no_mpi_netcdf --target all

# verify build
#./cmake_build_no_mpi_netcdf/ngen

# cmake_build_no_sqlite
#cmake -B cmake_build_no_sqlite -S . -DNGEN_WITH_MPI=ON -DNGEN_WITH_NETCDF=ON -DNGEN_WITH_SQLITE=OFF -DNGEN_WITH_UDUNITS=ON -DNGEN_WITH_BMI_FORTRAN=ON -DNGEN_WITH_BMI_C=ON -DNGEN_WITH_PYTHON=ON -DNGEN_WITH_TESTS=ON -DNGEN_WITH_ROUTING=ON -DNGEN_QUIET=ON
#cmake --build cmake_build_no_sqlite --target all

# verify build
#./cmake_build_no_sqlite/ngen

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

cmake -B extern/cfe/cmake_build -S extern/cfe/
make -C extern/cfe/cmake_build/

#cd extern/t-route/
#./compiler.sh no-e
