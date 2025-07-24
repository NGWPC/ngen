#!/usr/bin/env bash

NPROCS=1

#partition
#/contrib/Zhengtao.Cui/home/ngwpc/ngen/cmake_build.parallel/partitionGenerator ../../../geopackage/gauge_50051800.gpkg ../../../geopackage/gauge_50051800.gpkg Input/50051800_partition_config_${NPROCS}.json ${NPROCS} '' ''

export LD_LIBRARY_PATH=/contrib/software/hdf5/1.12.3/lib:/contrib/software/netcdf/4.7.4/lib:/contrib/software/python_3_10_14/lib

source /contrib/software/py_venvs/ngen_python_3_10_14/bin/activate

cd ../

mpirun -n ${NPROCS} cmake_build_test/ngen data/gauge_01073000/gauge_01073000.gpkg "all" data/gauge_01073000/gauge_01073000.gpkg "all" data/gauge_01073000/example_bmi_multi_realization_config_w_routing_w_coastal.json 


