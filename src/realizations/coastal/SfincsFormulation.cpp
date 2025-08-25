#include <NGenConfig.h>

#if NGEN_WITH_BMI_FORTRAN && NGEN_WITH_MPI

#include <stdexcept>
#include <algorithm>
#include <utilities/parallel_utils.h>
#include <realizations/coastal/SfincsFormulation.hpp>

namespace {
    // Must match the symbol exported by the Fortran BMI library (same as SCHISM pattern)
    constexpr const char* s_sfincs_registration_function = "register_bmi";
}

// No required inputs initially for SFINCS
std::map<std::string, SfincsFormulation::InputMapping>
SfincsFormulation::expected_input_variables_ = {};

// Exported outputs fixed to your BMI implementation
std::vector<std::string> SfincsFormulation::exported_output_variable_names_ = {
    "water_surface_elevation",
    "water_depth",
    "velocity_x",
    "velocity_y"
};

void SfincsFormulation::check_forcing_provider( ProviderType const& /*provider*/ )
{
    // No-op until you add expected inputs and want to assert presence of those fields
}

SfincsFormulation::SfincsFormulation(
    std::string const& id,
    std::string const& library_path,
    std::string const& init_config_path,
    MPI_Comm            mpi_comm,
    std::shared_ptr<ProviderType> met_forcings,
    std::shared_ptr<ProviderType> offshore_boundary,
    std::shared_ptr<ProviderType> channel_flow_boundary)
: CoastalFormulation(id)
, meteorological_forcings_provider_(std::move(met_forcings))
, offshore_boundary_provider_(std::move(offshore_boundary))
, channel_flow_boundary_provider_(std::move(channel_flow_boundary))
{
    bmi_ = std::make_unique<models::bmi::Bmi_Fortran_Adapter>(
        id,
        library_path,
        init_config_path,
        /* model_time_step_fixed = */ true,
        s_sfincs_registration_function,
        mpi_comm);
}

SfincsFormulation::~SfincsFormulation() = default;

void SfincsFormulation::initialize()
{
    // Collect (optional) input variables (your BMI currently returns 0)
    for (auto const& name : bmi_->GetInputVarNames()) {
        if (name == "bmi_mpi_comm_handle") continue; // handled by adapter

        // If you later define expected inputs, you can enforce them here.
        input_variable_units_[name] = bmi_->GetVarUnits(name);
        input_variable_type_[name]  = bmi_->GetVarType(name);
        input_variable_count_[name] = mesh_size(name);
    }

    // Validate and record outputs for our exported list
    auto const& bmi_outputs = bmi_->GetOutputVarNames();
    for (auto const& name : exported_output_variable_names_) {
        auto it = std::find(bmi_outputs.begin(), bmi_outputs.end(), name);
        if (it == bmi_outputs.end()) {
            throw std::runtime_error("SFINCS BMI missing expected output variable '" + name + "'");
        }
        output_variable_units_[name] = bmi_->GetVarUnits(name);
        output_variable_type_[name]  = bmi_->GetVarType(name);
        output_variable_count_[name] = mesh_size(name);
    }

    time_step_length_ = std::chrono::seconds(static_cast<long long>(bmi_->GetTimeStep()));
    current_time_     = std::chrono::system_clock::time_point{}; // not used externally

    set_inputs(); // currently does nothing
}

void SfincsFormulation::finalize()
{
    if (meteorological_forcings_provider_)    meteorological_forcings_provider_->finalize();
    if (offshore_boundary_provider_)          offshore_boundary_provider_->finalize();
    if (channel_flow_boundary_provider_)      channel_flow_boundary_provider_->finalize();

    bmi_->Finalize();
}

void SfincsFormulation::set_inputs()
{
    // No inputs for SFINCS by default; add logic here if you extend expected_input_variables_
    if (expected_input_variables_.empty()) return;

    for (auto const& kv : expected_input_variables_) {
        auto const& bmi_name = kv.first;
        auto const& mapping  = kv.second;

        ProviderType* provider = [this, &mapping]() -> ProviderType* {
            switch(mapping.selector) {
                case METEO:        return meteorological_forcings_provider_.get();
                case OFFSHORE:     return offshore_boundary_provider_.get();
                case CHANNEL_FLOW: return channel_flow_boundary_provider_.get();
                default: throw std::runtime_error("Unknown SFINCS provider selector");
            }
        }();

        if (provider == nullptr) continue;

        auto points = MeshPointsSelector{
            mapping.name,
            current_time_,
            time_step_length_,
            input_variable_units_[bmi_name],
            all_points
        };

        std::vector<double> buffer(mesh_size(bmi_name));
        provider->get_values(points, buffer);
        bmi_->SetValue(bmi_name, buffer.data());
    }
}

void SfincsFormulation::update()
{
    current_time_ += time_step_length_;
    set_inputs();
    bmi_->Update();
}

void SfincsFormulation::update_until( double const& time )
{
    double current = this->get_current_time();
    while ( current <= time ) {
        set_inputs();
        bmi_->Update();
        current = this->get_current_time();
        current_time_ += time_step_length_;
    }
}

boost::span<const std::string> SfincsFormulation::get_available_variable_names() const
{
    return exported_output_variable_names_;
}

long SfincsFormulation::get_data_start_time() const  { throw std::runtime_error(__func__); }
long SfincsFormulation::get_data_stop_time()  const  { throw std::runtime_error(__func__); }
long SfincsFormulation::record_duration()     const  { throw std::runtime_error(__func__); }
size_t SfincsFormulation::get_ts_index_for_time(const time_t &/*epoch_time*/) const
{ throw std::runtime_error(__func__); }

SfincsFormulation::data_type
SfincsFormulation::get_value(const selection_type& /*selector*/, data_access::ReSampleMethod /*m*/)
{
    // Vectorized path is expected in coastal; use get_values().
    throw std::runtime_error(__func__);
}

void SfincsFormulation::get_values(const selection_type& selector, boost::span<double> data)
{
    // Pull directly from BMI by variable name (the span size should match mesh_size())
    bmi_->GetValue(selector.variable_name, data.data());
}

size_t SfincsFormulation::mesh_size(std::string const& variable_name)
{
    auto nbytes   = bmi_->GetVarNbytes(variable_name);
    auto itemsize = bmi_->GetVarItemsize(variable_name);
    if (itemsize == 0 || (nbytes % itemsize) != 0) {
        throw std::runtime_error(
            "For SFINCS variable '" + variable_name + "': bad itemsize (" +
            std::to_string(itemsize) + ") vs nbytes (" + std::to_string(nbytes) + ")");
    }
    return static_cast<size_t>(nbytes / itemsize);
}

double SfincsFormulation::get_current_time() { return bmi_->GetCurrentTime(); }
double SfincsFormulation::get_start_time()   { return bmi_->GetStartTime();   }
double SfincsFormulation::get_end_time()     { return bmi_->GetEndTime();     }
double SfincsFormulation::get_time_step()    { return bmi_->GetTimeStep();    }

#endif // NGEN_WITH_BMI_FORTRAN && NGEN_WITH_MPI

