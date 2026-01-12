#include <realizations/coastal/SfincsFormulation.hpp>

#include <memory>
#include <string>

int main(int /*argc*/, char** /*argv*/)
{
    // These two are required by the SfincsFormulation constructor
    const std::string lib  = "libsfincs_bmi.so";  // can be a real path in runtime tests
    const std::string init = "sfincs.json";       // can be a real config in runtime tests

    // Updated provider types (matches SfincsFormulation.hpp constructor in your build errors)
    using ProviderPtr = std::shared_ptr<data_access::DataProvider<double, MeshPointsSelector>>;

    ProviderPtr met    = nullptr;
    ProviderPtr off    = nullptr;
    ProviderPtr chflow = nullptr;

    // Just ensure we can instantiate the object (compile/link smoke test)
    SfincsFormulation f("sfincs_demo", lib, init, met, off, chflow);

    (void)f;
    return 0;
}

