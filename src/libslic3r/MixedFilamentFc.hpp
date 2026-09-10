// Optional FilamentColors.xyz Lab/TD overlay (0068). wx-free.
#pragma once

#include "MixedFilamentOfd.hpp"
#include "MixedFilamentSwatch.hpp"

#include <optional>
#include <string>
#include <vector>

namespace Slic3r {

struct SpectrumFcSwatch {
    std::string          manufacturer;
    std::string          color_name;
    std::string          coarse;
    std::string          hex;
    double               L = 0;
    double               a = 0;
    double               b = 0;
    std::optional<float> td;
};

struct SpectrumFcOverlaySlot {
    int                  slot = 0;
    std::string          key;
    double               lab_l = 0;
    double               lab_a = 0;
    double               lab_b = 0;
    std::optional<float> td;
};

struct SpectrumFcOverlayStore {
    int                                 version = 1;
    std::vector<SpectrumFcOverlaySlot>  slots;
};

std::string spectrum_fc_normalize_hex(const std::string &raw);
std::string spectrum_fc_norm(const std::string &s);
std::string spectrum_fc_coarse_type(const std::string &material_or_filament);
std::string spectrum_fc_match_key(const std::string &brand,
                                  const std::string &color,
                                  const std::string &material);

// Never throws. `results[]` page or a single swatch object. Skip rows without Lab.
std::vector<SpectrumFcSwatch> spectrum_fc_parse_swatch_list(const std::string &json);

// count==1 and results[0].id → id; else nullopt. Never throws.
std::optional<int> spectrum_fc_parse_manufacturer_id(const std::string &json);
// Paginated list `next` URL, or empty.
std::string spectrum_fc_parse_next_url(const std::string &json);

// Conservative key: 0 or >1 hits → nullopt.
std::optional<SpectrumFcSwatch> spectrum_fc_match(const SpectrumOfdVariant              &pick,
                                                  const std::vector<SpectrumFcSwatch> &page);

SpectrumFcOverlayStore spectrum_fc_store_parse(const std::string &json);
std::string            spectrum_fc_store_serialize(const SpectrumFcOverlayStore &store);

// dir empty → data_dir()/spectrum/filamentcolors_overlay.json. Never throws.
// Missing file → empty store.
SpectrumFcOverlayStore spectrum_fc_load(const std::string &dir = {});
bool                   spectrum_fc_save(const SpectrumFcOverlayStore &store, const std::string &dir = {});

void spectrum_fc_upsert_slot(SpectrumFcOverlayStore &store, const SpectrumFcOverlaySlot &slot);

// Size n. 0.f where that physical has no overlay TD.
std::vector<float> spectrum_fc_overlay_td_vector(const SpectrumFcOverlayStore &store, size_t n);

// If any overlay TD: seed missing from Panchroma cards (when physicals match), else 0.
// No overlay TD → nullptr (caller keeps card / default path).
const std::vector<float> *spectrum_fc_td_for_match(const SpectrumFcOverlayStore   &store,
                                                   const std::vector<ColorRGB>    &physicals,
                                                   std::vector<float>             &storage);

// Copy owner when present. Add "P{n}" Lab only when that recipe is absent. No TD. No batch_key change.
SwatchLUT spectrum_fc_fallback_lut(const SwatchLUT                     *owner,
                                   const SpectrumFcOverlayStore        &store);

// Process-wide cache for Match / bake / PicPrint (already-loaded; no fetch).
void                           spectrum_fc_set_session_store(const SpectrumFcOverlayStore &store);
const SpectrumFcOverlayStore  &spectrum_fc_session_store();
void                           spectrum_fc_ensure_session_loaded();

} // namespace Slic3r
