#ifndef NGEN_REALIZATION_CONFIG_COASTAL_H
#define NGEN_REALIZATION_CONFIG_COASTAL_H

#include <boost/property_tree/ptree.hpp>

#include "coastal/Coastal_Config_Params.h"

namespace realization{
  namespace config{

    static const std::string COASTAL_CONFIG_KEY = "coastal";
    struct Coastal{
        std::shared_ptr<coastal_config_params> params;
        Coastal(const boost::property_tree::ptree& tree){
            params = std::make_shared<coastal_config_params>(tree.get_child( COASTAL_CONFIG_KEY ));
        }
    };

  }//end namespace config
}//end namespace realization
#endif //NGEN_REALIZATION_CONFIG_COASTAL_H
