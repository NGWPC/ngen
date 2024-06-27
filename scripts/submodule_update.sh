#!/bin/bash

#git config --global url."https://oauth2:${GITLAB_TOKEN}@gitlab.sh.nextgenwaterprediction.com/".insteadOf "https://gitlab.sh.nextgenwaterprediction.com/"
#git submodule sync --recursive

git submodule add --force https://gitlab.sh.nextgenwaterprediction.com/NGWPC/nwm-ngen/lgar-c.git extern/LASAM/
git submodule add --force https://gitlab.sh.nextgenwaterprediction.com/NGWPC/nwm-ngen/snow17.git extern/snow17/
git submodule add --force https://gitlab.sh.nextgenwaterprediction.com/NGWPC/nwm-ngen/sac-sma.git ./extern/sac-sma/sac-sma/

cp ./extern/sac-sma/sac-sma/ngen_files/sacbmi.pc.in ./extern/sac-sma/sac-sma/ngen_files/CMakeLists.txt ./extern/sac-sma

git submodule update --remote extern/SoilFreezeThaw/SoilFreezeThaw
git submodule update --remote extern/SoilMoistureProfiles/SoilMoistureProfiles
git submodule update --remote extern/cfe/cfe
git submodule update --remote extern/evapotranspiration/evapotranspiration
git submodule update --remote extern/noah-owp-modular/noah-owp-modular
git submodule update --remote extern/topmodel/topmodel
git submodule set-branch --branch development extern/t-route
git submodule update --remote extern/t-route
git submodule set-branch --branch development extern/sloth/
git submodule update --remote extern/sloth/
