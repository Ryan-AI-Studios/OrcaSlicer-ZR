#include <catch2/catch_all.hpp>

#include "libslic3r/MixedFilamentFc.hpp"
#include "libslic3r/MixedFilamentOfd.hpp"
#include "libslic3r/MixedFilamentSwatch.hpp"

#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>

#include <string>
#include <vector>

using namespace Slic3r;

namespace {

const char *k_swatch_td = R"({
  "count": 1,
  "results": [{
    "id": 327,
    "manufacturer": {"id": 59, "name": "SnoLabs"},
    "filament_type": {"name": "PLA", "parent_type": {"name": "PLA"}},
    "color_name": "Transparent Blue",
    "hex_color": "2D80A6",
    "lab_l": 51.2,
    "lab_a": -12.4,
    "lab_b": -28.1,
    "td": 1.25
  }]
})";

const char *k_swatch_null_td = R"({
  "id": 1,
  "manufacturer": {"name": "SnoLabs"},
  "filament_type": {"name": "PLA", "parent_type": {"name": "PLA"}},
  "color_name": "Transparent Blue",
  "hex_color": "2D80A6",
  "lab_l": 51.2,
  "lab_a": -12.4,
  "lab_b": -28.1,
  "td": null
})";

const char *k_two_materials = R"({
  "results": [
    {
      "manufacturer": {"name": "SnoLabs"},
      "filament_type": {"name": "PLA", "parent_type": {"name": "PLA"}},
      "color_name": "Transparent Green",
      "hex_color": "00AA00",
      "lab_l": 40, "lab_a": -20, "lab_b": 10,
      "td": 2
    },
    {
      "manufacturer": {"name": "SnoLabs"},
      "filament_type": {"name": "PETG", "parent_type": {"name": "PETG"}},
      "color_name": "Transparent Green",
      "hex_color": "00BB00",
      "lab_l": 41, "lab_a": -21, "lab_b": 11,
      "td": 3
    }
  ]
})";

SpectrumOfdVariant pick_of(const std::string &brand,
                           const std::string &variant,
                           const std::string &material)
{
    SpectrumOfdVariant v;
    v.brand    = brand;
    v.variant  = variant;
    v.material = material;
    v.filament = material;
    v.color_hexes = {"#2D80A6"};
    return v;
}

} // namespace

TEST_CASE("fc parse Lab+TD and hex normalize", "[spectrum_filamentcolors]")
{
    CHECK(spectrum_fc_normalize_hex("2D80A6") == "#2D80A6");
    CHECK(spectrum_fc_normalize_hex("#2d80a6") == "#2D80A6");
    CHECK(spectrum_fc_normalize_hex("zz") == "");

    const auto page = spectrum_fc_parse_swatch_list(k_swatch_td);
    REQUIRE(page.size() == 1);
    CHECK(page[0].hex == "#2D80A6");
    CHECK(page[0].L == 51.2);
    REQUIRE(page[0].td.has_value());
    CHECK(*page[0].td == 1.25f);
}

TEST_CASE("fc nullable TD keeps Lab", "[spectrum_filamentcolors]")
{
    const auto page = spectrum_fc_parse_swatch_list(k_swatch_null_td);
    REQUIRE(page.size() == 1);
    CHECK(page[0].L == 51.2);
    CHECK_FALSE(page[0].td.has_value());
}

TEST_CASE("fc bad JSON is empty", "[spectrum_filamentcolors]")
{
    CHECK(spectrum_fc_parse_swatch_list("").empty());
    CHECK(spectrum_fc_parse_swatch_list("{not json").empty());
    CHECK(spectrum_fc_parse_swatch_list("[]").empty());
    CHECK_FALSE(spectrum_fc_parse_manufacturer_id("{\"count\":0,\"results\":[]}").has_value());
    CHECK_FALSE(spectrum_fc_parse_manufacturer_id("{\"count\":2,\"results\":[{\"id\":1},{\"id\":2}]}").has_value());
    REQUIRE(spectrum_fc_parse_manufacturer_id("{\"count\":1,\"results\":[{\"id\":59}]}").value() == 59);
}

TEST_CASE("fc match key separates PLA vs PETG", "[spectrum_filamentcolors]")
{
    CHECK(spectrum_fc_match_key("SnoLabs", "Transparent Green", "PLA") !=
          spectrum_fc_match_key("SnoLabs", "Transparent Green", "PETG"));

    const auto page = spectrum_fc_parse_swatch_list(k_two_materials);
    REQUIRE(page.size() == 2);

    const auto pla = spectrum_fc_match(pick_of("SnoLabs", "Transparent Green", "PLA"), page);
    REQUIRE(pla.has_value());
    CHECK(pla->hex == "#00AA00");
    CHECK(pla->L == 40);

    const auto petg = spectrum_fc_match(pick_of("SnoLabs", "Transparent Green", "PETG"), page);
    REQUIRE(petg.has_value());
    CHECK(petg->hex == "#00BB00");
}

