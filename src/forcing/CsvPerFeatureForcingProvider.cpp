#include <forcing/CsvPerFeatureForcingProvider.hpp>

CsvPerFeatureForcingProvider::CsvPerFeatureForcingProvider(forcing_params forcing_config)
    : start_date_time_epoch(forcing_config.simulation_start_t)
    , end_date_time_epoch(forcing_config.simulation_end_t)
    , current_date_time_epoch(forcing_config.simulation_start_t)
    , forcing_file_name(forcing_config.path)
{
    read_csv(forcing_file_name);
}

long CsvPerFeatureForcingProvider::get_data_start_time() const {
    //FIXME: Trace this back and you will find that it is the simulation start time, not having anything to do with the forcing at all.
    // Apparently this "worked", but at a minimum the description above is false.
    return start_date_time_epoch;
}

long CsvPerFeatureForcingProvider::get_data_stop_time() const {
    return end_date_time_epoch;
}

long CsvPerFeatureForcingProvider::record_duration() const {
    return time_epoch_vector[1] - time_epoch_vector[0];
}

size_t  CsvPerFeatureForcingProvider::get_ts_index_for_time(const time_t &epoch_time) const {
    if (epoch_time < start_date_time_epoch) {
        std::string throw_msg; throw_msg.assign("Forcing had bad pre-start time for index query: " + std::to_string(epoch_time));
        LOG(throw_msg, LogLevel::WARNING);
        throw std::out_of_range(throw_msg);
    }
    size_t i = 0;
    // 1 hour
    time_t seconds_in_time_step = 3600;
    time_t time = start_date_time_epoch;
    while (epoch_time >= time + seconds_in_time_step && time < end_date_time_epoch) {
        i++;
        time += seconds_in_time_step;
    }
    // The end_date_time_epoch is the epoch value of the BEGINNING of the last time step, not its end.
    // I.e., to make sure we cover it, we have to go another time step beyond.
    if (time >= end_date_time_epoch + 3600) {
        std::string throw_msg; throw_msg.assign("Forcing had bad beyond-end time for index query: " + std::to_string(epoch_time));
        LOG(throw_msg, LogLevel::WARNING);
        throw std::out_of_range(throw_msg);
    }
    else {
        return i;
    }
}

double CsvPerFeatureForcingProvider::get_value(const CatchmentAggrDataSelector& selector, data_access::ReSampleMethod m) {
    auto init_time = selector.get_init_time();
    auto duration = selector.get_duration_secs();
    auto output_name = selector.get_variable_name();
    auto output_units = selector.get_output_units();

    size_t current_index;
    long time_remaining = selector.get_duration_secs();

    try {
        current_index = get_ts_index_for_time(init_time);
    }
    catch (const std::out_of_range &e) {
        std::string throw_msg; throw_msg.assign("Forcing had bad init_time " + std::to_string(init_time) + " for value request");
        LOG(throw_msg, LogLevel::WARNING);
        throw std::out_of_range(throw_msg);
    }

    std::vector<double> involved_time_step_values;

    std::vector<long> involved_time_step_seconds;
    long ts_involved_s;

    time_t first_time_step_start_epoch = start_date_time_epoch + (current_index * 3600);
    // Handle the first time step differently, since we need to do more to figure out how many seconds came from it
    // Total time step size minus the offset of the beginning, before the init time
    ts_involved_s = 3600 - (init_time - first_time_step_start_epoch);

    involved_time_step_seconds.push_back(ts_involved_s);
    involved_time_step_values.push_back(get_value_for_param_name(output_name, current_index));
    time_remaining -= ts_involved_s;
    current_index++;

    while (time_remaining > 0) {
        if (current_index >= time_epoch_vector.size()) {
            // XXX There may be callers almost-reasonably relying on being able to request data extending one record_duration() past the end
            Logger::logMsgAndThrowError("Requested forcing value runs past the end of the data from CSV file '" + forcing_file_name + "'");
        }
        ts_involved_s = time_remaining > 3600 ? 3600 : time_remaining;
        involved_time_step_seconds.push_back(ts_involved_s);
        involved_time_step_values.push_back(get_value_for_param_name(output_name, current_index));
        time_remaining -= ts_involved_s;
        current_index++;
    }

    double value = 0;
    for (size_t i = 0; i < involved_time_step_values.size(); ++i) {
        if (is_param_sum_over_time_step(output_name))
            value += involved_time_step_values[i] * ((double)involved_time_step_seconds[i] / 3600.0);
        else
            value += involved_time_step_values[i] * ((double)involved_time_step_seconds[i] / (double)selector.get_duration_secs());
    }

    // Convert units
    try {
        return UnitsHelper::get_converted_value(available_forcings_units[output_name], value, output_units);
    }
    catch (const std::runtime_error& e) {
        data_access::unit_conversion_exception uce(e.what());
        uce.provider_model_name = "CsvPerFeatureProvider (file '" + forcing_file_name + "')";
        uce.provider_bmi_var_name = output_name;
        uce.provider_units = available_forcings_units[output_name];
        uce.unconverted_values.push_back(value);
        throw uce;
    }
}

