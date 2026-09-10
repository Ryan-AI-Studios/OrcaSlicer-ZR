#include "SpectrumPhysicalRemapDialog.hpp"

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "MainFrame.hpp"
#include "Widgets/Label.hpp"

#include "libslic3r/Color.hpp"
#include "libslic3r/MixedFilamentMatch.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace Slic3r {
namespace GUI {

namespace {

ColorRGB decode_hex(const std::string &hex)
{
    ColorRGB          c;
    const std::string norm = normalize_mix_match_hex(hex);
    if ((!norm.empty() && decode_color(norm, c)) || decode_color(hex, c))
        return c;
    return ColorRGB::BLACK();
}

wxPanel *make_swatch(wxWindow *parent, const ColorRGB &c, int dip)
{
    auto *swatch = new wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(dip, dip));
    swatch->SetBackgroundColour(wxColour(c.r_uchar(), c.g_uchar(), c.b_uchar()));
    return swatch;
}

} // namespace

SpectrumPhysicalRemapDialog::SpectrumPhysicalRemapDialog(wxWindow                            *parent,
                                                         const Model                         &model,
                                                         const SpectrumPhysicalRemapContext &ctx)
    : DPIDialog(parent ? parent : static_cast<wxWindow *>(wxGetApp().mainframe),
                wxID_ANY,
                _L("Assign painted colors to loaded filaments"),
                wxDefaultPosition,
                wxDefaultSize,
                wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_ctx(ctx)
{
    SetBackgroundColour(*wxWHITE);
    SetFont(Label::Body_14);

    const std::vector<int> used = spectrum_collect_used_facet_states(model);
    for (int id : used) {
        if (spectrum_physical_remap_is_remappable_source(size_t(id), ctx.physical_n, ctx.source_n,
                                                         ctx.enabled_mix_count))
            m_source_ids.push_back(id);
    }

    const EnforcerBlockerStateMap seed = spectrum_physical_remap_seed_map(model, ctx);

    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *hint = new wxStaticText(this, wxID_ANY,
        _L("Assign each painted color to a currently loaded filament slot. Destinations are loaded filaments only, not mix recipes."));
    hint->Wrap(FromDIP(520));
    root->Add(hint, 0, wxEXPAND | wxALL, FromDIP(12));

    if (ctx.mix_defs_nonempty || ctx.paint_mapped) {
        auto *mix_warn = new wxStaticText(this, wxID_ANY,
            _L("Existing mix recipes still name physical slots. Mix paint IDs will not be remapped."));
        mix_warn->Wrap(FromDIP(520));
        mix_warn->SetForegroundColour(wxColour(0x8A, 0x5A, 0x00));
        root->Add(mix_warn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));
    }

    m_many_warn = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_many_warn->Wrap(FromDIP(520));
    m_many_warn->SetForegroundColour(wxColour(0x8A, 0x5A, 0x00));
    root->Add(m_many_warn, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(12));

    auto *scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    scroll->SetBackgroundColour(*wxWHITE);
    scroll->SetScrollRate(0, FromDIP(16));
    auto *grid = new wxFlexGridSizer(0, 4, FromDIP(6), FromDIP(10));
    grid->AddGrowableCol(3, 1);

    auto add_header = [&](const wxString &text) {
        auto *h = new wxStaticText(scroll, wxID_ANY, text);
        h->SetFont(Label::Head_14);
        grid->Add(h, 0, wxALIGN_CENTER_VERTICAL);
    };
    add_header(_L("ID"));
    add_header(_L("Painted"));
    add_header(wxEmptyString);
    add_header(_L("Loaded filament"));

    const int    swatch_dip = FromDIP(18);
    const size_t dest_n     = ctx.physical_n > 0 ? ctx.physical_n : ctx.dest_hexes.size();
    wxArrayString dest_labels;
    for (size_t d = 1; d <= dest_n; ++d) {
        wxString lab = wxString::Format(_L("Slot %d"), int(d));
        if (d <= ctx.dest_hexes.size())
            lab += "  " + wxString::FromUTF8(ctx.dest_hexes[d - 1].c_str());
        dest_labels.Add(lab);
    }

    for (int id : m_source_ids) {
        auto *id_txt = new wxStaticText(scroll, wxID_ANY, wxString::Format("%d", id));
        grid->Add(id_txt, 0, wxALIGN_CENTER_VERTICAL);

        std::string src_hex;
        if (size_t(id) <= ctx.source_hexes.size())
            src_hex = ctx.source_hexes[size_t(id) - 1];
        grid->Add(make_swatch(scroll, decode_hex(src_hex), swatch_dip), 0, wxALIGN_CENTER_VERTICAL);
        auto *hex_txt = new wxStaticText(scroll, wxID_ANY, wxString::FromUTF8(src_hex.c_str()));
        grid->Add(hex_txt, 0, wxALIGN_CENTER_VERTICAL);

        auto *choice = new wxChoice(scroll, wxID_ANY, wxDefaultPosition, wxDefaultSize, dest_labels);
        size_t seed_dest = size_t(seed[size_t(id)]);
        if (seed_dest < 1 || seed_dest > dest_n)
            seed_dest = 1;
        choice->SetSelection(int(seed_dest - 1));
        choice->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) { refresh_many_to_one_warn(); });
        grid->Add(choice, 0, wxALIGN_CENTER_VERTICAL | wxEXPAND);
        m_dest_choices.push_back(choice);
    }

    scroll->SetSizer(grid);
    scroll->SetMinSize(wxSize(FromDIP(520), FromDIP(220)));
    scroll->SetMaxSize(wxSize(-1, FromDIP(360)));
    root->Add(scroll, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(12));

    auto *btns = new wxBoxSizer(wxHORIZONTAL);
    btns->AddStretchSpacer(1);
    auto *cancel = new wxButton(this, wxID_CANCEL, _L("Cancel"));
    auto *apply  = new wxButton(this, wxID_OK, _L("Apply"));
    btns->Add(cancel, 0, wxRIGHT, FromDIP(8));
    btns->Add(apply, 0);
    root->Add(btns, 0, wxEXPAND | wxALL, FromDIP(12));

    SetSizerAndFit(root);
    CentreOnParent();
    wxGetApp().UpdateDlgDarkUI(this);
    refresh_many_to_one_warn();
}

