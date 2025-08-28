#include <NGenConfig.h>

#if NGEN_WITH_BMI_FORTRAN && NGEN_WITH_MPI

#include <iostream>
#include <algorithm>
#include <map>
#include <string>
#include <stdexcept>
#include <utilities/parallel_utils.h>
#include <realizations/coastal/SfincsFormulation.hpp>

static const char* s_sfincs_registration_function = "register_bmi";

// BMI input mapping: BMI variable name -> { provider selector, name in provider }
// Your Fortran BMI exposes "rain_rate" (m s-1) on the z-grid.
std::map<std::string, SfincsFormulation::InputMapping>
SfincsFormulation::expected_input_variables_ = {
    {"rain_rate", { SfincsFormulation::METEO, "RAINRATE" }},
};

std::vector<std::string> SfincsFormulation::exported_output_variable_names_ = {
    "water_surface_elevation",
    "water_depth",
    "velocity_x",
    "velocity_y"
};

void SfincsFormulation::check_forcing_provider(ProviderType const& provider)
{
    auto available = provider.get_available_variable_names();
    for (auto const& kv : expected_input_variables_) {
        auto const& want = kv.second.provider_name;
        auto it = std::find(available.begin(), available.end(), want);
        if (it == available.end()) {
            throw std::runtime_error(std::string("Missing expected forcing variable: ") + want);
        }
    }
}

SfincsFormulation::SfincsFormulation(std::string const& id,
                                     std::string const& library_path,
                                     std::string const& init_config_path,
                                     MPI_Comm mpi_comm,
                                     std::shared_ptr<ProviderType> met_forcings,
                                     std::shared_ptr<ProviderType> offshore_boundary,
                                     std::shared_ptr<ProviderType> channel_flow_boundary)
    : CoastalFormulation(id)
    , meteorological_forcings_provider_(met_forcings)
    , offshore_boundary_provider_(offshore_boundary)
    , channel_flow_boundary_provider_(channel_flow_boundary)
{
    bmi_ = std::make_unique<models::bmi::Bmi_Fortran_Adapter>(
        id,
        library_path,
        init_config_path,
        /* model_time_step_fixed = */ true,
        s_sfincs_registration_function,
        mpi_comm
    );
}

SfincsFormulation::~SfincsFormulation() = default;

void SfincsFormulation::initialize()
{
    // ~~~~~ Inputs (BMI-advertised) ~~~~~
    auto const& input_vars = bmi_->GetInputVarNames();
    for (auto const& name : input_vars) {
        input_variable_units_[name]  = bmi_->GetVarUnits(name);
        input_variable_type_[name]   = bmi_->GetVarType(name);
        input_variable_count_[name]  = mesh_size(name);
    }

    // If we have providers and we expect inputs, sanity-check both sides
    if (meteorological_forcings_provider_)
        check_forcing_provider(*meteorological_forcings_provider_);
    if (offshore_boundary_provider_)
        check_forcing_provider(*offshore_boundary_provider_);
    if (channel_flow_boundary_provider_)
        check_forcing_provider(*channel_flow_boundary_provider_);

    for (auto const& kv : expected_input_variables_) {
        auto const& name = kv.first;
        if (input_variable_units_.find(name) == input_variable_units_.end()) {
            throw std::runtime_error("SFINCS BMI missing expected input variable '" + name + "'");
        }
    }

    // ~~~~~ Outputs ~~~~~
    auto const& output_vars = bmi_->GetOutputVarNames();
    for (auto const& name : output_vars) {
        output_variable_units_[name] = bmi_->GetVarUnits(name);
        output_variable_type_[name]  = bmi_->GetVarType(name);
        output_variable_count_[name] = mesh_size(name);
    }
    for (auto const& name : exported_output_variable_names_) {
        if (output_variable_units_.find(name) == output_variable_units_.end()) {
            throw std::runtime_error("SFINCS BMI missing expected output variable '" + name + "'");
        }
    }

    // Time bookkeeping
    auto ts = bmi_->GetTimeStep();
    if (ts <= 0.0) {
        throw std::runtime_error("SFINCS BMI returned non-positive time step");
    }
    time_step_length_ = std::chrono::seconds(static_cast<long long>(ts));

    // Anchor current_time_ to the model's start time (model seconds tick)
    current_time_ = std::chrono::system_clock::time_point{
        std::chrono::seconds(static_cast<long long>(bmi_->GetStartTime()))
    };

    // Initial input push (no-op if no providers or mapping)
    set_inputs();
}

