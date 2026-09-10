#include "SpectrumPhysicalRemap.hpp"

#include "MixedFilament.hpp"
#include "MixedFilamentMatch.hpp"
#include "Model.hpp"

#include <algorithm>
#include <limits>

namespace Slic3r {

namespace {

bool decode_hex_rgb(const std::string &hex, ColorRGB &out)
{
    const std::string norm = normalize_mix_match_hex(hex);
    if (!norm.empty() && decode_color(norm, out))
        return true;
    return decode_color(hex, out);
}

void remap_volume_extruder(ModelObject &obj, ModelVolume &vol, int dest_1based)
{
    const ConfigOption *vol_opt = vol.config.option("extruder");
    if (vol_opt != nullptr && vol_opt->getInt() != 0)
        vol.config.set("extruder", dest_1based);
    else
        obj.config.set("extruder", dest_1based);
}

} // namespace

EnforcerBlockerStateMap spectrum_physical_remap_identity_map()
{
    EnforcerBlockerStateMap state_map;
    constexpr size_t        n = (size_t) EnforcerBlockerType::ExtruderMax + 1;
    for (size_t i = 0; i < n; ++i)
        state_map[i] = static_cast<EnforcerBlockerType>(i);
    return state_map;
}

bool spectrum_model_has_mmu_paint(const Model &model)
{
    for (const ModelObject *obj : model.objects) {
        if (obj == nullptr)
            continue;
        for (const ModelVolume *vol : obj->volumes) {
            if (vol != nullptr && vol->is_model_part() && !vol->mmu_segmentation_facets.empty())
                return true;
        }
    }
    return false;
}

std::vector<int> spectrum_collect_used_facet_states(const Model &model)
{
    std::vector<bool> seen((size_t) EnforcerBlockerType::ExtruderMax + 1, false);
    for (const ModelObject *obj : model.objects) {
        if (obj == nullptr)
            continue;
        for (const ModelVolume *vol : obj->volumes) {
            if (vol == nullptr || !vol->is_model_part() || vol->mmu_segmentation_facets.empty())
                continue;
            const auto used = TriangleSelector::extract_used_facet_states(vol->mmu_segmentation_facets.get_data());
            for (EnforcerBlockerType st : used) {
                const size_t idx = size_t(st);
                if (idx < seen.size())
                    seen[idx] = true;
            }
        }
    }
    std::vector<int> out;
    for (size_t i = 1; i < seen.size(); ++i) {
        if (seen[i])
            out.push_back(int(i));
    }
    return out;
}

bool spectrum_physical_remap_is_mix_recipe_id(size_t id_1based, size_t physical_n, size_t enabled_mix_count)
{
    if (enabled_mix_count == 0 || physical_n == 0 || id_1based <= physical_n)
        return false;
    return id_1based <= physical_n + enabled_mix_count;
}

bool spectrum_physical_remap_is_source_gap_id(size_t id_1based,
                                              size_t physical_n,
                                              size_t source_n,
                                              size_t enabled_mix_count)
{
    if (id_1based <= physical_n)
        return false;
    if (spectrum_physical_remap_is_mix_recipe_id(id_1based, physical_n, enabled_mix_count))
        return false;
    return source_n > 0 && id_1based <= source_n;
}

bool spectrum_physical_remap_is_remappable_source(size_t id_1based,
                                                  size_t physical_n,
                                                  size_t source_n,
                                                  size_t enabled_mix_count)
{
    if (id_1based == 0)
        return false;
    if (id_1based <= physical_n)
        return true;
    return spectrum_physical_remap_is_source_gap_id(id_1based, physical_n, source_n, enabled_mix_count);
}

bool spectrum_physical_remap_hexes_match(const std::string &a, const std::string &b)
{
    ColorRGB ca;
    ColorRGB cb;
    if (!decode_hex_rgb(a, ca) || !decode_hex_rgb(b, cb))
        return false;
    if (ca == cb)
        return true;
    return mixer_delta_e00(ca, cb) < 2.0f;
}

size_t spectrum_physical_remap_nearest_dest(const std::string              &source_hex,
                                            const std::vector<std::string> &dest_hexes)
{
    if (dest_hexes.empty())
        return 1;
    ColorRGB src;
    if (!decode_hex_rgb(source_hex, src))
        return 1;
    float  best = std::numeric_limits<float>::max();
    size_t dest = 1;
    for (size_t i = 0; i < dest_hexes.size(); ++i) {
        ColorRGB d;
        if (!decode_hex_rgb(dest_hexes[i], d))
            continue;
        const float de = mixer_delta_e00(src, d);
        if (de < best) {
            best = de;
            dest = i + 1;
        }
    }
    return dest;
}

bool spectrum_physical_remap_should_skip(const Model &model, const SpectrumPhysicalRemapContext &ctx)
{
    if (!spectrum_model_has_mmu_paint(model))
        return true;

    const std::vector<int> used = spectrum_collect_used_facet_states(model);
    bool                   any_remappable = false;
    for (int id : used) {
        if (!spectrum_physical_remap_is_remappable_source(size_t(id), ctx.physical_n, ctx.source_n,
                                                          ctx.enabled_mix_count))
            continue;
        any_remappable = true;
        if (spectrum_physical_remap_is_source_gap_id(size_t(id), ctx.physical_n, ctx.source_n,
                                                     ctx.enabled_mix_count))
            return false;
        if (ctx.source_hexes.size() < size_t(id))
            return false;
        if (ctx.dest_hexes.size() < size_t(id) || ctx.dest_hexes.size() < ctx.physical_n)
            return false;
        if (!spectrum_physical_remap_hexes_match(ctx.source_hexes[size_t(id) - 1],
                                                 ctx.dest_hexes[size_t(id) - 1]))
            return false;
    }
    if (!any_remappable)
        return true;

    const EnforcerBlockerStateMap seed = spectrum_physical_remap_seed_map(model, ctx);
    for (int id : used) {
        if (!spectrum_physical_remap_is_remappable_source(size_t(id), ctx.physical_n, ctx.source_n,
                                                          ctx.enabled_mix_count))
            continue;
        if (size_t(seed[size_t(id)]) != size_t(id))
            return false;
    }
    return true;
}

EnforcerBlockerStateMap spectrum_physical_remap_seed_map(const Model                         &model,
                                                         const SpectrumPhysicalRemapContext &ctx)
{
    EnforcerBlockerStateMap state_map = spectrum_physical_remap_identity_map();
    const std::vector<int>  used      = spectrum_collect_used_facet_states(model);
    for (int id : used) {
        if (!spectrum_physical_remap_is_remappable_source(size_t(id), ctx.physical_n, ctx.source_n,
                                                          ctx.enabled_mix_count))
            continue;
        std::string src_hex;
        if (size_t(id) <= ctx.source_hexes.size())
            src_hex = ctx.source_hexes[size_t(id) - 1];
        size_t dest = spectrum_physical_remap_nearest_dest(src_hex, ctx.dest_hexes);
        if (ctx.physical_n > 0)
            dest = std::min(dest, ctx.physical_n);
        if (dest == 0)
            dest = 1;
        state_map[size_t(id)] = static_cast<EnforcerBlockerType>(dest);
    }
    return state_map;
}

void spectrum_physical_remap_apply_object(ModelObject                    &obj,
                                          const EnforcerBlockerStateMap  &state_map,
                                          bool                            remap_volume_extruders,
                                          size_t                          physical_n,
                                          size_t                          max_filament_id)
{
    for (ModelVolume *vol : obj.volumes) {
        if (vol == nullptr || !vol->is_model_part())
            continue;
        if (!vol->mmu_segmentation_facets.empty()) {
            TriangleSelector selector(vol->mesh());
            selector.deserialize(vol->mmu_segmentation_facets.get_data(), true, EnforcerBlockerType::ExtruderMax);
            selector.remap_triangle_state(state_map);
            vol->mmu_segmentation_facets.set(selector);
        }
        if (!remap_volume_extruders)
            continue;
        const int current = vol->extruder_id();
        if (current <= 0)
            continue;
        const size_t uid = size_t(current);
        if (uid >= state_map.size())
            continue;
        int dest = int(state_map[uid]);
        if (dest != current)
            remap_volume_extruder(obj, *vol, dest);
        const int after = vol->extruder_id();
        if (!spectrum_volume_extruder_keep(after, physical_n, max_filament_id)) {
            const int fallback = (dest >= 1 && size_t(dest) <= physical_n) ? dest : 1;
            remap_volume_extruder(obj, *vol, fallback);
        }
    }
}

void spectrum_physical_remap_apply(Model                          &model,
                                   const EnforcerBlockerStateMap  &state_map,
                                   bool                            remap_volume_extruders,
                                   size_t                          physical_n,
                                   size_t                          max_filament_id)
{
    for (ModelObject *obj : model.objects) {
        if (obj != nullptr)
            spectrum_physical_remap_apply_object(*obj, state_map, remap_volume_extruders, physical_n,
                                                 max_filament_id);
    }
}

} // namespace Slic3r
