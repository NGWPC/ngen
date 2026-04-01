/**
 * The ModelCreator class declares the factory method that is supposed to return an
 * object of a CoastalFormulation  class. The ModelCreator's subclasses usually provide the
 * implementation of this method.
 */
#ifndef MODEL_CREATOR_HEADER
#define MODEL_CREATOR_HEADER

#include <cmath>
#include <limits>
#include <iostream>
#include "realizations/coastal/CoastalFormulation.hpp"
#include "realizations/coastal/Coastal_Config_Params.hpp"
#include "simulation_time/Simulation_Time.hpp"

// AbstractFactory
class ModelCreator {
public:
    virtual std::unique_ptr<CoastalFormulation> createCoastalFormulation(
                    coastal_config_params const&,
                    Simulation_Time const&  ) const = 0;

    virtual ModelCreator* clone() const = 0;

    virtual void executeModel( coastal_config_params const&,
                    Simulation_Time const& );
};

#endif //#ifndef MODEL_CREATOR_HEADER

