#ifndef NGEN_COASTAL_CONFIG_PARAMS
#define NGEN_COASTAL_CONFIG_PARAMS

#include <string>
#include <boost/property_tree/ptree.hpp>

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

    bool isValid();

    ModelType getModelType();
};

#endif // NGEN_COASTAL_CONFIG_PARAMS
