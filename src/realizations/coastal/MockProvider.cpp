#include "realizations/coastal/MockProvider.h"
#include "forcing/GenericDataProvider.hpp"

std::vector<MockProvider::data_type> MockProvider::get_values(const selection_type& selector, 
		data_access::ReSampleMethod m) 
{ 
	throw ""; 
	return data; 
}

void MockProvider::get_values(const selection_type& selector, boost::span<double> out_data)
{
        auto default_value = input_variables_defaults.at(selector.variable_name);
        for (auto& val : out_data) {
            val = default_value;
        }
}
