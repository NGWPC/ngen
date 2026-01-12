/*
 * Implement the coastal_config_params class
 */

#include "utilities/logging_utils.h"
#include "realizations/coastal/Coastal_Config_Params.h"

bool coastal_config_params::isValid()
{
    using boost::property_tree::ptree;

    auto it = params.find("params");
    if (it == params.not_found()) {
        logging::critical("\"params\" not definded in coastal realization!\n");
        return false;
    }

    ptree params_tree = params.get_child("params");

    auto require_key = [&](const char* key) -> bool {
        if (params_tree.find(key) == params_tree.not_found()) {
            logging::critical((std::string("\"") + key + "\" not definded in coastal realization!\n").c_str());
            return false;
        }
        return true;
    };

    // always required
    if (!require_key("model_type_name")) return false;
    if (!require_key("library_file")) return false;
    if (!require_key("model_start_time_in_secs")) return false;
    if (!require_key("nscribs")) return false;
    if (!require_key("working_dir")) return false;

    const std::string model_type_name = params_tree.get<std::string>("model_type_name");

    // SCHISM requires NetCDF forcing paths
    if (model_type_name == "schism_coastal_formulation") {
        if (!require_key("met_forcing_netcdf_path")) return false;
        if (!require_key("offshore_boundary_netcdf_path")) return false;
        if (!require_key("streamflow_boundary_netcdf_path")) return false;
    }
    // SFINCS does NOT require them (for now)
    else if (model_type_name == "bmi_fortran_sfincs") {
        // optional: allow empty or missing netcdf paths
    }
    else {
        logging::critical((std::string("Unknown coastal type: ") + model_type_name).c_str());
        return false;
    }

    return true;
}

/*
bool coastal_config_params::isValid()
{
    boost::property_tree::ptree::const_assoc_iterator it;

    it = params.find("params");
    if (it == params.not_found()) {
        logging::critical("\"params\" not definded in coastal realization!\n");
        return false;
    }

    boost::property_tree::ptree params_tree = params.get_child("params");

    auto require_key = [&](const char* key) -> bool {
        if (params_tree.find(key) == params_tree.not_found()) {
            logging::critical((std::string("\"") + key + "\" not definded in coastal realization!\n").c_str());
            return false;
        }
        return true;
    };

    if (!require_key("model_type_name")) return false;
    if (!require_key("library_file")) return false;
    if (!require_key("model_start_time_in_secs")) return false;
    if (!require_key("nscribs")) return false;

    // Only require forcing paths for SCHISM (SFINCS can run without them initially)
    const std::string model_type = params_tree.get<std::string>("model_type_name");
    if (model_type == "schism_coastal_formulation") {
        if (!require_key("met_forcing_netcdf_path")) return false;
        if (!require_key("offshore_boundary_netcdf_path")) return false;
        if (!require_key("streamflow_boundary_netcdf_path")) return false;
    }

    return true;
}


bool coastal_config_params::isValid()
{
    boost::property_tree::ptree::const_assoc_iterator it;

    // Top-level "params" subtree must exist
    it = params.find("params");
    if (it == params.not_found())
    {
        logging::critical("\"params\" not definded in coastal realization!\n");
        return false;
    }

    boost::property_tree::ptree params_tree = params.get_child("params");

    // Required fields for any coastal model (SCHISM, SFINCS, etc.)
    it = params_tree.find("model_type_name");
    if (it == params_tree.not_found())
    {
        logging::critical("\"model_type_name\" not definded in coastal realization!\n");
        return false;
    }

    it = params_tree.find("library_file");
    if (it == params_tree.not_found())
    {
        logging::critical("\"library_file\" not definded in coastal realization!\n");
        return false;
    }

    it = params_tree.find("model_start_time_in_secs");
    if (it == params_tree.not_found())
    {
        logging::critical("\"model_start_time_in_secs\" not definded in coastal realization!\n");
        return false;
    }

    it = params_tree.find("met_forcing_netcdf_path");
    if (it == params_tree.not_found())
    {
        logging::critical("\"met_forcing_netcdf_path\" not definded in coastal realization!\n");
        return false;
    }

    it = params_tree.find("offshore_boundary_netcdf_path");
    if (it == params_tree.not_found())
    {
        logging::critical("\"offshore_boundary_netcdf_path\" not definded in coastal realization!\n");
        return false;
    }

    it = params_tree.find("streamflow_boundary_netcdf_path");
    if (it == params_tree.not_found())
    {
        logging::critical("\"streamflow_boundary_netcdf_path\" not definded in coastal realization!\n");
        return false;
    }

    it = params_tree.find("nscribs");
    if (it == params_tree.not_found())
    {
        logging::critical("\"nscribs\" not definded in coastal realization!\n");
        return false;
    }

    return true;
}
*/

ModelType coastal_config_params::getModelType()
{
    std::string model_type =
        params.get_child("params").get<std::string>("model_type_name");

    if (model_type == std::string("schism_coastal_formulation"))
    {
        return ModelType::SCHISM;
    }
    else if (model_type == std::string("bmi_fortran_sfincs"))
    {
        return ModelType::SFINCS;
    }
    else
    {
        logging::critical((std::string("Unknown coastal type: ") + model_type).c_str());
        throw std::runtime_error(std::string("FATAL: Unknown coastal type: ") + model_type);
    }
}

