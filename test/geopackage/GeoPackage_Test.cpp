#include <boost/geometry/io/wkt/write.hpp>
#include <gtest/gtest.h>

#include "geopackage.hpp"
#include "FileChecker.h"

class GeoPackage_Test : public ::testing::Test
{
  protected:
    void SetUp() override 
    {
        this->path = utils::FileChecker::find_first_readable({
            "test/data/geopackage/example_nhf_4326.gpkg",
            "../test/data/geopackage/example_nhf_4326.gpkg",
            "../../test/data/geopackage/example_nhf_4326.gpkg"
        });

        if (this->path.empty()) {
            FAIL() << "can't find test/data/geopackage/example_nhf_4326.gpkg";
        }

        this->path2 = utils::FileChecker::find_first_readable({
            "test/data/geopackage/example_nhf_5070.gpkg",
            "../test/data/geopackage/example_nhf_5070.gpkg",
            "../../test/data/geopackage/example_nhf_5070.gpkg"
        });

        if (this->path2.empty()) {
            FAIL() << "can't find test/data/geopackage/example_nhf_5070.gpkg";
        }
    }

    void TearDown() override {};

    std::string path;
    std::string path2;
};

TEST_F(GeoPackage_Test, geopackage_read_test)
{
    const auto gpkg = ngen::geopackage::read(this->path, "divides", {});
    EXPECT_NE(gpkg->find("cat-3309683"), -1);
    const auto bbox = gpkg->get_bounding_box();
    EXPECT_EQ(bbox.size(), 4);
    EXPECT_NEAR(bbox[0], -72.1, 0.1);
    EXPECT_NEAR(bbox[1], 41.6, 0.1);
    EXPECT_NEAR(bbox[2], -72, 0.1);
    EXPECT_NEAR(bbox[3], 41.8, 0.1);

    EXPECT_EQ(8, gpkg->get_size());

    const auto gpkg_nexus = ngen::geopackage::read(this->path, "nexus", {});
    EXPECT_NE(gpkg_nexus->find("nex-3309683"), -1);
    const auto& first = gpkg_nexus->get_feature(0);
    const auto& last = gpkg_nexus->get_feature(gpkg->get_size());
    EXPECT_EQ(first->get_id(), "nex-3309652");

    const auto point = boost::get<geojson::coordinate_t>(first->geometry());
    EXPECT_NEAR(point.get<0>(), -72.1, 0.1);
    EXPECT_NEAR(point.get<1>(),41.6, 0.1);

    ASSERT_TRUE(last == nullptr);
}

TEST_F(GeoPackage_Test, geopackage_idsubset_test)
{
    const auto gpkg = ngen::geopackage::read(this->path, "nexus", { "nex-3309696" });
    EXPECT_NE(gpkg->find("nex-3309696"), -1);
    EXPECT_EQ(gpkg->find("nex-3309652"), -1);

    const auto& first = gpkg->get_feature(0);
    EXPECT_EQ(first->get_id(), "nex-3309696");
    const auto point = boost::get<geojson::coordinate_t>(first->geometry());
    EXPECT_NEAR(point.get<0>(), -72.1, 0.1);
    EXPECT_NEAR(point.get<1>(), 41.8, 0.1);

    ASSERT_TRUE(gpkg->get_feature(1) == nullptr);
}

// this test is essentially the same as the above, however, the coordinates
// are stored in EPSG:3857. When read in, they should convert to EPSG:4326.
TEST_F(GeoPackage_Test, geopackage_projection_test)
{
    const auto gpkg = ngen::geopackage::read(this->path2, "divides", {});
    EXPECT_NE(gpkg->find("cat-3309683"), -1);
    const auto bbox = gpkg->get_bounding_box();
    EXPECT_EQ(bbox.size(), 4);
    EXPECT_NEAR(bbox[0], -72.1, 0.1);
    EXPECT_NEAR(bbox[1], 41.6, 0.1);
    EXPECT_NEAR(bbox[2], -72, 0.1);
    EXPECT_NEAR(bbox[3], 41.8, 0.1);

    EXPECT_EQ(8, gpkg->get_size());

    const auto gpkg_nexus = ngen::geopackage::read(this->path, "nexus", {});
    EXPECT_NE(gpkg_nexus->find("nex-3309683"), -1);
    const auto& first = gpkg_nexus->get_feature(0);
    const auto& last = gpkg_nexus->get_feature(gpkg->get_size());
    EXPECT_EQ(first->get_id(), "nex-3309652");

    const auto point = boost::get<geojson::coordinate_t>(first->geometry());
    EXPECT_NEAR(point.get<0>(), -72.1, 0.1);
    EXPECT_NEAR(point.get<1>(),41.6, 0.1);

    ASSERT_TRUE(last == nullptr);
}
