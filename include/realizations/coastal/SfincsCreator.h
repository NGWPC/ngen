#pragma once

#include <memory>
#include <string>
#include <stdexcept>

#include <boost/property_tree/ptree.hpp>

#include "SfincsFormulation.hpp"

namespace ngen {
namespace realization {
namespace coastal {

/**
 * @brief Creator/Factory for SFINCS BMI formulation.
 *
 * This mirrors the pattern used by SCHISM in your branch: a thin factory that
 * consumes a config tree and returns a fully-configured formulation instance.
 */
class SfincsCreator
{
public:
    using ptree = boost::property_tree::ptree;

    static std::shared_ptr<SfincsFormulation> create(
        const ptree& config,
        const std::string& registration_name = "sfincs-bmi"
    )
    {
        auto f = std::make_shared<SfincsFormulation>();
        // In NGen, `create_formulation` often wants a stream handler and the path
        // to any forcing. If you don’t have a stream handler here, you can pass a
        // default-constructed one (or wire in your branch’s logging handler).
        ngen::realization::utils::StreamHandler null_stream;
        f->create_formulation(
            /* forcing_file_path */ std::string(),
            null_stream,
            config,
            registration_name,
            /* ignore_output_root */ false
        );
        return f;
    }
};

} // namespace coastal
} // namespace realization
} // namespace ngen

