#!/bin/bash

# Clone ngen submodules
#git submodule update --init --recursive
git submodule add --force https://gitlab.sh.nextgenwaterprediction.com/NGWPC/nwm-ngen/lgar-c.git extern/LASAM/
git submodule add --force https://gitlab.sh.nextgenwaterprediction.com/NGWPC/nwm-ngen/snow17.git extern/snow17/
git submodule add --force https://gitlab.sh.nextgenwaterprediction.com/NGWPC/nwm-ngen/sac-sma.git ./extern/sac-sma/sac-sma/
git submodule update --init --recursive
cp ./extern/sac-sma/sac-sma/ngen_files/sacbmi.pc.in ./extern/sac-sma/sac-sma/ngen_files/CMakeLists.txt ./extern/sac-sma
git submodule update --remote extern/cfe/cfe
git submodule update --remote extern/SoilFreezeThaw/SoilFreezeThaw
git submodule update --remote extern/SoilMoistureProfiles/SoilMoistureProfiles
git submodule set-branch --branch master extern/t-route
git submodule update --remote extern/t-route
git submodule set-branch --branch latest extern/sloth/
git submodule update --remote extern/sloth/