std::vector<double> CsvPerFeatureForcingProvider::get_values(const CatchmentAggrDataSelector& selector, data_access::ReSampleMethod m) {
    return std::vector<double>(1, get_value(selector, m));
}

bool CsvPerFeatureForcingProvider::is_param_sum_over_time_step(const std::string& name) const {
    if (name == CSDMS_STD_NAME_RAIN_VOLUME_FLUX) {
        return true;
    }
    if (name == CSDMS_STD_NAME_SOLAR_SHORTWAVE) {
        return true;
    }
    if (name == CSDMS_STD_NAME_SOLAR_LONGWAVE) {
        return true;
    }
    if (name == CSDMS_STD_NAME_LIQUID_EQ_PRECIP_RATE) {
        return true;
    }
    return false;
}

bool CsvPerFeatureForcingProvider::is_property_sum_over_time_step(const std::string& name) const {
    return is_param_sum_over_time_step(name);
}

boost::span<const std::string> CsvPerFeatureForcingProvider::get_available_variable_names() const {
    return available_forcings;
}

double CsvPerFeatureForcingProvider::get_value_for_param_name(const std::string& name, int index) const {
    if (index < 0 || index >= time_epoch_vector.size() ) {
        std::string throw_msg; throw_msg.assign("Forcing had bad index " + std::to_string(index) + " for value lookup of " + name);
        LOG(throw_msg, LogLevel::WARNING);
        throw std::out_of_range(throw_msg);
    }

    std::string const* can_name = &name;

    auto wkf_iter = data_access::WellKnownFields.find(name);
    if (wkf_iter != data_access::WellKnownFields.end()) {
        can_name = &std::get<0>(wkf_iter->second);
    }

    auto forcings_iter = forcing_vectors.find(*can_name);
    if (forcings_iter != forcing_vectors.end()) {
        return forcings_iter->second.at(index);
    }
    else {
        std::string throw_msg; throw_msg.assign("Cannot get forcing value for unrecognized parameter name '" + name + "'.");
        LOG(throw_msg, LogLevel::WARNING);
        throw std::runtime_error(throw_msg);
    }
}

