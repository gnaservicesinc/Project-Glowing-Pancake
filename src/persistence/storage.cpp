#include <pgp/persistence/storage.hpp>

#include <cerrno>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace pgp::persistence {
namespace {
[[noreturn]] void io_error(std::string_view operation) {
#ifdef _WIN32
    const auto error=std::error_code(static_cast<int>(GetLastError()),std::system_category());
#else
    const auto error=std::error_code(errno,std::generic_category());
#endif
    throw PersistenceError("io",std::string(operation)+": "+error.message());
}
std::filesystem::file_status local_status(const std::filesystem::path& path) {
    std::error_code error;
    const auto status=std::filesystem::symlink_status(path,error);
    if (error && error!=std::errc::no_such_file_or_directory)
        throw PersistenceError("io","Cannot inspect local file: "+error.message());
    return status;
}
bool existing_regular(const std::filesystem::path& path) {
    const auto status=local_status(path);
    if (!std::filesystem::exists(status)) return false;
    if (!std::filesystem::is_regular_file(status))
        throw PersistenceError("file_type","Save/settings paths must be regular files, not links or directories");
    return true;
}
std::filesystem::path sibling(const std::filesystem::path& path,std::string_view suffix) {
    auto result=path;result+=suffix;return result;
}
void write_exclusive(const std::filesystem::path& path,std::string_view bytes) {
#ifdef _WIN32
    const auto handle=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,
                                 FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr);
    if (handle==INVALID_HANDLE_VALUE) io_error("Cannot create temporary sibling");
    bool opened=true;
    try {
        std::size_t offset=0;
        while (offset<bytes.size()) {
            const auto count=static_cast<DWORD>(std::min<std::size_t>(bytes.size()-offset,65536));
            DWORD written=0;
            if (!WriteFile(handle,bytes.data()+offset,count,&written,nullptr) || written==0)
                io_error("Cannot write temporary sibling");
            offset+=written;
        }
        if (!FlushFileBuffers(handle)) io_error("Cannot flush temporary sibling");
        opened=false;
        if (!CloseHandle(handle)) io_error("Cannot close temporary sibling");
    } catch (...) {
        if (opened) CloseHandle(handle);
        std::error_code ignored;std::filesystem::remove(path,ignored);throw;
    }
#else
    int flags=O_WRONLY|O_CREAT|O_EXCL;
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
    const int descriptor=::open(path.c_str(),flags,0600);
    if (descriptor<0) io_error("Cannot create temporary sibling");
    bool opened=true;
    try {
        std::size_t offset=0;
        while (offset<bytes.size()) {
            const auto written=::write(descriptor,bytes.data()+offset,bytes.size()-offset);
            if (written<0 && errno==EINTR) continue;
            if (written<=0) io_error("Cannot write temporary sibling");
            offset+=static_cast<std::size_t>(written);
        }
        if (::fsync(descriptor)!=0) io_error("Cannot flush temporary sibling");
        opened=false;
        if (::close(descriptor)!=0) io_error("Cannot close temporary sibling");
    } catch (...) {
        if (opened) ::close(descriptor);
        std::error_code ignored;std::filesystem::remove(path,ignored);throw;
    }
#endif
}
void replace(const std::filesystem::path& temporary,const std::filesystem::path& target) {
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        io_error("Cannot atomically replace local file");
#else
    if (::rename(temporary.c_str(),target.c_str())!=0) io_error("Cannot atomically replace local file");
#endif
}
void sync_directory(const std::filesystem::path& directory) {
#ifndef _WIN32
    int flags=O_RDONLY;
#ifdef O_DIRECTORY
    flags|=O_DIRECTORY;
#endif
#ifdef O_CLOEXEC
    flags|=O_CLOEXEC;
#endif
    const int descriptor=::open(directory.c_str(),flags);
    if (descriptor<0) io_error("Cannot open save directory for flush");
    const auto result=::fsync(descriptor);
    const auto saved_errno=errno;
    ::close(descriptor);
    if (result!=0) {errno=saved_errno;io_error("Cannot flush save directory");}
#else
    (void)directory; // MOVEFILE_WRITE_THROUGH flushes the same-volume rename.
#endif
}
}

PersistenceError::PersistenceError(std::string code,std::string detail)
    :std::runtime_error(detail),code_(std::move(code)) {}

std::string read_bounded(const std::filesystem::path& path,std::size_t maximum_bytes) {
    if (maximum_bytes==0 || maximum_bytes>64U*1024U*1024U)
        throw PersistenceError("limit","Local file bound must be between one byte and 64 MiB");
    if (!existing_regular(path)) throw PersistenceError("missing_file","The selected local file does not exist");
    std::ifstream input(path,std::ios::binary);
    if (!input) throw PersistenceError("io","Cannot open selected local file");
    std::string result(maximum_bytes+1,'\0');
    input.read(result.data(),static_cast<std::streamsize>(result.size()));
    if (input.bad()) throw PersistenceError("io","Cannot read selected local file");
    result.resize(static_cast<std::size_t>(input.gcount()));
    if (result.size()>maximum_bytes) throw PersistenceError("oversized","The local file exceeds its configured size bound");
    return result;
}

