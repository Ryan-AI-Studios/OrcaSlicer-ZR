#pragma once

#include "GUI_Utils.hpp"
#include "libslic3r/SpectrumPhysicalRemap.hpp"

#include <vector>

class wxChoice;
class wxStaticText;

namespace Slic3r {
class Model;

namespace GUI {

class SpectrumPhysicalRemapDialog : public DPIDialog
{
public:
    SpectrumPhysicalRemapDialog(wxWindow                            *parent,
                                const Model                         &model,
                                const SpectrumPhysicalRemapContext &ctx);
    ~SpectrumPhysicalRemapDialog() override = default;

    EnforcerBlockerStateMap state_map() const;

protected:
    void on_dpi_changed(const wxRect &suggested_rect) override;

private:
    void refresh_many_to_one_warn();

    SpectrumPhysicalRemapContext m_ctx;
    std::vector<int>             m_source_ids;
    std::vector<wxChoice *>      m_dest_choices;
    wxStaticText                *m_many_warn{nullptr};
};

} // namespace GUI
} // namespace Slic3r
