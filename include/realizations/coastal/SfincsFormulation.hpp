#pragma once

#include <NGenConfig.h>

#if NGEN_WITH_BMI_FORTRAN && NGEN_WITH_MPI

#include <realizations/coastal/CoastalFormulation.hpp>
#include <bmi/Bmi_Fortran_Adapter.hpp>
#include <memory>
#include <map>
#include <vector>

class SfincsFormulation final : public CoastalFormulation
{
public:
    using ProviderType = data_access::MeshPointsDataProvider;

    static void check_forcing_provider(ProviderType const& provider);

    SfincsFormulation(std::string const& id,
                      std::string const& library_path,
                      std::string const& init_config_path,
                      MPI_Comm mpi_comm,
                      std::shared_ptr<ProviderType> met_forcings,
                      std::shared_ptr<ProviderType> offshore_boundary,
                      std::shared_ptr<ProviderType> channel_flow_boundary);

    ~SfincsFormulation();

    // DataProvider API (for exported outputs)
    boost::span<const std::string> get_available_variable_names() const override;

    long get_data_start_time() const override;
    long get_data_stop_time() const override;
    long record_duration() const override;
    size_t get_ts_index_for_time(const time_t &epoch_time) const override;

    data_type get_value(const selection_type& selector, data_access::ReSampleMethod m) override;
    void get_values(const selection_type& selector, boost::span<data_type> data) override;

    // CoastalFormulation
    void initialize() override;
    void finalize() override;
    void update() override;
    void update_until(double const& time);
    double get_current_time() override;
    double get_start_time() override;
    double get_end_time() override;
    double get_time_step() override;

    // Visible for future wiring
    enum ForcingSelector { METEO, OFFSHORE, CHANNEL_FLOW };
    struct InputMapping { ForcingSelector selector; std::string provider_name; };
    static std::map<std::string, InputMapping> expected_input_variables_;

protected:
    size_t mesh_size(std::string const& variable_name) override;

private:
    std::unique_ptr<models::bmi::Bmi_Fortran_Adapter> bmi_;

    // Input metadata (empty for now – BMI advertises none)
    std::map<std::string, std::string> input_variable_units_;
    std::map<std::string, std::string> input_variable_type_;
    std::map<std::string, size_t>      input_variable_count_;

    // Output metadata
    static std::vector<std::string> exported_output_variable_names_;
    std::map<std::string, std::string> output_variable_units_;
    std::map<std::string, std::string> output_variable_type_;
    std::map<std::string, size_t>      output_variable_count_;

    std::chrono::time_point<std::chrono::system_clock> current_time_;
    std::chrono::seconds time_step_length_;

    std::shared_ptr<ProviderType> meteorological_forcings_provider_;
    std::shared_ptr<ProviderType> offshore_boundary_provider_;
    std::shared_ptr<ProviderType> channel_flow_boundary_provider_;

    void set_inputs();
};

#endif // NGEN_WITH_BMI_FORTRAN && NGEN_WITH_MPI

