#include "MixedFilamentFc.hpp"

#include "MixedFilamentMatch.hpp"
#include "Utils.hpp"
#include "nlohmann/json.hpp"

#include <cctype>
#include <sstream>

#include <boost/filesystem.hpp>
#include <boost/nowide/fstream.hpp>

namespace Slic3r {

namespace {

SpectrumFcOverlayStore g_session;
bool                   g_session_loaded = false;

std::string trim_copy(const std::string &s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

std::string first_token(const std::string &s)
{
    const std::string t = trim_copy(s);
    size_t            i = 0;
    while (i < t.size() && !std::isspace(static_cast<unsigned char>(t[i])))
        ++i;
    return t.substr(0, i);
}

std::string json_string_field(const nlohmann::json &j, const char *key)
{
    if (!j.is_object() || !j.contains(key))
        return {};
    const nlohmann::json &v = j[key];
    if (v.is_string())
        return v.get<std::string>();
    return {};
}

bool json_double_field(const nlohmann::json &j, const char *key, double &out)
{
    if (!j.is_object() || !j.contains(key) || j[key].is_null())
        return false;
    const nlohmann::json &v = j[key];
    if (v.is_number()) {
        out = v.get<double>();
        return true;
    }
    if (v.is_string()) {
        try {
            out = std::stod(v.get<std::string>());
            return true;
        } catch (...) {
            return false;
        }
    }
    return false;
}

std::optional<float> json_optional_float(const nlohmann::json &j, const char *key)
{
    if (!j.is_object() || !j.contains(key) || j[key].is_null())
        return std::nullopt;
    const nlohmann::json &v = j[key];
    if (v.is_number())
        return static_cast<float>(v.get<double>());
    if (v.is_string()) {
        try {
            return static_cast<float>(std::stod(v.get<std::string>()));
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

std::string fc_material_from_swatch(const nlohmann::json &j)
{
    if (j.is_object() && j.contains("filament_type") && j["filament_type"].is_object()) {
        const nlohmann::json &ft = j["filament_type"];
        if (ft.contains("parent_type") && ft["parent_type"].is_object()) {
            const std::string parent = json_string_field(ft["parent_type"], "name");
            if (!parent.empty())
                return parent;
        }
        const std::string name = json_string_field(ft, "name");
        if (!name.empty())
            return name;
    }
    return {};
}

std::string manufacturer_name(const nlohmann::json &j)
{
    if (j.is_object() && j.contains("manufacturer") && j["manufacturer"].is_object())
        return json_string_field(j["manufacturer"], "name");
    return json_string_field(j, "manufacturer");
}

bool parse_one_swatch(const nlohmann::json &j, SpectrumFcSwatch &out)
{
    if (!j.is_object())
        return false;
    double L = 0, a = 0, b = 0;
    if (!json_double_field(j, "lab_l", L) || !json_double_field(j, "lab_a", a) ||
        !json_double_field(j, "lab_b", b))
        return false;
    SpectrumFcSwatch s;
    s.manufacturer = manufacturer_name(j);
    s.color_name   = json_string_field(j, "color_name");
    s.coarse       = spectrum_fc_coarse_type(fc_material_from_swatch(j));
    s.hex          = spectrum_fc_normalize_hex(json_string_field(j, "hex_color"));
    s.L            = L;
    s.a            = a;
    s.b            = b;
    s.td           = json_optional_float(j, "td");
    out            = std::move(s);
    return true;
}

boost::filesystem::path overlay_dir(const std::string &dir)
{
    if (!dir.empty())
        return boost::filesystem::path(dir);
    const std::string &root = data_dir();
    if (root.empty())
        return {};
    return boost::filesystem::path(root) / "spectrum";
}

boost::filesystem::path overlay_path(const std::string &dir)
{
    const boost::filesystem::path folder = overlay_dir(dir);
    if (folder.empty())
        return {};
    return folder / "filamentcolors_overlay.json";
}

std::string read_file_or_empty(const boost::filesystem::path &path)
{
    if (path.empty())
        return {};
    boost::nowide::ifstream ifs(path.string(), std::ios::binary);
    if (!ifs)
        return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

bool write_file(const boost::filesystem::path &path, const std::string &body)
{
    try {
        if (path.empty())
            return false;
        boost::system::error_code ec;
        boost::filesystem::create_directories(path.parent_path(), ec);
        if (ec)
            return false;
        boost::nowide::ofstream ofs(path.string(), std::ios::binary | std::ios::trunc);
        if (!ofs)
            return false;
        ofs << body;
        return ofs.good();
    } catch (...) {
        return false;
    }
}

void seed_card_tds(const std::vector<ColorRGB> &physicals, std::vector<float> &td)
{
    const size_t n = td.size();
    if (n == 0)
        return;
    const SpectrumPhysicalCard cmyk = spectrum_panchroma_cmyk_card();
    const SpectrumPhysicalCard rgbw = spectrum_panchroma_rgbw_card();
    const SpectrumPhysicalCard *card = nullptr;
    if (spectrum_physicals_match_card(physicals, cmyk))
        card = &cmyk;
    else if (spectrum_physicals_match_card(physicals, rgbw))
        card = &rgbw;
    if (card == nullptr)
        return;
    for (size_t i = 0; i < n && i < 4; ++i) {
        if (td[i] <= 0.f)
            td[i] = card->td[i];
    }
}

} // namespace

std::string spectrum_fc_normalize_hex(const std::string &raw)
{
    std::string s = trim_copy(raw);
    if (s.empty())
        return {};
    if (s[0] == '#')
        s.erase(s.begin());
    if (s.size() > 6)
        s = s.substr(0, 6);
    if (s.size() != 6)
        return {};
    for (char &c : s) {
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return {};
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return std::string("#") + s;
}

std::string spectrum_fc_norm(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c))
            out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string spectrum_fc_coarse_type(const std::string &material_or_filament)
{
    std::string token = first_token(material_or_filament);
    if (token.empty())
        token = material_or_filament;
    const std::string n = spectrum_fc_norm(token);
    if (n.compare(0, 3, "pla") == 0)
        return "PLA";
    if (n.compare(0, 4, "petg") == 0)
        return "PETG";
    if (n.compare(0, 3, "abs") == 0 || n.compare(0, 3, "asa") == 0)
        return "ABS";
    if (n.compare(0, 3, "tpu") == 0 || n.compare(0, 3, "tpe") == 0)
        return "TPU";
    const std::string all = spectrum_fc_norm(material_or_filament);
    if (all.compare(0, 3, "pla") == 0)
        return "PLA";
    if (all.compare(0, 4, "petg") == 0)
        return "PETG";
    return "OTHER";
}

std::string spectrum_fc_match_key(const std::string &brand,
                                  const std::string &color,
                                  const std::string &material)
{
    return spectrum_fc_norm(brand) + "|" + spectrum_fc_coarse_type(material) + "|" +
           spectrum_fc_norm(color);
}

std::vector<SpectrumFcSwatch> spectrum_fc_parse_swatch_list(const std::string &json)
{
    std::vector<SpectrumFcSwatch> out;
    const std::string             trimmed = trim_copy(json);
    if (trimmed.empty())
        return out;
    try {
        nlohmann::json j = nlohmann::json::parse(trimmed, nullptr, false);
        if (j.is_discarded())
            return out;
        auto push = [&](const nlohmann::json &item) {
            SpectrumFcSwatch s;
            if (parse_one_swatch(item, s))
                out.push_back(std::move(s));
        };
        if (j.is_object() && j.contains("results") && j["results"].is_array()) {
            for (const nlohmann::json &item : j["results"])
                push(item);
        } else if (j.is_array()) {
            for (const nlohmann::json &item : j)
                push(item);
        } else if (j.is_object()) {
            push(j);
        }
    } catch (...) {
        out.clear();
    }
    return out;
}

std::optional<int> spectrum_fc_parse_manufacturer_id(const std::string &json)
{
    const std::string trimmed = trim_copy(json);
    if (trimmed.empty())
        return std::nullopt;
    try {
        nlohmann::json j = nlohmann::json::parse(trimmed, nullptr, false);
        if (j.is_discarded() || !j.is_object())
            return std::nullopt;
        int count = 0;
        if (j.contains("count") && j["count"].is_number_integer())
            count = j["count"].get<int>();
        else if (j.contains("results") && j["results"].is_array())
            count = static_cast<int>(j["results"].size());
        if (count != 1 || !j.contains("results") || !j["results"].is_array() || j["results"].empty())
            return std::nullopt;
        const nlohmann::json &row = j["results"][0];
        if (!row.is_object() || !row.contains("id") || !row["id"].is_number_integer())
            return std::nullopt;
        return row["id"].get<int>();
    } catch (...) {
        return std::nullopt;
    }
}

std::string spectrum_fc_parse_next_url(const std::string &json)
{
    const std::string trimmed = trim_copy(json);
    if (trimmed.empty())
        return {};
    try {
        nlohmann::json j = nlohmann::json::parse(trimmed, nullptr, false);
        if (j.is_discarded() || !j.is_object() || !j.contains("next") || j["next"].is_null())
            return {};
        if (j["next"].is_string())
            return j["next"].get<std::string>();
    } catch (...) {}
    return {};
}

std::optional<SpectrumFcSwatch> spectrum_fc_match(const SpectrumOfdVariant              &pick,
                                                  const std::vector<SpectrumFcSwatch> &page)
{
    const std::string material = !pick.material.empty() ? pick.material : pick.filament;
    const std::string want     = spectrum_fc_match_key(pick.brand, pick.variant, material);
    if (want.size() < 3)
        return std::nullopt;
    const SpectrumFcSwatch *hit = nullptr;
    int                     n   = 0;
    for (const SpectrumFcSwatch &s : page) {
        const std::string key = spectrum_fc_match_key(s.manufacturer, s.color_name,
                                                      s.coarse == "OTHER" ? std::string() : s.coarse);
        if (key != want)
            continue;
        ++n;
        hit = &s;
        if (n > 1)
            return std::nullopt;
    }
    if (n != 1 || hit == nullptr)
        return std::nullopt;
    return *hit;
}

SpectrumFcOverlayStore spectrum_fc_store_parse(const std::string &json)
{
    SpectrumFcOverlayStore        out;
    const std::string             trimmed = trim_copy(json);
    if (trimmed.empty())
        return out;
    try {
        nlohmann::json j = nlohmann::json::parse(trimmed, nullptr, false);
        if (j.is_discarded() || !j.is_object())
            return {};
        if (j.contains("version") && j["version"].is_number_integer())
            out.version = j["version"].get<int>();
        if (!j.contains("slots") || !j["slots"].is_array())
            return out;
        for (const nlohmann::json &row : j["slots"]) {
            if (!row.is_object())
                continue;
            SpectrumFcOverlaySlot s;
            if (row.contains("slot") && row["slot"].is_number_integer())
                s.slot = row["slot"].get<int>();
            s.key = json_string_field(row, "key");
            if (!json_double_field(row, "lab_l", s.lab_l) || !json_double_field(row, "lab_a", s.lab_a) ||
                !json_double_field(row, "lab_b", s.lab_b))
                continue;
            s.td = json_optional_float(row, "td");
            out.slots.push_back(std::move(s));
        }
    } catch (...) {
        return {};
    }
    return out;
}

std::string spectrum_fc_store_serialize(const SpectrumFcOverlayStore &store)
{
    nlohmann::json j = nlohmann::json::object();
    j["version"]     = store.version;
    nlohmann::json arr = nlohmann::json::array();
    for (const SpectrumFcOverlaySlot &s : store.slots) {
        nlohmann::json o = nlohmann::json::object();
        o["slot"]        = s.slot;
        o["key"]         = s.key;
        o["lab_l"]       = s.lab_l;
        o["lab_a"]       = s.lab_a;
        o["lab_b"]       = s.lab_b;
        if (s.td.has_value())
            o["td"] = *s.td;
        arr.push_back(std::move(o));
    }
    j["slots"] = std::move(arr);
    return j.dump(2);
}

SpectrumFcOverlayStore spectrum_fc_load(const std::string &dir)
{
    try {
        return spectrum_fc_store_parse(read_file_or_empty(overlay_path(dir)));
    } catch (...) {
        return {};
    }
}

bool spectrum_fc_save(const SpectrumFcOverlayStore &store, const std::string &dir)
{
    try {
        return write_file(overlay_path(dir), spectrum_fc_store_serialize(store));
    } catch (...) {
        return false;
    }
}

void spectrum_fc_upsert_slot(SpectrumFcOverlayStore &store, const SpectrumFcOverlaySlot &slot)
{
    for (SpectrumFcOverlaySlot &s : store.slots) {
        if (s.slot == slot.slot) {
            s = slot;
            return;
        }
    }
    store.slots.push_back(slot);
}

std::vector<float> spectrum_fc_overlay_td_vector(const SpectrumFcOverlayStore &store, size_t n)
{
    std::vector<float> out(n, 0.f);
    for (const SpectrumFcOverlaySlot &s : store.slots) {
        if (s.slot < 0 || size_t(s.slot) >= n || !s.td.has_value())
            continue;
        out[size_t(s.slot)] = *s.td;
    }
    return out;
}

const std::vector<float> *spectrum_fc_td_for_match(const SpectrumFcOverlayStore &store,
                                                   const std::vector<ColorRGB>  &physicals,
                                                   std::vector<float>           &storage)
{
    const size_t n = physicals.size();
    bool         any = false;
    for (const SpectrumFcOverlaySlot &s : store.slots) {
        if (s.td.has_value() && s.slot >= 0 && size_t(s.slot) < n)
            any = true;
    }
    if (!any)
        return nullptr;
    storage = spectrum_fc_overlay_td_vector(store, n);
    seed_card_tds(physicals, storage);
    return &storage;
}

SwatchLUT spectrum_fc_fallback_lut(const SwatchLUT              *owner,
                                   const SpectrumFcOverlayStore &store)
{
    SwatchLUT out;
    if (owner != nullptr)
        out = *owner;
    for (const SpectrumFcOverlaySlot &s : store.slots) {
        if (s.slot < 0)
            continue;
        const std::string recipe = "P" + std::to_string(s.slot + 1);
        if (out.find_recipe(recipe) != nullptr)
            continue;
        SwatchLUTEntry e;
        e.recipe = recipe;
        e.L      = s.lab_l;
        e.a      = s.lab_a;
        e.b      = s.lab_b;
        out.entries.push_back(std::move(e));
    }
    return out;
}

void spectrum_fc_set_session_store(const SpectrumFcOverlayStore &store)
{
    g_session        = store;
    g_session_loaded = true;
}

const SpectrumFcOverlayStore &spectrum_fc_session_store()
{
    return g_session;
}

void spectrum_fc_ensure_session_loaded()
{
    if (g_session_loaded)
        return;
    g_session        = spectrum_fc_load();
    g_session_loaded = true;
}

} // namespace Slic3r
