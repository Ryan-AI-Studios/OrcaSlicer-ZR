#include "Exception.hpp"
#include "Print.hpp"

#include <algorithm>

namespace Slic3r {

// 1-based extruder identifier for this region and role.
unsigned int PrintRegion::extruder(FlowRole role) const
{
    size_t extruder = 0;
    if (role == frPerimeter)
        extruder = m_config.inner_wall_filament_id;
    else if (role == frExternalPerimeter)
        extruder = m_config.outer_wall_filament_id;
    else if (role == frInfill)
        extruder = m_config.sparse_infill_filament_id;
    else if (role == frSolidInfill)
        extruder = m_config.internal_solid_filament_id;
    else if (role == frTopSolidInfill)
        extruder = m_config.top_surface_filament_id;
    else
        throw Slic3r::InvalidArgument("Unknown role");
    return extruder;
}

Flow PrintRegion::flow(const PrintObject &object, FlowRole role, double layer_height, bool first_layer) const
{
    const PrintConfig          &print_config = object.print()->config();
    ConfigOptionFloatOrPercent config_width;
    // Get extrusion width from configuration.
    // (might be an absolute value, or a percent value, or zero for auto)
    if (first_layer && print_config.initial_layer_line_width.value > 0) {
        config_width = print_config.initial_layer_line_width;
    } else if (role == frExternalPerimeter) {
        config_width = m_config.outer_wall_line_width;
    } else if (role == frPerimeter) {
        config_width = m_config.inner_wall_line_width;
    } else if (role == frInfill) {
        config_width = m_config.sparse_infill_line_width;
    } else if (role == frSolidInfill) {
        config_width = m_config.internal_solid_infill_line_width;
    } else if (role == frTopSolidInfill) {
        config_width = m_config.top_surface_line_width;
    } else {
        throw Slic3r::InvalidArgument("Unknown role");
    }

    if (config_width.value == 0)
        config_width = object.config().line_width;
    
    // Mix IDs are virtual — map to component_a, then 0030 get_at-equivalent nozzle lookup.
    const size_t       np   = print_config.filament_diameter.size();
    const unsigned int phys = spectrum_physical_for_filament(
        this->extruder(role), np, &object.print()->mixed_filament_manager());
    auto nozzle_diameter = spectrum_nozzle_mm_for_physical(print_config.nozzle_diameter.values, phys);
    return Flow::new_from_config_width(role, config_width, nozzle_diameter, float(layer_height));
}

coordf_t PrintRegion::nozzle_dmr_avg(const PrintConfig &print_config) const
{
    const size_t      np   = print_config.filament_diameter.size();
    const std::string &defs = print_config.mixed_filament_definitions.value;
    auto dmr = [&](int filament_id_1based) {
        const unsigned int phys = spectrum_physical_for_filament(unsigned(std::max(0, filament_id_1based)), np, defs);
        return double(spectrum_nozzle_mm_for_physical(print_config.nozzle_diameter.values, phys));
    };
    return (dmr(m_config.outer_wall_filament_id.value) +
            dmr(m_config.inner_wall_filament_id.value) +
            dmr(m_config.sparse_infill_filament_id.value) +
            dmr(m_config.internal_solid_filament_id.value) +
            dmr(m_config.top_surface_filament_id.value) +
            dmr(m_config.bottom_surface_filament_id.value)) / 6.;
}

coordf_t PrintRegion::bridging_height_avg(const PrintConfig &print_config) const
{
    return this->nozzle_dmr_avg(print_config) * sqrt(m_config.bridge_flow.value);
}

void PrintRegion::collect_object_printing_extruders(const PrintConfig &print_config, const PrintRegionConfig &region_config, const bool has_brim, std::vector<unsigned int> &object_extruders)
{
    // These checks reflect the same logic used in the GUI for enabling/disabling extruder selection fields.
    // Mix IDs expand to physical components; never clamp a virtual ID to Tool 0.
    auto num_extruders = (int)print_config.filament_diameter.size();
    MixedFilamentManager mgr;
    mgr.load_definitions(print_config.mixed_filament_definitions.value);
    auto emplace_extruder = [&](int extruder_id) {
        if (extruder_id <= 0)
            return;
        mgr.append_physical_0based(unsigned(extruder_id), size_t(num_extruders), object_extruders);
    };
    if (region_config.wall_loops.value > 0 || has_brim) {
    	emplace_extruder(region_config.outer_wall_filament_id);
                if (region_config.wall_loops.value > 1)
			emplace_extruder(region_config.inner_wall_filament_id);
    }
    if (region_config.sparse_infill_density.value > 0)
    	emplace_extruder(region_config.sparse_infill_filament_id);
    if (region_config.sparse_infill_density.value > 0 || region_config.top_shell_layers.value > 0 || region_config.bottom_shell_layers.value > 0)
    	emplace_extruder(region_config.internal_solid_filament_id);
    if (region_config.top_shell_layers.value > 0)
    	emplace_extruder(region_config.top_surface_filament_id);
    if (region_config.bottom_shell_layers.value > 0)
    	emplace_extruder(region_config.bottom_surface_filament_id);
}

void PrintRegion::collect_object_printing_extruders(const Print &print, std::vector<unsigned int> &object_extruders) const
{
    // PrintRegion, if used by some PrintObject, shall have all the extruders set to an existing printer extruder
    // or to a virtual mixed filament ID (num_physical+1 ... total_filaments).
    // If not, then there must be something wrong with the Print::apply() function.
#ifndef NDEBUG
    // BBS
    auto num_physical = size_t(print.config().filament_diameter.size());
    auto num_total    = int(print.mixed_filament_manager().total_filaments(num_physical));
    assert(this->config().outer_wall_filament_id    <= num_total);
    assert(this->config().inner_wall_filament_id    <= num_total);
    assert(this->config().sparse_infill_filament_id       <= num_total);
    assert(this->config().internal_solid_filament_id <= num_total);
    assert(this->config().top_surface_filament_id <= num_total);
    assert(this->config().bottom_surface_filament_id <= num_total);
#endif
    collect_object_printing_extruders(print.config(), this->config(), print.has_brim(), object_extruders);
}

}
