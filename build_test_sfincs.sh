cmake -B cmake_build -S . \
  -DNGEN_WITH_BMI_FORTRAN=ON \
  -DNGEN_BUILD_COASTAL_TESTS=ON \
  -DNGEN_ENABLE_SCHISM=OFF \
  -DSFINCS_BMI_LIBRARY=/home/mohammed.karim/Calibration/ngen/extern/SFINCS/source/src/build/libsfincs_bmi.so \
  -DSFINCS_INIT_CONFIG=/home/mohammed.karim/Calibration/ngen/extern/SFINCS/source/src/build/sfincs_config.txt
cmake --build cmake_build -j
ctest --test-dir cmake_build -N | grep -i sfincs
ctest --test-dir cmake_build -R sfincs -V

