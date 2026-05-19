#include <NGenConfig.h>

#include "gtest/gtest.h"

#if NGEN_WITH_NETCDF
#include "netcdf/NetCDFManager.hpp"
#include "netcdf/NetCDFFile.hpp"
#include "netcdf/NetCDFVar.hpp"
#endif

#include "mdframe.hpp"

class mdframe_netcdf_Test : public ::testing::Test
{
  protected:
    mdframe_netcdf_Test()
        : path(testing::TempDir())
    {
        char last_char = *(path.end() - 1);

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
        if (last_char == '\\')
            path.append("\\");
#else
        if (last_char != '/')
            path.append("/");
#endif

        path.append("ngen__mdframe_Test_netCDF.nc");
    }

    ~mdframe_netcdf_Test() override
    {
        unlink(this->path.c_str());
    }

    std::string path;
};

// TODO: Convert to test fixture for setup/teardown members.
TEST_F(mdframe_netcdf_Test, io_netcdf)
{
#if !NGEN_WITH_NETCDF
    GTEST_SKIP() << "NetCDF is not available";
#else

    ngen::mdframe df;

    df.add_dimension("x", 2)
      .add_dimension("y", 2);

    df.add_variable<int>("x", { "x" })
      .add_variable<int>("y", { "y" })
      .add_variable<double>("v", {"x", "y"});

    for (size_t x = 0; x < 2; x++) {
        df["x"].insert({{ x }}, x + 1);
        for (size_t y = 0; y < 2; y++) {
            df["y"].insert({{ y }}, y + 1);
            df["v"].insert({{ x, y }}, (x + 1) * (y + 1));
        }
    }

    df.to_netcdf(this->path);

    NetCDFFile ex(this->path, false, false);

    int dim_id = ex.get_dim_id("x");
    ASSERT_FALSE(dim_id == -1);
    ASSERT_EQ(ex.get_dim_size("x"), 2);

    dim_id = ex.get_dim_id("y");
    ASSERT_FALSE(dim_id == -1);
    ASSERT_EQ(ex.get_dim_size("y"), 2);

    auto xvar = ex.get_ncvar("x");
    auto yvar = ex.get_ncvar("y");
    auto vvar = ex.get_ncvar("v");

    EXPECT_EQ(xvar->get_dim_count(), 1);
    EXPECT_EQ(yvar->get_dim_count(), 1);
    EXPECT_EQ(vvar->get_dim_count(), 2);

    int    xval = 0;
    int    yval = 0;
    double vval = 0;
    for (size_t x = 0; x < 2; x++) {
        std::vector<size_t> index = {x};
        int xval = xvar->get_int_value_at_index(index);
        EXPECT_EQ(xval, x + 1);
        for (size_t y = 0; y < 2; y++) {
            index = {y};
            int yval = yvar->get_int_value_at_index(index);
            EXPECT_EQ(yval, y + 1);
            index = {x, y};
            double vval = vvar->get_dbl_value_at_index(index);
            EXPECT_EQ(vval, (x + 1) * (y + 1));
        }
    }

  if (ex.get_ncid() >= 0) nc_close(ex.get_ncid());
#endif
}
