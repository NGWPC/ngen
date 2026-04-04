#ifndef NGEN_FORMULATION_MANAGER_H
#define NGEN_FORMULATION_MANAGER_H

#include <NGenConfig.h>
#include "ewts_ngen/logger.hpp"

#include <memory>
#include <sstream>
#include <tuple>
#include <functional>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
#include <regex>
#include <sys/types.h>
#include <unistd.h>
#include <string>

#include <cstring>
#include <fstream>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <vector>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <FeatureBuilder.hpp>
#include "features/Features.hpp"
#include "Formulation_Constructors.hpp"
#include "LayerData.hpp"
#include "realizations/config/time.hpp"
#include "realizations/config/routing.hpp"
#include "realizations/config/config.hpp"
#include "realizations/config/layer.hpp"
#include "forcing/ForcingsEngineDataProvider.hpp"



namespace realization {

    class Formulation_Manager {
        public:
            Formulation_Manager(std::stringstream &data) {
                boost::property_tree::ptree loaded_tree;
                try {
                    boost::property_tree::json_parser::read_json(data, loaded_tree);
                    this->tree = loaded_tree;
                }
                catch (const std::exception& e) {
                    std::string msg = std::string("Reading json data") + e.what();
                    LOG(msg, LogLevel::FATAL);
                    throw;
                }
            }

            Formulation_Manager(const std::string &file_path) {
                boost::property_tree::ptree loaded_tree;
                try {
                    boost::property_tree::json_parser::read_json(file_path, loaded_tree);
                    this->tree = loaded_tree;
                }
                catch (const std::exception& e) {
                    std::string msg = std::string("Reading json file ") + file_path + e.what();
                    LOG(msg, LogLevel::FATAL);
                    throw;
                }
            }

            Formulation_Manager(boost::property_tree::ptree &loaded_tree) {
                this->tree = loaded_tree;
            }

            ~Formulation_Manager() = default;

