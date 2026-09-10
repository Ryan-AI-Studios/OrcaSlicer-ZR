#include <catch2/catch_all.hpp>

#include "libslic3r/Model.hpp"
#include "libslic3r/SpectrumPhysicalRemap.hpp"
#include "libslic3r/TriangleMesh.hpp"
#include "libslic3r/TriangleSelector.hpp"

using namespace Slic3r;

namespace {

ModelVolume *add_painted_cube(Model &model, const std::vector<std::pair<int, int>> &facet_states)
{
    ModelObject *obj = model.add_object("physical_remap_cube", "", make_cube(20., 20., 20.));
    REQUIRE(obj != nullptr);
    obj->add_instance();
    REQUIRE_FALSE(obj->volumes.empty());
    ModelVolume *vol = obj->volumes.front();
    TriangleSelector sel(vol->mesh());
    for (const auto &facet_state : facet_states)
        sel.set_facet(facet_state.first, EnforcerBlockerType(facet_state.second));
    REQUIRE(vol->mmu_segmentation_facets.set(sel));
    return vol;
}

indexed_triangle_set facets_of(const ModelVolume &vol, int state)
{
    TriangleSelector sel(vol.mesh());
    sel.deserialize(vol.mmu_segmentation_facets.get_data(), true, EnforcerBlockerType::ExtruderMax);
    return sel.get_facets(EnforcerBlockerType(state));
}

} // namespace

TEST_CASE("physical remap swap 1 and 2", "[spectrum_physical_remap]")
{
    Model model;
    ModelVolume *vol = add_painted_cube(model, {{0, 1}, {1, 2}, {2, 0}});
    EnforcerBlockerStateMap map = spectrum_physical_remap_identity_map();
    map[1] = EnforcerBlockerType(2);
    map[2] = EnforcerBlockerType(1);
    spectrum_physical_remap_apply(model, map, false, 4, 4);
    REQUIRE(facets_of(*vol, 1).indices.size() == 1);
    REQUIRE(facets_of(*vol, 2).indices.size() == 1);
    REQUIRE(facets_of(*vol, 0).indices.size() >= 1);
}

TEST_CASE("physical remap 3 to 1 leaves NONE", "[spectrum_physical_remap]")
{
    Model model;
    ModelVolume *vol = add_painted_cube(model, {{0, 3}, {1, 0}});
    const size_t none_before = facets_of(*vol, 0).indices.size();
    EnforcerBlockerStateMap map = spectrum_physical_remap_identity_map();
    map[3] = EnforcerBlockerType(1);
    spectrum_physical_remap_apply(model, map, false, 4, 4);
    REQUIRE(facets_of(*vol, 1).indices.size() == 1);
    REQUIRE(facets_of(*vol, 3).indices.empty());
    REQUIRE(facets_of(*vol, 0).indices.size() == none_before);
}

TEST_CASE("physical remap leaves Mix 5 bitstream", "[spectrum_physical_remap]")
{
    Model model;
    ModelVolume *vol = add_painted_cube(model, {{0, 1}, {1, 5}});
    const auto mix_before = facets_of(*vol, 5);
    REQUIRE(mix_before.indices.size() == 1);
    EnforcerBlockerStateMap map = spectrum_physical_remap_identity_map();
    map[1] = EnforcerBlockerType(2);
    spectrum_physical_remap_apply(model, map, false, 4, 5);
    const auto mix_after = facets_of(*vol, 5);
    REQUIRE(mix_after.indices.size() == mix_before.indices.size());
    REQUIRE(facets_of(*vol, 2).indices.size() == 1);
    REQUIRE(facets_of(*vol, 1).indices.empty());
}

TEST_CASE("physical remap walks two objects", "[spectrum_physical_remap]")
{
    Model model;
    ModelVolume *a = add_painted_cube(model, {{0, 1}});
    ModelVolume *b = add_painted_cube(model, {{0, 1}});
    EnforcerBlockerStateMap map = spectrum_physical_remap_identity_map();
    map[1] = EnforcerBlockerType(2);
    spectrum_physical_remap_apply(model, map, false, 4, 4);
    REQUIRE(facets_of(*a, 2).indices.size() == 1);
    REQUIRE(facets_of(*b, 2).indices.size() == 1);
}

TEST_CASE("physical remap identity skip hex match", "[spectrum_physical_remap]")
{
    Model model;
    add_painted_cube(model, {{0, 1}, {1, 2}});
    SpectrumPhysicalRemapContext ctx;
    ctx.physical_n = 4;
    ctx.source_n   = 4;
    ctx.source_hexes = {"#FF0000", "#00FF00", "#0000FF", "#AAAAAA"};
    ctx.dest_hexes   = {"#FF0000", "#00FF00", "#0000FF", "#AAAAAA"};
    REQUIRE(spectrum_physical_remap_should_skip(model, ctx));

    ctx.dest_hexes[0] = "#0000AA";
    REQUIRE_FALSE(spectrum_physical_remap_should_skip(model, ctx));
}

TEST_CASE("physical remap skip false for source-palette gap", "[spectrum_physical_remap]")
{
    Model model;
    add_painted_cube(model, {{0, 7}});
    SpectrumPhysicalRemapContext ctx;
    ctx.physical_n = 4;
    ctx.source_n   = 8;
    ctx.source_hexes.assign(8, "#112233");
    ctx.dest_hexes   = {"#112233", "#445566", "#778899", "#AABBCC"};
    REQUIRE_FALSE(spectrum_physical_remap_should_skip(model, ctx));
}

TEST_CASE("physical remap skip when no paint", "[spectrum_physical_remap]")
{
    Model model;
    model.add_object("bare", "", make_cube(10., 10., 10.));
    SpectrumPhysicalRemapContext ctx;
    ctx.physical_n = 4;
    REQUIRE(spectrum_physical_remap_should_skip(model, ctx));
}

TEST_CASE("physical remap volume extruder policy", "[spectrum_physical_remap]")
{
    Model model;
    ModelVolume *vol = add_painted_cube(model, {{0, 1}});
    vol->config.set("extruder", 1);
    EnforcerBlockerStateMap map = spectrum_physical_remap_identity_map();
    map[1] = EnforcerBlockerType(2);
    spectrum_physical_remap_apply(model, map, true, 4, 5);
    REQUIRE(vol->extruder_id() == 2);

    vol->config.set("extruder", 5);
    map = spectrum_physical_remap_identity_map();
    map[1] = EnforcerBlockerType(2);
    spectrum_physical_remap_apply(model, map, true, 4, 5);
    REQUIRE(vol->extruder_id() == 5);

    vol->config.set("extruder", 7);
    map = spectrum_physical_remap_identity_map();
    map[7] = EnforcerBlockerType(3);
    spectrum_physical_remap_apply(model, map, true, 4, 4);
    REQUIRE(vol->extruder_id() == 3);
}
