#ifndef SFINCS_CREATOR_HEADER
#define SFINCS_CREATOR_HEADER

#include "realizations/coastal/ModelCreator.h"
#include "realizations/coastal/Coastal_Config_Params.h"

class SfincsCreator : public ModelCreator {
public:
    std::unique_ptr<CoastalFormulation>
    createCoastalFormulation(coastal_config_params const&,
                             Simulation_Time const&) const override;

    SfincsCreator* clone() const override;

private:
    void writeInitConfig(coastal_config_params const&,
                         Simulation_Time const&) const;
};

#endif // SFINCS_CREATOR_HEADER

