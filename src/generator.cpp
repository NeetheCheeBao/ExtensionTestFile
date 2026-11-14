#include "generator.hpp"

#include <windows.h>

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace etf {
namespace {

constexpr wchar_t kFilePrefix[] = L"FILE.";
constexpr char kFileContent[] = "EEEEE";
// 内容长度须与 kFileContentSize 一致，不一致时编译失败
static_assert(sizeof(kFileContent) - 1 == kFileContentSize, "文件内容必须为 5 字节");

const wchar_t* const kVideoExtensions[] = {
    L"avi", L"wmv", L"wmp", L"wm", L"asf", L"mpg", L"mpeg", L"mpe", L"m1v",
    L"m2v", L"mpv2", L"mp2v", L"ts", L"tp", L"tpr", L"trp", L"vob", L"ifo",
    L"ogm", L"ogv", L"mp4", L"m4v", L"m4p", L"m4b", L"3gp", L"3gpp", L"3g2",
    L"3gp2", L"mkv", L"rm", L"ram", L"rmvb", L"rpm", L"flv", L"mov", L"qt",
    L"nsv", L"dpg", L"m2ts", L"m2t", L"mts", L"dvr-ms", L"k3g", L"skm",
    L"evo", L"nsr", L"amv", L"divx", L"webm", L"wtv", L"f4v", L"mxf"
};

const wchar_t* const kAudioExtensions[] = {
    L"wav", L"wma", L"mpa", L"mp2", L"m1a", L"m2a", L"mp3", L"ogg", L"m4a",
    L"aac", L"mka", L"ra", L"flac", L"ape", L"mpc", L"mod", L"ac3", L"eac3",
    L"dts", L"dtshd", L"wv", L"tak", L"cda", L"dsf", L"tta", L"aiff", L"aif",
    L"opus", L"amr"
};

const wchar_t* const kPlaylistExtensions[] = {
    L"asx", L"m3u", L"m3u8", L"pls", L"wvx", L"wax", L"wmx", L"cue", L"mpls",
    L"mpl", L"dpl", L"xspf", L"mpd"
};

// 常量数组转为 vector，供“全部”分类在运行时汇总
std::vector<std::wstring> MakeList(const wchar_t* const* items, std::size_t count) {
    std::vector<std::wstring> list;
    list.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        list.push_back(items[i]);
    }
    return list;
}

// CREATE_ALWAYS 覆盖同名文件，重复运行结果一致
bool WriteFileContent(const std::wstring& file) {
    HANDLE handle = ::CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    DWORD written = 0;
    const BOOL ok = ::WriteFile(handle, kFileContent, static_cast<DWORD>(kFileContentSize),
                                &written, nullptr);
    ::CloseHandle(handle);
    return ok != FALSE && written == kFileContentSize;
}

}

// 目录已带分隔符时不再重复拼接
std::wstring JoinPath(const std::wstring& directory, const std::wstring& name) {
    if (directory.empty()) {
        return name;
    }
    const wchar_t last = directory[directory.size() - 1];
    if (last == L'\\' || last == L'/') {
        return directory + name;
    }
    return directory + L"\\" + name;
}

// CreateDirectoryW 不创建父目录，失败后逐级向上补建
bool EnsureDirectory(const std::wstring& path, std::wstring& error) {
    if (path.empty()) {
        error = L"目录路径为空";
        return false;
    }

    if (::CreateDirectoryW(path.c_str(), nullptr) != FALSE) {
        return true;
    }

    DWORD code = ::GetLastError();
    // 同名路径可能是文件而非目录，需校验属性
    if (code == ERROR_ALREADY_EXISTS) {
        const DWORD attributes = ::GetFileAttributesW(path.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            return true;
        }
        error = L"同名路径已存在且不是目录：" + path;
        return false;
    }

    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos || separator == 0) {
        error = L"无法创建目录 " + path + L"（错误码 " + std::to_wstring(code) + L"）";
        return false;
    }

    std::wstring parent = path.substr(0, separator);
    if (parent.size() == 2 && parent[1] == L':') {
        parent += L'\\';
    }
    if (!EnsureDirectory(parent, error)) {
        return false;
    }

    if (::CreateDirectoryW(path.c_str(), nullptr) != FALSE) {
        return true;
    }
    code = ::GetLastError();
    if (code == ERROR_ALREADY_EXISTS) {
        return true;
    }
    error = L"无法创建目录 " + path + L"（错误码 " + std::to_wstring(code) + L"）";
    return false;
}

const std::vector<Category>& BuiltinCategories() {
    static const std::vector<Category> categories = [] {
        std::vector<Category> list;
        list.push_back(Category{L"视频", MakeList(kVideoExtensions, std::size(kVideoExtensions))});
        list.push_back(Category{L"音频", MakeList(kAudioExtensions, std::size(kAudioExtensions))});
        list.push_back(
            Category{L"播放列表", MakeList(kPlaylistExtensions, std::size(kPlaylistExtensions))});

        // 按首次出现顺序去重，避免同一扩展名生成两次
        Category all{L"全部", {}};
        std::set<std::wstring> seen;
        for (const Category& category : list) {
            for (const std::wstring& extension : category.extensions) {
                if (seen.insert(extension).second) {
                    all.extensions.push_back(extension);
                }
            }
        }
        list.insert(list.begin(), std::move(all));
        return list;
    }();
    return categories;
}

bool Generate(const std::wstring& root, Report& report, std::wstring& error) {
    report = Report();
    report.root = root;
    error.clear();

    if (!EnsureDirectory(root, error)) {
        return false;
    }

    for (const Category& category : BuiltinCategories()) {
        CategoryResult result;
        result.name = category.name;
        result.directory = JoinPath(root, category.name);

        if (!EnsureDirectory(result.directory, error)) {
            return false;
        }

        // 单个文件失败不中断流程，计入报告
        for (const std::wstring& extension : category.extensions) {
            const std::wstring file =
                JoinPath(result.directory, std::wstring(kFilePrefix) + extension);
            if (WriteFileContent(file)) {
                ++result.created;
            } else {
                ++result.failed;
                if (report.first_failure.empty()) {
                    report.first_failure = file;
                }
            }
        }

        report.created += result.created;
        report.failed += result.failed;
        report.categories.push_back(std::move(result));
    }

    return true;
}

}