void SfincsFormulation::finalize()
{
    if (meteorological_forcings_provider_) meteorological_forcings_provider_->finalize();
    if (offshore_boundary_provider_)       offshore_boundary_provider_->finalize();
    if (channel_flow_boundary_provider_)   channel_flow_boundary_provider_->finalize();

    bmi_->Finalize();
}

void SfincsFormulation::set_inputs()
{
    if (expected_input_variables_.empty())
        return;

    for (auto const& kv : expected_input_variables_) {
        auto const& bmi_name      = kv.first;
        auto const& mapping       = kv.second;
        auto const& provider_name = mapping.provider_name;

        ProviderType* provider = [this, sel = mapping.selector]() {
            switch (sel) {
                case METEO:        return meteorological_forcings_provider_.get();
                case OFFSHORE:     return offshore_boundary_provider_.get();
                case CHANNEL_FLOW: return channel_flow_boundary_provider_.get();
                default:           return (ProviderType*)nullptr;
            }
        }();

        if (!provider)
            continue; // nothing wired for this mapping

        // Use BMI-declared units for the variable when asking provider
        auto units = input_variable_units_.at(bmi_name);
        auto n     = mesh_size(bmi_name);

        // Select "all points" for this time slice
        MeshPointsSelector points{provider_name, current_time_, time_step_length_, units, all_points};

        std::vector<double> buf(n);
        provider->get_values(points, buf);     // fill from forcing
        bmi_->SetValue(bmi_name, buf.data());  // pass to BMI
    }
}

void SfincsFormulation::update()
{
    // Push new inputs for this step, then advance SFINCS
    set_inputs();
    bmi_->Update();
    current_time_ += time_step_length_;
}

void SfincsFormulation::update_until(double const& time)
{
    // Advance until BMI model time reaches 'time' (model seconds)
    // Keep inputs flowing each step.
    for (double t = get_current_time(); t < time; t = get_current_time()) {
        set_inputs();
        bmi_->Update();
        current_time_ += time_step_length_;
    }
}

boost::span<const std::string> SfincsFormulation::get_available_variable_names() const
{
    return exported_output_variable_names_;
}

size_t SfincsFormulation::mesh_size(std::string const& variable_name)
{
    auto nbytes   = bmi_->GetVarNbytes(variable_name);
    auto itemsize = bmi_->GetVarItemsize(variable_name);
    if (itemsize <= 0 || (nbytes % itemsize) != 0) {
        throw std::runtime_error("SFINCS variable '" + variable_name + "': bad sizes (itemsize="
                                 + std::to_string(itemsize) + ", nbytes=" + std::to_string(nbytes) + ")");
    }
    return static_cast<size_t>(nbytes / itemsize);
}

// DataProvider pass-through for outputs
SfincsFormulation::data_type
SfincsFormulation::get_value(const selection_type& selector, data_access::ReSampleMethod)
{
    std::vector<double> tmp(1, 0.0);
    get_values(selector, tmp);
    return tmp.front();
}

void SfincsFormulation::get_values(const selection_type& selector, boost::span<double> data)
{
    bmi_->GetValue(selector.variable_name, data.data());
}

// Unused in coastal path
long  SfincsFormulation::get_data_start_time() const { throw std::runtime_error(__func__); }
long  SfincsFormulation::get_data_stop_time()  const { throw std::runtime_error(__func__); }
long  SfincsFormulation::record_duration()     const { throw std::runtime_error(__func__); }
size_t SfincsFormulation::get_ts_index_for_time(const time_t&) const { throw std::runtime_error(__func__); }

double SfincsFormulation::get_current_time() { return bmi_->GetCurrentTime(); }
double SfincsFormulation::get_start_time()   { return bmi_->GetStartTime(); }
double SfincsFormulation::get_end_time()     { return bmi_->GetEndTime(); }
double SfincsFormulation::get_time_step()    { return bmi_->GetTimeStep(); }

#endif // NGEN_WITH_BMI_FORTRAN && NGEN_WITH_MPI

