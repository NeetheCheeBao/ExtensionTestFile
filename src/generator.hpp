#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace etf {

// 与生成内容 "EEEEE" 等长，实现处用 static_assert 约束
constexpr std::size_t kFileContentSize = 5;

// 分类目录名与扩展名清单，扩展名不含点号
struct Category {
    std::wstring name;
    std::vector<std::wstring> extensions;
};

struct CategoryResult {
    std::wstring name;
    std::wstring directory;
    std::size_t created = 0;
    std::size_t failed = 0;
};

struct Report {
    std::wstring root;
    std::vector<CategoryResult> categories;
    std::size_t created = 0;
    std::size_t failed = 0;
    std::wstring first_failure; // 首个失败文件的完整路径

    // 全部文件写入成功才视为成功
    bool Succeeded() const { return failed == 0; }
};

// 首次调用构造；顺序为 全部、视频、音频、播放列表，“全部”按首次出现去重
const std::vector<Category>& BuiltinCategories();

std::wstring JoinPath(const std::wstring& directory, const std::wstring& name);

bool EnsureDirectory(const std::wstring& path, std::wstring& error);

// 无法创建目录视为致命错误并返回 false；单文件写入失败只记入 Report
bool Generate(const std::wstring& root, Report& report, std::wstring& error);

}