TEST_CASE("fc no match and ambiguous skip", "[spectrum_filamentcolors]")
{
    const auto page = spectrum_fc_parse_swatch_list(k_swatch_td);
    CHECK_FALSE(spectrum_fc_match(pick_of("OtherBrand", "Transparent Blue", "PLA"), page).has_value());
    CHECK_FALSE(spectrum_fc_match(pick_of("SnoLabs", "No Such Color", "PLA"), page).has_value());

    const char *dup = R"({"results":[
      {"manufacturer":{"name":"SnoLabs"},"filament_type":{"name":"PLA","parent_type":{"name":"PLA"}},
       "color_name":"Blue","hex_color":"111111","lab_l":1,"lab_a":2,"lab_b":3,"td":1},
      {"manufacturer":{"name":"SnoLabs"},"filament_type":{"name":"PLA","parent_type":{"name":"PLA"}},
       "color_name":"Blue","hex_color":"222222","lab_l":4,"lab_a":5,"lab_b":6,"td":2}
    ]})";
    CHECK_FALSE(spectrum_fc_match(pick_of("SnoLabs", "Blue", "PLA"),
                                  spectrum_fc_parse_swatch_list(dup))
                    .has_value());
}

TEST_CASE("fc stamp helper unchanged without overlay", "[spectrum_filamentcolors]")
{
    std::vector<std::string> colour{"#111111", "#222222"};
    std::vector<std::string> multi{"#111111", "#222222"};
    std::vector<std::string> type{"1", "1"};
    std::vector<char>        flags{0, 0};
    REQUIRE(spectrum_ofd_stamp_slot(colour, multi, type, flags, 0, {"#2d80a6"}, false));
    CHECK(colour[0] == "#2D80A6");
    CHECK(colour[1] == "#222222");
}

TEST_CASE("fc overlay TD does not feed batch_key", "[spectrum_filamentcolors]")
{
    const std::vector<std::string> hexes{"#111111", "#222222", "#333333", "#444444"};
    const std::vector<std::string> names{"A", "B", "C", "D"};
    const std::string              k_null = spectrum_compute_batch_key(hexes, names, nullptr);

    SpectrumFcOverlayStore store;
    SpectrumFcOverlaySlot  slot;
    slot.slot  = 0;
    slot.key   = "snolabs|pla|transparentblue";
    slot.lab_l = 51.2;
    slot.lab_a = -12.4;
    slot.lab_b = -28.1;
    slot.td    = 1.25f;
    spectrum_fc_upsert_slot(store, slot);

    const std::vector<float> overlay_td = spectrum_fc_overlay_td_vector(store, 4);
    REQUIRE(overlay_td.size() == 4);
    CHECK(overlay_td[0] == 1.25f);
    CHECK(spectrum_compute_batch_key(hexes, names, nullptr) == k_null);
    CHECK(spectrum_compute_batch_key(hexes, names, &overlay_td) != k_null);
}

TEST_CASE("fc 0055 owner Lab wins fallback LUT", "[spectrum_filamentcolors]")
{
    SpectrumFcOverlayStore store;
    SpectrumFcOverlaySlot  slot;
    slot.slot  = 0;
    slot.key   = "k";
    slot.lab_l = 1;
    slot.lab_a = 2;
    slot.lab_b = 3;
    spectrum_fc_upsert_slot(store, slot);

    SwatchLUT owner;
    owner.batch_key = "keep-me";
    owner.entries.push_back({"P1", 10, 20, 30});

    const SwatchLUT keep = spectrum_fc_fallback_lut(&owner, store);
    REQUIRE(keep.find_recipe("P1") != nullptr);
    CHECK(keep.find_recipe("P1")->L == 10);
    CHECK(keep.find_recipe("P1")->a == 20);
    CHECK(keep.batch_key == "keep-me");

    const SwatchLUT fresh = spectrum_fc_fallback_lut(nullptr, store);
    REQUIRE(fresh.find_recipe("P1") != nullptr);
    CHECK(fresh.find_recipe("P1")->L == 1);
    CHECK(fresh.find_recipe("P1")->a == 2);
}

TEST_CASE("fc persist round-trip missing file is empty", "[spectrum_filamentcolors]")
{
    const boost::filesystem::path dir =
        boost::filesystem::temp_directory_path() / "spectrum_fc_overlay_test";
    boost::system::error_code ec;
    boost::filesystem::remove_all(dir, ec);
    boost::filesystem::create_directories(dir, ec);

    CHECK(spectrum_fc_load(dir.string()).slots.empty());

    SpectrumFcOverlayStore store;
    SpectrumFcOverlaySlot  slot;
    slot.slot  = 1;
    slot.key   = "snolabs|pla|x";
    slot.lab_l = 9;
    slot.lab_a = 8;
    slot.lab_b = 7;
    slot.td    = 4.5f;
    spectrum_fc_upsert_slot(store, slot);
    REQUIRE(spectrum_fc_save(store, dir.string()));

    const SpectrumFcOverlayStore loaded = spectrum_fc_load(dir.string());
    REQUIRE(loaded.slots.size() == 1);
    CHECK(loaded.slots[0].slot == 1);
    CHECK(loaded.slots[0].lab_l == 9);
    REQUIRE(loaded.slots[0].td.has_value());
    CHECK(*loaded.slots[0].td == 4.5f);

    boost::filesystem::remove_all(dir, ec);
}