	    static std::string to_lower_copy(std::string s) {
                std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c){ return std::tolower(c); });
                return s;
            }

            void read(simulation_time_params &simulation_time_config,
                      geojson::GeoJSON fabric, utils::StreamHandler output_stream) {
                std::stringstream ss;
                ss.str(""); ss << "Entering Formulation_Manager::read()" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                //TODO seperate the parsing of configuration options like time
                //and routing and other non feature specific tasks from this main function
                //which has to iterate the entire hydrofabric.
                auto possible_global_config = tree.get_child_optional("global");

                if (possible_global_config) {
                    global_config = realization::config::Config(*possible_global_config);
                }

                // Log layer descriptions
                // try to get the json node
                auto layers_json_array = tree.get_child_optional("layers");
                //Create the default surface layer
                config::Layer layer;
                // layer description struct
                ngen::LayerDescription layer_desc;
                layer_desc = layer.get_descriptor();
                // add the default surface layer to storage
                layer_storage.put_layer(layer_desc, layer_desc.id);

                if (layers_json_array) {
                    
                    for (std::pair<std::string, boost::property_tree::ptree> layer_config : *layers_json_array) 
                    {
                        layer = config::Layer(layer_config.second);
                        layer_desc = layer.get_descriptor();

                        // add the layer to storage
                        layer_storage.put_layer(layer_desc, layer_desc.id);
                        ss.str(""); ss << "Layer added: ID = " << layer_desc.id << ", Name = " << layer_desc.name << std::endl;
                        LOG(ss.str(), LogLevel::DEBUG);

                        if (layer.has_formulation() && layer.get_domain() == "catchments") {
                            domain_formulations.emplace(
                                layer_desc.id,
                                construct_formulation_from_config(
                                    simulation_time_config,
                                    "layer-" + std::to_string(layer_desc.id),
                                    layer.formulation,
                                    output_stream
                                )
                            );
                            auto formulation = domain_formulations.at(layer_desc.id);
                            if (formulation->get_output_header_count() > 0) {
                                formulation->set_output_stream(get_output_root() + layer_desc.name + "_layer_"+std::to_string(layer_desc.id) + ".csv");
                            }
                        }
                        //TODO for each layer, create deferred providers for use by other layers
                        //VERY SIMILAR TO NESTED MODULE INIT
                    }
                }

                //TODO use the set of layer providers as input for catchments to lookup from

                 // Read routing configurations from configuration file
                auto possible_routing_configs = tree.get_child_optional("routing");
                
                if (possible_routing_configs) {
                    // Since it is possible to build NGEN without routing support, if we see it in the config
                    // but it isn't enabled in the build, we should at least put up a warning
                #if NGEN_WITH_ROUTING
                    this->routing_config = (config::Routing(*possible_routing_configs)).params;
                    using_routing = true;
                #else
                    using_routing = false;
                    ss.str("");
                    ss <<"Formulation Manager found routing configuration"
                       << ", but routing support isn't enabled. No routing will occur." << std::endl;
                    LOG(ss.str(), LogLevel::WARNING);
                #endif // NGEN_WITH_ROUTING
                 }

                std::unordered_map<std::string, realization::config::Formulation> formulation_groups;
                auto possible_formulation_groups = tree.get_child_optional("formulation_groups");
                if (possible_formulation_groups) {
                    for (std::pair<std::string, boost::property_tree::ptree> formulation_config : *possible_formulation_groups) {
                        std::cout << formulation_config.first.c_str() << std::endl;
                        realization::config::Formulation formulation(formulation_config.second.get_child(".")); // "." for first element in list
                        formulation_groups[formulation_config.first] = formulation;
                    }
                }

                std::unordered_map<std::string, realization::config::Forcing> forcing_groups;
                auto possible_forcing_groups = tree.get_child_optional("forcing_groups");
                if (possible_forcing_groups) {
                    for (std::pair<std::string, boost::property_tree::ptree> forcing_config : *possible_forcing_groups) {
                        realization::config::Forcing forcing(forcing_config.second);
                        forcing_groups[forcing_config.first] = forcing;
                    }
                }

                /**
                 * Read catchment configurations from configuration file
                 */      
                auto possible_catchment_configs = tree.get_child_optional("catchments");

                if (possible_catchment_configs) {
                    for (std::pair<std::string, boost::property_tree::ptree> catchment_config : *possible_catchment_configs) {
                        ss.str(""); ss << "Processing catchment: " << catchment_config.first << std::endl;
                        LOG(ss.str(), LogLevel::DEBUG);

                        int catchment_index = fabric->find(catchment_config.first);
                        if (catchment_index == -1) {
                            #ifndef NGEN_QUIET
                                ss.str("");
                                ss << "Formulation_Manager::read: Cannot create formulation for catchment "
                                   << catchment_config.first
                                   << " that isn't identified in the hydrofabric or requested subset" << std::endl;
                                LOG(ss.str(), LogLevel::WARNING);
                            #endif
                            continue;
                        }
                        realization::config::Config catchment_formulation(catchment_config.second, formulation_groups, forcing_groups);

                        if (!catchment_formulation.has_formulation()) {
                            std::string throw_msg;
                            throw_msg.assign("ERROR: No formulations defined for " + catchment_config.first + ".");
                            LOG(throw_msg, LogLevel::WARNING);
                            throw std::runtime_error(throw_msg);
                        }

                        // Parse catchment-specific model_params
                        auto catchment_feature = fabric->get_feature(catchment_index);
                        ss.str(""); ss << "Linking external properties for catchment: " << catchment_config.first << std::endl;
                        LOG(ss.str(), LogLevel::DEBUG);
                        catchment_formulation.formulation.link_external(catchment_feature);

                        this->add_formulation(
                            this->construct_formulation_from_config(
                                simulation_time_config,
                                catchment_config.first,
                                catchment_formulation,
                                output_stream
                            )
                        );
                        ss.str(""); ss << "Formulation constructed for catchment: " << catchment_config.first << std::endl;
                        LOG(ss.str(), LogLevel::DEBUG);
                    }
                }

                // Process any catchments not explicitly defined in the realization file
                for (geojson::Feature location : *fabric) {
                    if (not this->contains(location->get_id())) {
                        ss.str(""); ss << "Creating missing formulation for location: " << location->get_id() << std::endl;
                        LOG(ss.str(), LogLevel::DEBUG);
                        std::shared_ptr<Catchment_Formulation> missing_formulation = this->construct_missing_formulation(
                            location, output_stream, simulation_time_config);
                        this->add_formulation(missing_formulation);
//                        ss.str(""); ss << "Missing formulation created for location: " << location->get_id() << std::endl;
//                        LOG(ss.str(), LogLevel::DEBUG);
                    }
                }
            }

            void add_formulation(std::shared_ptr<Catchment_Formulation> formulation) {
                this->formulations.emplace(formulation->get_id(), formulation);
            }

            std::shared_ptr<Catchment_Formulation> get_formulation(std::string id) const {
                return this->formulations.at(id);
            }

            std::shared_ptr<Catchment_Formulation> get_domain_formulation(long id) const {
                return this->domain_formulations.at(id);
            }

            bool has_domain_formulation(int id) const {
                return this->domain_formulations.count(id) > 0;
            }

            bool contains(std::string identifier) const {
                return this->formulations.count(identifier) > 0;
            }

            /**
             * @return The number of elements within the collection
             */
            int get_size() {
                return this->formulations.size();
            }

            /**
             * @return Whether or not the collection is empty
             */
            bool is_empty() {
                return this->formulations.empty();
            }

            typename std::map<std::string, std::shared_ptr<Catchment_Formulation>>::const_iterator begin() const {
                return this->formulations.cbegin();
            }

            typename std::map<std::string, std::shared_ptr<Catchment_Formulation>>::const_iterator end() const {
                return this->formulations.cend();
            }

            /**
             * @return Whether or not using routing
             */
            bool get_using_routing() {
                return this->using_routing;
            }

            /**
             * @return routing t_route_config_file_with_path
             */
            std::string get_t_route_config_file_with_path() {
                std::stringstream ss;
                ss.str(""); ss << "Retrieving t_route config file path" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                if(this->routing_config != nullptr)
                    return this->routing_config->t_route_config_file_with_path;
                else
                    return "";
            }

            /**
             * Release any resources that should not be held as the run is shutting down
             *
             * In particular, this should be called before MPI_Finalize()
             */
            void finalize() {
                // The calls in these loops are staticly dispatched to
                // Catchment_Formulation::finalize(). That does not
                // inherit from DataProvider, with its virtual member
                // function of the same name.
                //
                // If any formulation class needs to customize this
                // behavior through this becoming a virtual dispatch,
                // take care. Bmi_Multi_Formulation was a concern, but
                // does not currently need to because none of its
                // constituent formulations points to any forcing
                // object other than the enclosing
                // Bmi_Multi_Formulation instance itself.
                std::stringstream ss;
                ss.str(""); ss << "Finalizing Formulation_Manager" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                for (auto const& fmap: formulations) {
                    fmap.second->finalize();
                }
                for (auto const& fmap: domain_formulations) {
                    fmap.second->finalize();
                }

#if NGEN_WITH_NETCDF
                data_access::NetCDFPerFeatureDataProvider::cleanup_shared_providers();
#endif
#if NGEN_WITH_PYTHON
                data_access::detail::ForcingsEngineStorage::instances.finalize();
#endif
                ss.str(""); ss << "Formulation_Manager finalized" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
            }

            /**
             * @brief Get the formatted output root: check the existence of the output_root directory defined
             * in realization. If true, return the directory name. Otherwise, try to create the directory
             * or throw an error on failure.
             *
             * @code{.cpp}
             * // Example config:
             * // ...
             * // "output_root": "/path/to/dir/"
             * // ...
             * const auto manager = Formulation_Manger(CONFIG);
             * manager.get_output_root();
             * //> "/path/to/dir/"
             * @endcode
             * 
             * @return std::string of the output root directory
             */
            std::string get_output_root() const {
                const auto output_root = this->tree.get_optional<std::string>("output_root");
                if (output_root != boost::none && *output_root != "") {
                    // Check if the path ends with a trailing slash,
                    // otherwise add it.
                    std::string str = output_root->back() == '/'
                           ? *output_root
                           : *output_root + "/";

                    const char* dir = str.c_str();

                    //use C++ system function to check if there is a dir match that defined in realization
                    struct stat sb;
                    if (stat(dir, &sb) == 0 && S_ISDIR(sb.st_mode)) {
                        return str;
                    } else {
                        errno = 0;
                        int result = mkdir(dir, 0755);      
                        if (result == 0)
                            return str;
                        else
                            throw std::runtime_error("failed to create directory '" + str + "': " + std::strerror(errno));
                    }
                }
 
                //for case where there is no output_root in the realization file
                return "./";
            }

            /**
             * @brief return the layer storage used for formulations
             * @return a reference to the LayerStorageObject
             */
            ngen::LayerDataStorage& get_layer_metadata() {
                std::stringstream ss;
                ss.str(""); ss << "Retrieving layer metadata" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                return layer_storage;
            }


        protected:
            static std::string trim_copy(const std::string& s) {
                const auto begin = s.find_first_not_of(" \t\r\n");
                if (begin == std::string::npos) {
                    return "";
                }
                const auto end = s.find_last_not_of(" \t\r\n");
                return s.substr(begin, end - begin + 1);
            }

            static std::string dirname_of(const std::string& path) {
                const auto pos = path.find_last_of('/');
                if (pos == std::string::npos) {
                    return ".";
                }
                if (pos == 0) {
                    return "/";
                }
                return path.substr(0, pos);
            }

            static std::string basename_of(const std::string& path) {
                const auto pos = path.find_last_of('/');
                if (pos == std::string::npos) {
                    return path;
                }
                return path.substr(pos + 1);
            }

            static std::string basename_without_extension(const std::string& path) {
                const std::string base = basename_of(path);
                const auto pos = base.find_last_of('.');
                if (pos == std::string::npos) {
                    return base;
                }
                return base.substr(0, pos);
            }

            static std::string join_path(const std::string& a, const std::string& b) {
                if (a.empty()) {
                    return b;
                }
                if (a.back() == '/') {
                    return a + b;
                }
                return a + "/" + b;
            }

            static bool is_absolute_path(const std::string& p) {
                return !p.empty() && p.front() == '/';
            }

            static std::string absolutize_path(const std::string& maybe_relative, const std::string& base_dir) {
                const std::string trimmed = trim_copy(maybe_relative);
                if (trimmed.empty()) {
                    return trimmed;
                }
                if (is_absolute_path(trimmed)) {
                    return trimmed;
                }
                return join_path(base_dir, trimmed);
            }

            static void ensure_directory_exists(const std::string& dir) {
                struct stat sb {};
                if (stat(dir.c_str(), &sb) == 0) {
                    if (S_ISDIR(sb.st_mode)) {
                        return;
                    }
                    throw std::runtime_error("Path exists but is not a directory: " + dir);
                }

                if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
                    throw std::runtime_error("Failed to create directory '" + dir + "': " + std::string(std::strerror(errno)));
                }
            }

            static void parse_realization_datetime(
                const std::string& dt,
                int& year,
                int& month,
                int& day,
                int& hour,
                int& minute,
                int& second
            ) {
                if (std::sscanf(dt.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
                    throw std::runtime_error("Unable to parse realization datetime: " + dt);
                }
            }

            static std::string format_ueb_datetime_line(const std::string& dt) {
                int year, month, day, hour, minute, second;
                parse_realization_datetime(dt, year, month, day, hour, minute, second);
                const double fractional_hour = static_cast<double>(hour)
                                             + (static_cast<double>(minute) / 60.0)
                                             + (static_cast<double>(second) / 3600.0);

                std::ostringstream oss;
                oss << year << " " << month << " " << day << " "
                    << std::fixed << std::setprecision(6) << fractional_hour;
                return oss.str();
            }

            static std::string sanitize_identifier_for_filename(std::string s) {
                std::replace_if(
                    s.begin(),
                    s.end(),
                    [](unsigned char c) {
                        return !(std::isalnum(c) || c == '_' || c == '-');
                    },
                    '_'
                );
                return s;
            }

            bool property_map_contains_ueb_module(const geojson::PropertyMap& params) const {
                auto model_type_it = params.find(BMI_REALIZATION_CFG_PARAM_REQ__MODEL_TYPE);
                if (model_type_it != params.end() &&
                    model_type_it->second.get_type() == geojson::PropertyType::String) {
                    const std::string model_type_name = to_lower_copy(model_type_it->second.as_string());
                    if (model_type_name.find("ueb") != std::string::npos) {
                        return true;
                    }

                }

                auto modules_it = params.find("modules");
                if (modules_it != params.end() &&
                    modules_it->second.get_type() == geojson::PropertyType::List) {
                    for (const auto& module_prop : modules_it->second.as_list()) {
                        if (module_prop.get_type() != geojson::PropertyType::Object) {
                            continue;
                        }
                        auto module_map = module_prop.get_values();
                        auto params_it = module_map.find("params");
                        if (params_it != module_map.end() &&
                            params_it->second.get_type() == geojson::PropertyType::Object) {
                            if (property_map_contains_ueb_module(params_it->second.get_values())) {
                                return true;
                            }
                        }
                    }
                }

                return false;
            }

            std::string generate_ueb_runtime_init_config(
                std::string init_config_path,
                const std::string& identifier,
                simulation_time_params& simulation_time_config
            ) const {
                const std::string id_pattern = "{{id}}";
                std::size_t pos = 0;
                while ((pos = init_config_path.find(id_pattern, pos)) != std::string::npos) {
                    init_config_path.replace(pos, id_pattern.size(), identifier);
                    pos += identifier.size();
                }

                std::ifstream ifs(init_config_path);
                if (!ifs.is_open()) {
                    throw std::runtime_error("Unable to open UEB init config: " + init_config_path);
                }

                std::vector<std::string> lines;
                std::string line;
                while (std::getline(ifs, line)) {
                    lines.push_back(line);
                }
                ifs.close();

                // Expected UEB control.dat layout:
                // 0 header
                // 1 param file
                // 2 site file
                // 3 input control
                // 4 output control
                // 5 agg output file
                // 6 watershed file
                // 7 watershed var/y/x names
                // 8 start date line
                // 9 end date line
                // 10 dt line
                // ...
                if (lines.size() < 11) {
                    throw std::runtime_error("UEB control file is shorter than expected: " + init_config_path);
                }

                const std::string control_dir = dirname_of(init_config_path);

                // Make path lines absolute so the generated file can live anywhere.
                for (int i = 1; i <= 6 && i < static_cast<int>(lines.size()); ++i) {
                    lines[i] = absolutize_path(lines[i], control_dir);
                }

                lines[8]  = format_ueb_datetime_line(simulation_time_config.start_time);
                lines[9]  = format_ueb_datetime_line(simulation_time_config.end_time);

                const double dt_hours = static_cast<double>(simulation_time_config.output_interval) / 3600.0;
                if (dt_hours <= 0.0) {
                    throw std::runtime_error("Invalid realization output_interval for UEB: " + std::to_string(simulation_time_config.output_interval));
                }

                {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(6) << dt_hours;
                    lines[10] = oss.str();
                }

                const std::string runtime_root = join_path(get_output_root(), "ueb_runtime_configs");
                ensure_directory_exists(runtime_root);

                const std::string safe_id = sanitize_identifier_for_filename(identifier);
                const std::string runtime_config =
                    join_path(runtime_root,
                              basename_without_extension(init_config_path) + "__" + safe_id + ".dat");

                std::ofstream ofs(runtime_config, std::ios::out | std::ios::trunc);
                if (!ofs.is_open()) {
                    throw std::runtime_error("Unable to write generated UEB init config: " + runtime_config);
                }

                for (std::size_t i = 0; i < lines.size(); ++i) {
                    ofs << lines[i];
                    if (i + 1 < lines.size()) {
                        ofs << "\n";
                    }
                }
                ofs.close();

                std::stringstream ss;
                ss << "Generated UEB runtime init config for " << identifier << ": " << runtime_config << std::endl;
                ss << "  realization start_time: " << simulation_time_config.start_time << std::endl;
                ss << "  realization end_time:   " << simulation_time_config.end_time << std::endl;
                ss << "  realization dt_hours:   " << dt_hours << std::endl;
                LOG(ss.str(), LogLevel::INFO);

                return runtime_config;
            }

            void apply_realization_time_to_ueb_modules(
                geojson::PropertyMap& params,
                const std::string& identifier,
                simulation_time_params& simulation_time_config
            ) const {
                auto model_type_it = params.find(BMI_REALIZATION_CFG_PARAM_REQ__MODEL_TYPE);
                auto init_config_it = params.find(BMI_REALIZATION_CFG_PARAM_REQ__INIT_CONFIG);

                if (model_type_it != params.end() &&
                    init_config_it != params.end() &&
                    model_type_it->second.get_type() == geojson::PropertyType::String &&
                    init_config_it->second.get_type() == geojson::PropertyType::String) {

		    const std::string model_type_name = to_lower_copy(model_type_it->second.as_string());

                    if (model_type_name.find("ueb") != std::string::npos) {
                        const std::string original_init_config = init_config_it->second.as_string();
                        const std::string generated_init_config =
                            generate_ueb_runtime_init_config(original_init_config, identifier, simulation_time_config);

                        params[BMI_REALIZATION_CFG_PARAM_REQ__INIT_CONFIG] =
                            geojson::JSONProperty(BMI_REALIZATION_CFG_PARAM_REQ__INIT_CONFIG, generated_init_config);
                    }
                }

                auto modules_it = params.find("modules");
                if (modules_it != params.end() &&
                    modules_it->second.get_type() == geojson::PropertyType::List) {

                    std::vector<geojson::JSONProperty> updated_modules;
                    for (auto module_prop : modules_it->second.as_list()) {
                        if (module_prop.get_type() != geojson::PropertyType::Object) {
                            updated_modules.push_back(module_prop);
                            continue;
                        }

                        auto module_map = module_prop.get_values();
                        auto nested_params_it = module_map.find("params");
                        if (nested_params_it != module_map.end() &&
                            nested_params_it->second.get_type() == geojson::PropertyType::Object) {
                            auto nested_params = nested_params_it->second.get_values();
                            apply_realization_time_to_ueb_modules(nested_params, identifier, simulation_time_config);
                            module_map["params"] = geojson::JSONProperty("params", nested_params);
                        }

                        updated_modules.emplace_back("", module_map);
                    }

                    params["modules"] = geojson::JSONProperty("modules", updated_modules);
                }
            }

            std::shared_ptr<Catchment_Formulation> construct_formulation_from_config(
                simulation_time_params &simulation_time_config,
                std::string identifier,
                const realization::config::Config& catchment_formulation,
                utils::StreamHandler output_stream
            ) {
                std::stringstream ss;
                ss.str(""); ss << "Entering construct_formulation_from_config for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Check if the formulation exists
                if (!formulation_exists(catchment_formulation.formulation.type)) {
                    std::string throw_msg;
                    throw_msg.assign("Catchment " + identifier + " failed initialization: '" +
                                     catchment_formulation.formulation.type + "' is not a valid formulation. Options are: " +
                                     valid_formulation_keys());
                    LOG(throw_msg, LogLevel::WARNING);
                    throw std::runtime_error(throw_msg);
                }

                // Check for missing forcing parameters
                ss.str(""); ss << "Checking forcing parameters for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                if (catchment_formulation.forcing.parameters.empty()) {
                    std::string throw_msg;
                    throw_msg.assign("No forcing definition was found for " + identifier);
                    LOG(throw_msg, LogLevel::FATAL);
                    throw std::runtime_error(throw_msg);
                }

                // Check for missing path
                std::vector<std::string> missing_parameters;
                if (!catchment_formulation.forcing.has_key("path")) {
                    ss.str(""); ss << "Missing path parameter for identifier: " << identifier << std::endl;
                    LOG(ss.str(), LogLevel::DEBUG);
                    missing_parameters.push_back("path");
                }

                // Log missing parameters, if any
                if (!missing_parameters.empty()) {
                    std::string message = "A forcing configuration cannot be created for '" + identifier + "'; the following parameters are missing: ";
                    for (size_t i = 0; i < missing_parameters.size(); ++i) {
                        message += missing_parameters[i];
                        if (i < missing_parameters.size() - 1) {
                            message += ", ";
                        }
                    }
                    
                    std::string throw_msg;
                    throw_msg.assign(message);
                    LOG(throw_msg, LogLevel::WARNING);
                    throw std::runtime_error(throw_msg);
                }

                // Extract forcing parameters
                forcing_params forcing_config = this->get_forcing_params(catchment_formulation.forcing.parameters, identifier, simulation_time_config);
                ss.str("");
                ss << "Forcing parameters extracted for identifier: " << identifier << std::endl;
                ss << "  Forcing path:        " << forcing_config.path << std::endl;
                ss << "  Forcing provider:    " << forcing_config.provider << std::endl;
                ss << "  Forcing init_config: " << forcing_config.init_config << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                std::time_t start_t = static_cast<std::time_t>(forcing_config.simulation_start_t);
                std::time_t end_t   = static_cast<std::time_t>(forcing_config.simulation_end_t);

                ss.str("");
                ss << "  Simulation start time: " << std::put_time(std::gmtime(&start_t), "%Y-%m-%d %H:%M:%S UTC")
                          << " (" << forcing_config.simulation_start_t << ")" << std::endl;
                ss << "  Simulation end time:   " << std::put_time(std::gmtime(&end_t), "%Y-%m-%d %H:%M:%S UTC")
                          << " (" << forcing_config.simulation_end_t << ")" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Construct formulation
                ss.str(""); ss << "Constructing formulation for type: " << catchment_formulation.formulation.type << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                std::shared_ptr<Catchment_Formulation> constructed_formulation = construct_formulation(
                    catchment_formulation.formulation.type, identifier, forcing_config, output_stream
                );
                ss.str(""); ss << "Formulation constructed successfully for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Create formulation instance
                ss.str(""); ss << "Calling create_formulation for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
            
	   	// constructed_formulation->create_formulation(catchment_formulation.formulation.parameters);
                realization::config::Config local_copy = catchment_formulation;

                if (property_map_contains_ueb_module(local_copy.formulation.parameters)) {
                    apply_realization_time_to_ueb_modules(
                        local_copy.formulation.parameters,
                        identifier,
                        simulation_time_config
                    );
                }

                constructed_formulation->create_formulation(local_copy.formulation.parameters);
                ss.str(""); ss << "Formulation creation completed for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                return constructed_formulation;
            }

            std::shared_ptr<Catchment_Formulation> construct_missing_formulation(
                geojson::Feature& feature,
                utils::StreamHandler output_stream,
                simulation_time_params &simulation_time_config
            ) {
                std::stringstream ss;
                const std::string identifier = feature->get_id();
                ss.str(""); ss << "Entering construct_missing_formulation for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Extract forcing parameters from the global config
                forcing_params forcing_config = this->get_forcing_params(global_config.forcing.parameters, identifier, simulation_time_config);
                ss.str(""); 
                ss << "Forcing parameters extracted for identifier: " << identifier << std::endl;
                ss << "  Forcing path:        " << forcing_config.path << std::endl;
                ss << "  Forcing provider:    " << forcing_config.provider << std::endl;
                ss << "  Forcing init_config: " << forcing_config.init_config << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                std::time_t start_t = static_cast<std::time_t>(forcing_config.simulation_start_t);
                std::time_t end_t   = static_cast<std::time_t>(forcing_config.simulation_end_t);

                ss.str(""); 
                ss << "  Simulation start time: " << std::put_time(std::gmtime(&start_t), "%Y-%m-%d %H:%M:%S UTC")
                          << " (" << forcing_config.simulation_start_t << ")" << std::endl;
                ss << "  Simulation end time:   " << std::put_time(std::gmtime(&end_t), "%Y-%m-%d %H:%M:%S UTC")
                          << " (" << forcing_config.simulation_end_t << ")" << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Construct the formulation object
                ss.str(""); ss << "Entering construct_formulation for identifier: " << identifier << ", type: " << global_config.formulation.type << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                std::shared_ptr<Catchment_Formulation> missing_formulation = construct_formulation(
                    global_config.formulation.type,
                    identifier,
                    forcing_config,
                    output_stream
                );
                ss.str(""); ss << "Formulation object constructed for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Make a copy of the global configuration so parameters don't clash when linking to external data
                realization::config::Config global_copy = global_config;

//                // Log parameters before substitution
//                ss.str(""); ss << "Global config parameters (before substitution) for identifier: " << identifier << std::endl;
//                for (auto it = global_copy.formulation.parameters.begin(); it != global_copy.formulation.parameters.end(); ++it) {
//                    const std::string& key = it->first;
//                    const geojson::JSONProperty& value = it->second;
//
//                    if (value.get_type() == geojson::PropertyType::String) {
//                        std::cout << "    " << key << ": " << value.as_string() << std::endl;
//                    } else {
//                        std::cout << "    " << key << ": (non-string value)" << std::endl;
//                    }
//                }

                // Substitute {{id}} in the global formulation
                ss.str(""); ss << "Checking for init_config before substitution for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                auto init_config_it = global_copy.formulation.parameters.find(BMI_REALIZATION_CFG_PARAM_REQ__INIT_CONFIG);
                if (init_config_it != global_copy.formulation.parameters.end()) {
                    const geojson::JSONProperty& init_config_property = init_config_it->second;

                    if (init_config_property.get_type() == geojson::PropertyType::String) {
                        std::string original_value = init_config_property.as_string();
                        if (!original_value.empty()) {
                            ss.str(""); ss << "construct_missing_formulation Performing pattern substitution for key: " << BMI_REALIZATION_CFG_PARAM_REQ__INIT_CONFIG
                                           << ", pattern: {{id}}, replacement: " << identifier << std::endl;
                            LOG(ss.str(), LogLevel::DEBUG);
                            ss.str("");
//                            ss.str(""); ss << "Original value: " << original_value << std::endl;
//                            LOG(ss.str(), LogLevel::DEBUG);

                            Catchment_Formulation::config_pattern_substitution(
                                global_copy.formulation.parameters,
                                BMI_REALIZATION_CFG_PARAM_REQ__INIT_CONFIG,
                                "{{id}}",
                                identifier
                            );
                        } else {
                            ss.str(""); ss << "init_config is present but empty for identifier: " << identifier << std::endl;
                            LOG(ss.str(), LogLevel::WARNING);
                        }
                    } else {
                        ss.str(""); ss << "init_config is present but not a string for identifier: " << identifier << std::endl;
                        LOG(ss.str(), LogLevel::WARNING);
                    }
                } else {
                    ss.str(""); ss << "[WARNING] init_config not present in global configuration for identifier: " << identifier << std::endl;
                    LOG(ss.str(), LogLevel::WARNING);
                }

//                // Log parameters after substitution
//                ss.str(""); ss << "Global config parameters (after substitution) for identifier: " << identifier << std::endl;
//                LOG(ss.str(), LogLevel::DEBUG);
//                for (auto it = global_copy.formulation.parameters.begin(); it != global_copy.formulation.parameters.end(); ++it) {
//                    const std::string& key = it->first;
//                    const geojson::JSONProperty& value = it->second;
//
//                    if (value.get_type() == geojson::PropertyType::String) {
//                        std::cout << "    " << key << ": " << value.as_string() << std::endl;
//                    } else {
//                        std::cout << "    " << key << ": (non-string value)" << std::endl;
//                    }
//                }

                // Link external properties
                ss.str(""); ss << "Linking external properties for identifier: " << identifier << std::endl;
		LOG(ss.str(), LogLevel::DEBUG);
                auto formulation = realization::config::Formulation(global_copy.formulation);
                formulation.link_external(feature);

                // Apply realization time override for UEB before BMI initialization
                if (property_map_contains_ueb_module(formulation.parameters)) {
                    apply_realization_time_to_ueb_modules(
                        formulation.parameters,
                        identifier,
                        simulation_time_config
                    );
                }

                // Create the formulation
                ss.str(""); ss << "Creating formulation for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);
                missing_formulation->create_formulation(formulation.parameters);

                ss.str(""); ss << "Formulation creation completed for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                return missing_formulation;
            }

            forcing_params get_forcing_params(const geojson::PropertyMap &forcing_prop_map, std::string identifier, simulation_time_params &simulation_time_config) {
                std::stringstream ss;
                ss.str(""); ss << "Entering get_forcing_params for identifier: " << identifier << std::endl;
                LOG(ss.str(), LogLevel::DEBUG);

                // Extract the required 'path' parameter
                std::string path = "";
                if (forcing_prop_map.count("path") != 0) {
                    path = forcing_prop_map.at("path").as_string();
                    ss.str(""); ss << "  Forcing path: " << path << std::endl;
                    LOG(ss.str(), LogLevel::DEBUG);
                }

                // Extract the required 'provider' parameter
                std::string provider;
                if (forcing_prop_map.count("provider") != 0) {
                    provider = forcing_prop_map.at("provider").as_string();
                    ss.str(""); ss << "  Forcing provider: " << provider << std::endl;
                    LOG(ss.str(), LogLevel::DEBUG);
                }

                // Extract the optional 'init_config' parameter from 'params'
                std::string init_config = "";
                if (forcing_prop_map.count("params") != 0) {
                    const geojson::JSONProperty& params_property = forcing_prop_map.at("params");
                    if (params_property.get_type() == geojson::PropertyType::Object) {
                        const geojson::PropertyMap& params_map = params_property.get_values();
                        if (params_map.count("init_config") != 0) {
                            init_config = params_map.at("init_config").as_string();
                            ss.str(""); ss << "  Forcing init_config: " << init_config << std::endl;
                            LOG(ss.str(), LogLevel::DEBUG);
                        }
                    } else {
                        std::cout << "[WARNING] 'params' is not an object for identifier: " << identifier << std::endl;
                    }
                }

                // If no file pattern is present, return the parameters directly
                if (forcing_prop_map.count("file_pattern") == 0) {
                    return forcing_params(
                        path,
                        provider,
                        simulation_time_config.start_time,
                        simulation_time_config.end_time,
                        init_config
                    );
                }

                // Ensure the 'path' is set for pattern matching
                if (path.empty()) {
                    std::string throw_msg = "Error with NGEN config - 'path' in forcing params must be set to a "
                                            "non-empty parent directory path when 'file_pattern' is used.";
                    LOG(throw_msg, LogLevel::WARNING);
                    throw std::runtime_error(throw_msg);
                }

                // Append a trailing slash to the path if not already present
                if (path.compare(path.size() - 1, 1, "/") != 0) {
                    path += "/";
                }

                // Extract and process the file pattern
                std::string filepattern = forcing_prop_map.at("file_pattern").as_string();
                int id_index = filepattern.find("{{id}}");

                // Replace {{id}} if present
                if (id_index != std::string::npos) {
                    filepattern = filepattern.replace(id_index, sizeof("{{id}}") - 1, identifier);
                }

                // Compile the file pattern as a regex
                std::regex pattern(filepattern);

                // A stream providing the functions necessary for evaluating a directory:
                //    https://www.gnu.org/software/libc/manual/html_node/Opening-a-Directory.html#Opening-a-Directory
                DIR *directory = nullptr;

                // structure representing the member of a directory: https://www.gnu.org/software/libc/manual/html_node/Directory-Entries.html
                struct dirent *entry = nullptr;

                // Attempt to open the directory for evaluation
                directory = opendir(path.c_str());
                // Allow for a few retries in certain failure situations
                size_t attemptCount = 0;
                std::string errMsg;

                // Retry on certain error codes
                while (directory == nullptr && attemptCount++ < 5) {
                    // For several error codes, we should break immediately and not retry
                    if (errno == ENOENT) {
                        errMsg = "No such file or directory.";
                        break;
                    }
                    if (errno == ENXIO) {
                        errMsg = "No such device or address.";
                        break;
                    }
                    if (errno == EACCES) {
                        errMsg = "Permission denied.";
                        break;
                    }
                    if (errno == EPERM) {
                        errMsg = "Operation not permitted.";
                        break;
                    }
                    if (errno == ENOTDIR) {
                        errMsg = "File at provided path is not a directory.";
                        break;
                    }
                    if (errno == EMFILE) {
                        errMsg = "The current process has too many open files.";
                        break;
                    }
                    if (errno == ENFILE) {
                        errMsg = "The system has too many open files.";
                        break;
                    }

                    // Sleep before retrying to avoid a tight loop
                    sleep(2);
                    directory = opendir(path.c_str());
                    errMsg = "Received system error number " + std::to_string(errno);
                }

                // Iterate over directory entries
                if (directory != nullptr) {
                    // handle closing the directory regardless of how the function returns
                    auto closer = [](DIR *dir){ closedir(dir); };
                    std::unique_ptr<DIR, decltype(closer)> _(directory, closer);
                    while ((entry = readdir(directory))) {
                        if (std::regex_match(entry->d_name, pattern)) {
                            // Check for regular files and symlinks
            #ifdef _DIRENT_HAVE_D_TYPE
                            if (entry->d_type == DT_REG || entry->d_type == DT_LNK) {
                                return forcing_params(
                                    path + entry->d_name,
                                    provider,
                                    simulation_time_config.start_time,
                                    simulation_time_config.end_time,
                                    init_config
                                );
                            }
                            else if (entry->d_type == DT_UNKNOWN) {
            #endif
                                // Use stat for systems that don't set d_type
                                struct stat st;
                                if (stat((path + entry->d_name).c_str(), &st) != 0) {
                                    std::string throw_msg = "Could not stat file " + path + entry->d_name;
                                    LOG(throw_msg, LogLevel::WARNING);
                                    throw std::runtime_error(throw_msg);
                                }

                                if (S_ISREG(st.st_mode)) {
                                    return forcing_params(
                                        path + entry->d_name,
                                        provider,
                                        simulation_time_config.start_time,
                                        simulation_time_config.end_time,
                                        init_config
                                    );
                                }

                                // Log a warning if the entry is not a regular file
                                std::string throw_msg = "Forcing data in path " + path + entry->d_name + " is not a file";
                                LOG(throw_msg, LogLevel::WARNING);
                                throw std::runtime_error(throw_msg);
            #ifdef _DIRENT_HAVE_D_TYPE
                            }
            #endif
                        }
                    }
                } else {
                    // The directory wasn't found or otherwise couldn't be opened; forcing data cannot be retrieved
                    std::string throw_msg = "Error opening forcing data dir '" + path + "' after " + std::to_string(attemptCount) + " attempts: " + errMsg;
                    LOG(throw_msg, LogLevel::WARNING);
                    throw std::runtime_error(throw_msg);
                }

                // If no match was found, throw an error
                std::string throw_msg = "Forcing data could not be found for '" + identifier + "'";
                LOG(throw_msg, LogLevel::WARNING);
                throw std::runtime_error(throw_msg);
            }

            boost::property_tree::ptree tree;

            realization::config::Config global_config;

            std::map<std::string, std::shared_ptr<Catchment_Formulation>> formulations;

            //Store global layer formulation pointers
            std::map<int, std::shared_ptr<Catchment_Formulation> > domain_formulations;

            std::shared_ptr<routing_params> routing_config;

            bool using_routing = false;

            ngen::LayerDataStorage layer_storage;
    };
}
#endif // NGEN_FORMULATION_MANAGER_H
