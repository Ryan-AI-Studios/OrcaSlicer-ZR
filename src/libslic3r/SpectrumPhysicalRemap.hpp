#pragma once

#include "TriangleSelector.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace Slic3r {

class Model;
class ModelObject;

// Open/Adopt physical remap (track 0064). Destinations are loaded physical slots only.
// Mix-recipe paint IDs stay identity. Adopt source-palette gap IDs are remappable sources.

struct SpectrumPhysicalRemapContext
{
    std::vector<std::string> source_hexes; // index 0 = paint ID 1
    std::vector<std::string> dest_hexes;   // loaded physicals, index 0 = slot 1
    size_t                   physical_n        = 0;
    size_t                   source_n          = 0;
    size_t                   enabled_mix_count = 0;
    bool                     mix_defs_nonempty = false;
    bool                     paint_mapped      = false;
};

EnforcerBlockerStateMap spectrum_physical_remap_identity_map();

bool spectrum_model_has_mmu_paint(const Model &model);

std::vector<int> spectrum_collect_used_facet_states(const Model &model);

bool spectrum_physical_remap_is_mix_recipe_id(size_t id_1based,
                                              size_t physical_n,
                                              size_t enabled_mix_count);

bool spectrum_physical_remap_is_source_gap_id(size_t id_1based,
                                              size_t physical_n,
                                              size_t source_n,
                                              size_t enabled_mix_count);

bool spectrum_physical_remap_is_remappable_source(size_t id_1based,
                                                  size_t physical_n,
                                                  size_t source_n,
                                                  size_t enabled_mix_count);

bool spectrum_physical_remap_hexes_match(const std::string &a, const std::string &b);

size_t spectrum_physical_remap_nearest_dest(const std::string              &source_hex,
                                            const std::vector<std::string> &dest_hexes);

bool spectrum_physical_remap_should_skip(const Model &model, const SpectrumPhysicalRemapContext &ctx);

EnforcerBlockerStateMap spectrum_physical_remap_seed_map(const Model                         &model,
                                                         const SpectrumPhysicalRemapContext &ctx);

void spectrum_physical_remap_apply_object(ModelObject                    &obj,
                                          const EnforcerBlockerStateMap  &state_map,
                                          bool                            remap_volume_extruders,
                                          size_t                          physical_n,
                                          size_t                          max_filament_id);

void spectrum_physical_remap_apply(Model                          &model,
                                   const EnforcerBlockerStateMap  &state_map,
                                   bool                            remap_volume_extruders,
                                   size_t                          physical_n,
                                   size_t                          max_filament_id);

} // namespace Slic3r
