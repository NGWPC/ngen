#pragma once

#include <NGenConfig.h>

#if NGEN_WITH_BMI_FORTRAN

#include <realizations/coastal/CoastalFormulation.hpp>
#include <bmi/Bmi_Fortran_Adapter.hpp>

#include <boost/core/span.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <vector>

class SfincsFormulation final : public CoastalFormulation
{
public:
    using ProviderType = data_access::MeshPointsDataProvider;

    // NOTE: no MPI_Comm here
    SfincsFormulation(std::string const& id,
                      std::string const& library_path,
                      std::string const& init_config_path,
                      std::shared_ptr<ProviderType> met_forcings,
                      std::shared_ptr<ProviderType> offshore_boundary,
                      std::shared_ptr<ProviderType> channel_flow_boundary);

    ~SfincsFormulation();

    // -------------------- DataProvider API --------------------
    using selection_type = MeshPointsSelector;
    using data_type = double;

    boost::span<const std::string> get_available_variable_names() const override;
    data_type get_value(const selection_type& selector,
                        data_access::ReSampleMethod m) override;
    void get_values(const selection_type& selector,
                    boost::span<data_type> data) override;

    // -------------------- TimeSeries formulation API --------------------
    long get_data_start_time() const override;
    long get_data_stop_time()  const override;
    long record_duration()     const override;
    size_t get_ts_index_for_time(const time_t &epoch_time) const override;

    // -------------------- CoastalFormulation API --------------------
    void initialize() override;
    void finalize() override;
    void update() override;
    void update_until(double const& time);

    double get_current_time() const override;
    double get_start_time()   const override;
    double get_end_time()     const override;
    double get_time_step()    const override;

    size_t mesh_size(std::string const& variable_name) override;

private:
    enum ProviderKind { METEO, OFFSHORE, CHANNEL };
    struct InputMapping {
        ProviderKind provider;
        std::string  provider_name;
    };

    static std::map<std::string, InputMapping> expected_input_variables_;
    static std::vector<std::string> exported_output_variable_names_;
    static std::map<std::string, std::string> ngen_to_bmi_varname_;

    void set_inputs();
    void check_forcing_provider(ProviderType const& provider);
    static inline std::string to_bmi_name(std::string const& ngen_name) {
        auto it = ngen_to_bmi_varname_.find(ngen_name);
        return (it == ngen_to_bmi_varname_.end()) ? ngen_name : it->second;
    }

    std::unique_ptr<models::bmi::Bmi_Fortran_Adapter> bmi_;

    std::map<std::string, std::string> input_variable_units_;  // BMI name -> units
    std::map<std::string, std::string> input_variable_type_;   // BMI name -> type
    std::map<std::string, size_t>      input_variable_count_;  // BMI name -> count

    std::map<std::string, std::string> output_variable_units_; // NGen name -> units
    std::map<std::string, std::string> output_variable_type_;  // NGen name -> BMI type
    std::map<std::string, size_t>      output_variable_count_; // NGen name -> count

    std::chrono::time_point<std::chrono::system_clock> current_time_;
    std::chrono::seconds time_step_length_{0};

    std::shared_ptr<ProviderType> meteorological_forcings_provider_;
    std::shared_ptr<ProviderType> offshore_boundary_provider_;
    std::shared_ptr<ProviderType> channel_flow_boundary_provider_;
};

#endif // NGEN_WITH_BMI_FORTRAN
