#ifndef NGEN_COASTAL_CONFIG_PARAMS
#define NGEN_COASTAL_CONFIG_PARAMS

#include <string>
#include <boost/property_tree/ptree.hpp>
#include "utilities/logging_utils.h"


//Coastal model types
enum class ModelType{
        SCHISM,
        SFINCS,
};

/**
 * @brief coastal_config_params providing configuration information for coastal model.
 */
struct coastal_config_params
{
     boost::property_tree::ptree params;

    /**
     * Default constructor
     */
    coastal_config_params(){}

    /*
     * @brief Constructor for coastal_config_params
     *
     * @param coastal_config 
     */
    coastal_config_params( const boost::property_tree::ptree& coastal_config):
        params(coastal_config)
        {
        }

    bool isValid()
    {
       //to-do: add validation 
       return true;
    }

    ModelType getModelType()
    {
        std::string model_type = params.get_child("params").get<std::string>("model_type_name" );
        if ( model_type == std::string( "bmi_fortran_schism" ) )
	{
		return ModelType::SCHISM;
	}
	else if ( model_type == std::string("bmi_fortran_sfincs" ) )
	{
		return ModelType::SFINCS;
	}
	else
	{

		logging::critical((std::string("Unknown coastal type: ") + model_type).c_str()); 
                throw std::runtime_error( std::string( "FATAL: Unknown coastal type: ") 
				                         + model_type );
	}  
    }

};

#endif // NGEN_COASTAL_CONFIG_PARAMS
