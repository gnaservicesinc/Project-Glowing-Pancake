#include <pgp/persistence/settings.hpp>
#include "json_support.hpp"

namespace pgp::persistence {
namespace {
constexpr std::size_t maximum_settings_bytes=16384;
void validate(const Settings& settings) {
    if (settings.text_scale_percent<50 || settings.text_scale_percent>300 || settings.volume_percent>100)
        throw PersistenceError("settings_range","Text scale must be 50-300 percent and volume 0-100 percent");
}
}
std::string encode_settings(const Settings& settings) {
    validate(settings);
    return detail::Json{{"settings_format",1},{"autosave_enabled",settings.autosave_enabled},
        {"reduced_motion",settings.reduced_motion},{"auto_pause_alerts",settings.auto_pause_alerts},
        {"compact_management",settings.compact_management},{"text_scale_percent",settings.text_scale_percent},
        {"auto_pause_projects",settings.auto_pause_projects},{"auto_pause_evaluations",settings.auto_pause_evaluations},
        {"volume_percent",settings.volume_percent},{"music_enabled",settings.music_enabled},{"show_guide",settings.show_guide}}.dump()+"\n";
}
Settings decode_settings(std::string_view bytes) {
    const auto value=detail::parse(bytes,maximum_settings_bytes);
    detail::fields(value,"settings",{"settings_format","autosave_enabled","reduced_motion","auto_pause_alerts",
                                    "compact_management","text_scale_percent","volume_percent"},
                                    {"show_guide","auto_pause_projects","auto_pause_evaluations","music_enabled"});
    if (detail::u64(value.at("settings_format"),"settings_format")!=1)
        throw PersistenceError("future_settings","Unsupported settings format; original file was preserved");
    Settings result;
    if (value.contains("music_enabled")) result.music_enabled=detail::boolean(value.at("music_enabled"),"music_enabled");
    if (value.contains("auto_pause_projects")) result.auto_pause_projects=detail::boolean(value.at("auto_pause_projects"),"auto_pause_projects");
    if (value.contains("auto_pause_evaluations")) result.auto_pause_evaluations=detail::boolean(value.at("auto_pause_evaluations"),"auto_pause_evaluations");
    if (value.contains("show_guide")) result.show_guide=detail::boolean(value.at("show_guide"),"show_guide");
    result.autosave_enabled=detail::boolean(value.at("autosave_enabled"),"autosave_enabled");
    result.reduced_motion=detail::boolean(value.at("reduced_motion"),"reduced_motion");
    result.auto_pause_alerts=detail::boolean(value.at("auto_pause_alerts"),"auto_pause_alerts");
    result.compact_management=detail::boolean(value.at("compact_management"),"compact_management");
    result.text_scale_percent=static_cast<std::uint32_t>(detail::u64(value.at("text_scale_percent"),"text_scale_percent",300));
    result.volume_percent=static_cast<std::uint32_t>(detail::u64(value.at("volume_percent"),"volume_percent",100));
    validate(result);return result;
}
void save_settings(const std::filesystem::path& path,const Settings& settings) {
    atomic_write(path,encode_settings(settings),[](auto prior){(void)decode_settings(prior);},maximum_settings_bytes);
}
Settings load_settings(const std::filesystem::path& path) {
    std::error_code error;
    const auto status=std::filesystem::symlink_status(path,error);
    if (error==std::errc::no_such_file_or_directory || (!error&&!std::filesystem::exists(status))) return {};
    if (error) throw PersistenceError("io","Cannot inspect local settings: "+error.message());
    return decode_settings(read_bounded(path,maximum_settings_bytes));
}
}
