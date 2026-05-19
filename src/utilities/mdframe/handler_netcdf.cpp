#include "mdframe/mdframe.hpp"

#include <NGenConfig.h>

#if NGEN_WITH_NETCDF

#include <boost/variant.hpp>
#include "netcdf/NetCDFManager.hpp"
#include "netcdf/NetCDFFile.hpp"
#include "netcdf/NetCDFVar.hpp"

namespace ngen {

namespace visitors {

struct mdarray_netcdf_putvar : public boost::static_visitor<void>
{
    std::shared_ptr<NetCDFVar> var;

    mdarray_netcdf_putvar(std::shared_ptr<NetCDFVar> v) : var(v) {}
    
    template<typename T>
    void operator()(const mdarray<T>& arr) const
    {
        typename mdarray<T>::size_type rank = arr.rank();
        std::vector<typename mdarray<T>::size_type> expanded_index(rank);

        for (auto it = arr.begin(); it != arr.end(); it++) {
            it.mdindex(expanded_index);
            //var.putVar(expanded_index, *it);
        }
    }

};

} // namespace visitors

void mdframe::to_netcdf(const std::string& path) const
{
    NetCDFManager nc_manager;
    int ncid = nc_manager.create_file(path);
    NetCDFFile* nc_file = nc_manager.get_file_handle();

    std::unordered_map<std::string, int> dimmap;
    for (const auto& dim : this->m_dimensions)
        dimmap[dim.name()] = nc_manager.add_dimension(dim.name(), dim.size());
    
    std::unordered_map<std::string, std::shared_ptr<NetCDFVar>> varmap;

    for (const auto& pair : this->m_variables) {
        nc_type type; // NetCDF C type
        auto& var = pair.second;

        switch (var.values().which()) { 
            case 0: 
                type = NC_INT; 
                break;
            case 1: 
                type = NC_FLOAT; 
                break;
            case 2:
                default: type = NC_DOUBLE; 
                break;
        }
        std::vector<int> dim_ids;
        std::vector<std::string> dim_names;
        dim_ids.reserve(var.rank());
        dim_names.reserve(var.rank());
        for (const auto& dimname : var.dimensions())
        {
            auto it = dimmap.find(dimname);
            if(it == dimmap.end())
                throw std::runtime_error("NetCDF dimension not found: " + dimname);

            dim_ids.push_back(it->second);
            dim_names.push_back(it->first);
        }

        nc_manager.add_variable(var.name(), type, dim_ids, dim_names);
        const auto& nc_var = nc_manager.get_ncvar_by_name(var.name());
        varmap[var.name()] = nc_var;
        visitors::mdarray_netcdf_putvar visitor(nc_var);
        boost::apply_visitor(visitor, var.values());
    }
}

} // namespace ngen

#else // NGEN_WITH_NETCDF

namespace ngen {
    void mdframe::to_netcdf(const std::string& path) const
    {
        LOG(LogLevel::FATAL, "This functionality isn't available. Compile NGen with NGEN_WITH_NETCDF=ON to enable NetCDF support");
        throw std::runtime_error("This functionality isn't available. Compile NGen with NGEN_WITH_NETCDF=ON to enable NetCDF support");
    }
}

#endif // NGEN_WITH_NETCDF