EnforcerBlockerStateMap SpectrumPhysicalRemapDialog::state_map() const
{
    EnforcerBlockerStateMap map = spectrum_physical_remap_identity_map();
    const size_t            dest_n = m_ctx.physical_n > 0 ? m_ctx.physical_n : m_ctx.dest_hexes.size();
    for (size_t i = 0; i < m_source_ids.size() && i < m_dest_choices.size(); ++i) {
        int sel = m_dest_choices[i]->GetSelection();
        if (sel < 0)
            continue;
        size_t dest = size_t(sel + 1);
        if (dest_n > 0)
            dest = std::min(dest, dest_n);
        map[size_t(m_source_ids[i])] = static_cast<EnforcerBlockerType>(dest);
    }
    return map;
}

void SpectrumPhysicalRemapDialog::refresh_many_to_one_warn()
{
    if (m_many_warn == nullptr)
        return;
    std::set<int> seen;
    bool          many = false;
    for (wxChoice *choice : m_dest_choices) {
        if (choice == nullptr)
            continue;
        const int sel = choice->GetSelection();
        if (sel < 0)
            continue;
        if (!seen.insert(sel).second)
            many = true;
    }
    m_many_warn->SetLabel(many ? _L("Two painted colors map to the same loaded filament.") : wxString());
    m_many_warn->Wrap(FromDIP(520));
    Layout();
}

void SpectrumPhysicalRemapDialog::on_dpi_changed(const wxRect &)
{
    Layout();
    Fit();
}

} // namespace GUI
} // namespace Slic3r
