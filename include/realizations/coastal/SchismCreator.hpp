#ifndef SCHISM_CREATOR_HEADER
#define SCHISM_CREATOR_HEADER

#include "realizations/coastal/ModelCreator.hpp"
#include "realizations/coastal/Coastal_Config_Params.hpp"

class SchismCreator : public ModelCreator {
public:
    std::unique_ptr<CoastalFormulation>
                    createCoastalFormulation( coastal_config_params const&,
                                              Simulation_Time const&  ) const override;
    SchismCreator* clone() const override;
private:
    void writeInitConfig(coastal_config_params const&,
                         Simulation_Time const& ) const;
};

#endif // #ifndef SCHISM_CREATOR_HEADER