void CsvPerFeatureForcingProvider::read_csv(std::string const& file_name) {
    int time_col_index = 0;
    //std::map<std::string, int> col_indices;
    std::vector<std::vector<double>*> local_valvec_index = {};

    //Call CSVReader constuctor
    CSVReader reader(file_name);

    //Get the data from CSV File
    std::vector<std::vector<std::string> > data_list = reader.getData();

    // Process the header (first) row..
    int col_num = 0;
    for (const auto& col_head : data_list[0]){
        //std::cerr << s << std::endl;
        if(col_head == "Time" || col_head == "time"){
            time_col_index = col_num;
            local_valvec_index.push_back(nullptr); // make sure the column indices line up!
        } else {
            std::string var_name = col_head;
            std::string units = "";

            boost::trim(var_name); // remove leading/trailing ws
            const auto var_name_close = var_name.back();
            if (var_name_close == ']' || var_name_close == ')') {
                // found closing bracket/parenth

                const bool is_bracket = var_name_close == ']';
                const size_t var_name_open = is_bracket ? var_name.rfind('[') : var_name.rfind('(');
                if (var_name_open != std::string::npos) {
                    // found matching opening bracket/parenth

                    units = var_name.substr(var_name_open + 1);
                    units.pop_back(); // remove closing bracket

                    var_name = var_name.substr(0, var_name_open);
                    boost::trim(var_name); // trim again in case of ws between name and units
                }
            }

            LOG("CsvProvider has variable '" + var_name + "' with units '" + units + "'", LogLevel::DEBUG);

            auto wkf = data_access::WellKnownFields.find(var_name);
            if(wkf != data_access::WellKnownFields.end()){
                units = units.empty() ? std::get<1>(wkf->second) : units;
                auto wkf_name = std::get<0>(wkf->second);
                LOG("CsvProvider has well-known name '" + wkf_name + "' for variable '" + var_name + "' with units '" + units + "'", LogLevel::DEBUG);
                available_forcings.push_back(var_name); // Allow lookup by non-canonical name
                available_forcings_units[var_name] = units; // Allow lookup of units by non-canonical name
                var_name = wkf_name; // Use the CSDMS name from here on
            }

            forcing_vectors[var_name] = {};
            local_valvec_index.push_back(&(forcing_vectors[var_name]));
            available_forcings.push_back(var_name);
            available_forcings_units[var_name] = units;
        }
        col_num++;
    }

    time_t current_row_date_time_epoch;
    //Iterate through CSV starting on the second row
    int i = 1;
    for (i = 1; i < data_list.size(); i++) {
        //Row vector
        std::vector<std::string>& vec = data_list[i];

        struct tm current_row_date_time_utc = tm();
        std::string time_str = vec[time_col_index];
        //TODO: Support more time string formats? This is basically ISO8601 but not complete, support TZ?
        strptime(time_str.c_str(), "%Y-%m-%d %H:%M:%S", &current_row_date_time_utc);

        //Convert current row date-time UTC to epoch time
        current_row_date_time_epoch = timegm(&current_row_date_time_utc);

        //TODO: I am not sure this is a concern of this object. If forcing is retrieved that doesn't cover the
        //needed time period, isn't that the requester's concern? (Methods exist to check this...)
        //Ensure that forcing data covers the entire model period. Otherwise, throw an error.
        if (i == 1 && start_date_time_epoch < current_row_date_time_epoch) {
            struct tm start_date_tm;
            gmtime_r(&start_date_time_epoch, &start_date_tm);

            char tm_buff[128];
            strftime(tm_buff, 128, "%Y-%m-%d %H:%M:%S", &start_date_tm);
            std::string throw_msg; throw_msg.assign("Error: Forcing data " + file_name + " begins after the model start time:" + std::string(tm_buff) + " < " + time_str);
            LOG(throw_msg, LogLevel::WARNING);
            throw std::out_of_range(throw_msg);
        }


        if (start_date_time_epoch <= current_row_date_time_epoch && current_row_date_time_epoch <= end_date_time_epoch) {
            time_epoch_vector.push_back(current_row_date_time_epoch);

            int c = -1;
            for (auto& s : vec){
                c++;
                if(c == time_col_index)
                    continue;
                boost::algorithm::trim(s);
                local_valvec_index[c]->push_back(boost::lexical_cast<double>(s)); // This is supposed to update the vector in the map...
            }

        }

    }
    if (i <= 1 || current_row_date_time_epoch < end_date_time_epoch) {
        /// \todo TODO: Return appropriate error
        std::stringstream ss;
        ss << "CSV Forcing data ends before the model end time in file '" << forcing_file_name << "'";
        LOG(ss.str(), LogLevel::SEVERE);
        //std::string throw_msg; throw_msg.assign("Error: Forcing data ends before the model end time.");
    }

    time_t duration = record_duration();
    if (duration != 3600) {
        Logger::logMsgAndThrowError("CSV reader is hard-coded for hour-long records");
    }
    for (size_t i = 1; i < time_epoch_vector.size(); ++i) {
        time_t difference = time_epoch_vector[i] - time_epoch_vector[i-1];
        if (difference != duration) {
            Logger::logMsgAndThrowError("Time intervals in forcing file '" + forcing_file_name + "' are not constant at row " + std::to_string(i));
        }
    }
}