void atomic_write(const std::filesystem::path& path,std::string_view bytes,
                  const std::function<void(std::string_view)>& validate_existing,std::size_t maximum_bytes,
                  std::optional<bool> expected_exists) {
    if (bytes.size()>maximum_bytes || maximum_bytes==0 || maximum_bytes>64U*1024U*1024U)
        throw PersistenceError("oversized","New data exceeds the configured local file bound");
    if (path.empty() || path.filename().empty()) throw PersistenceError("path","A local filename is required");
    const auto parent=path.parent_path().empty()?std::filesystem::path{"."}:path.parent_path();
    const auto temporary=sibling(path,".tmp");
    const auto backup=sibling(path,".bak");
    const auto backup_temporary=sibling(path,".bak.tmp");
    std::optional<std::string> prior;
    const bool exists=existing_regular(path);
    if (expected_exists && *expected_exists!=exists)
        throw PersistenceError("conflict","The selected local file appeared or was removed since it was opened");
    if (exists) {prior=read_bounded(path,maximum_bytes);validate_existing(*prior);}
    if (std::filesystem::exists(local_status(temporary)) || std::filesystem::exists(local_status(backup_temporary)))
        throw PersistenceError("pending_write","A temporary sibling already exists; it was preserved for inspection");
    (void)existing_regular(backup);
    std::error_code error;
    std::filesystem::create_directories(parent,error);
    if (error) throw PersistenceError("io","Cannot create local save directory: "+error.message());
    bool owns_temporary=false;
    bool owns_backup_temporary=false;
    try {
        // Ownership begins after successful exclusive creation. write_exclusive
        // cleans no pre-existing path; the cleanup below removes only ours.
        write_exclusive(temporary,bytes);
        owns_temporary=true;
        if (prior) {
            if (read_bounded(path,maximum_bytes)!=*prior)
                throw PersistenceError("conflict","The local file changed while its replacement was prepared");
            write_exclusive(backup_temporary,*prior);
            owns_backup_temporary=true;
            replace(backup_temporary,backup);owns_backup_temporary=false;
            sync_directory(parent);
            if (expected_exists && (!existing_regular(path) || read_bounded(path,maximum_bytes)!=*prior))
                throw PersistenceError("conflict","The local file changed while its recovery copy was prepared");
        } else if (existing_regular(path)) {
            throw PersistenceError("conflict","A local file appeared while its replacement was prepared");
        }
        replace(temporary,path);owns_temporary=false;
        sync_directory(parent);
    } catch (...) {
        if (owns_temporary) std::filesystem::remove(temporary,error);
        if (owns_backup_temporary) std::filesystem::remove(backup_temporary,error);
        throw;
    }
}

std::filesystem::path standard_data_directory() {
#ifdef _WIN32
    const auto required=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);
    if (required==0) throw PersistenceError("data_directory","LOCALAPPDATA is unavailable; select a portable save directory");
    std::wstring configured(required,L'\0');
    const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",configured.data(),required);
    if (count==0 || count>=required) throw PersistenceError("data_directory","Cannot read the local data directory");
    configured.resize(count);
    return std::filesystem::path{configured}/"Project Glowing Pancake";
#else
    const auto environment=[](const char* key)->std::filesystem::path {
        const auto* value=std::getenv(key);return value&&*value?std::filesystem::path{value}:std::filesystem::path{};
    };
#ifdef __APPLE__
    auto base=environment("HOME");
    if (base.empty()) throw PersistenceError("data_directory","The local home directory is unavailable; select a portable save directory");
    return base/"Library"/"Application Support"/"Project Glowing Pancake";
#else
    auto base=environment("XDG_DATA_HOME");
    if (base.empty() || !base.is_absolute()) {
        base=environment("HOME");
        if (base.empty()) throw PersistenceError("data_directory","The local data directory is unavailable; select a portable save directory");
        base/=".local";base/="share";
    }
    return base/"project-glowing-pancake";
#endif
#endif
}
std::filesystem::path standard_settings_directory() {
#if defined(_WIN32) || defined(__APPLE__)
    return standard_data_directory();
#else
    const auto* configured=std::getenv("XDG_CONFIG_HOME");
    auto base=configured&&*configured?std::filesystem::path{configured}:std::filesystem::path{};
    if (base.empty() || !base.is_absolute()) {
        const auto* directory=std::getenv("HOME");
        if (!directory || !*directory) throw PersistenceError("data_directory","Select a portable settings directory");
        base=std::filesystem::path{directory}/".config";
    }
    return base/"project-glowing-pancake";
#endif
}
LocalPaths local_paths(std::optional<std::filesystem::path> portable_root) {
    if (portable_root) {
        if (portable_root->empty()) throw PersistenceError("path","Portable root cannot be empty");
        return {*portable_root/"saves",*portable_root/"settings.json"};
    }
    return {standard_data_directory()/"saves",standard_settings_directory()/"settings.json"};
}
}
