#include <windows.h>
#include <shellapi.h>

#include <string>

#include "generator.hpp"

namespace {

constexpr wchar_t kAppName[] = L"ExtensionTestFile";
constexpr wchar_t kAppVersion[] = L"2.0.0";
constexpr wchar_t kOutputFolderName[] = L"Test";

// GUI 子系统进程不自动拥有控制台，输出通道在此统一解析：
// 被继承的管道或重定向文件可直接复用，否则尝试附着父进程控制台。
class Console {
public:
    static Console& Instance() {
        static Console instance;
        return instance;
    }

    bool Available() const { return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE; }

    void Write(const std::wstring& text) {
        if (!Available() || text.empty()) {
            return;
        }
        if (unicode_) {
            DWORD written = 0;
            ::WriteConsoleW(handle_, text.c_str(), static_cast<DWORD>(text.size()), &written,
                            nullptr);
        } else {
            WriteUtf8(text);
        }
    }

    void WriteLine(const std::wstring& text = std::wstring()) {
        Write(text + L"\r\n");
    }

private:
    Console() {
        // 重定向到文件或管道时类型为 DISK/PIPE，直接写入才能被 shell 捕获
        HANDLE standard = ::GetStdHandle(STD_OUTPUT_HANDLE);
        const DWORD type = (standard == nullptr || standard == INVALID_HANDLE_VALUE)
                               ? FILE_TYPE_UNKNOWN
                               : ::GetFileType(standard);
        if (type == FILE_TYPE_DISK || type == FILE_TYPE_PIPE) {
            handle_ = standard;
            unicode_ = false;
            return;
        }

        // 命令行启动时父进程持有控制台，附着后经 CONOUT$ 写入宽字符
        if (::AttachConsole(ATTACH_PARENT_PROCESS) != FALSE) {
            handle_ = ::CreateFileW(L"CONOUT$", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    nullptr, OPEN_EXISTING, 0, nullptr);
            if (Available()) {
                unicode_ = true;
                owned_ = true;
            }
        }
    }

    ~Console() {
        // 仅关闭自行打开的 CONOUT$，继承句柄不归本进程释放
        if (owned_ && Available()) {
            ::CloseHandle(handle_);
        }
    }

    Console(const Console&) = delete;
    Console& operator=(const Console&) = delete;

    // 非控制台通道按 UTF-8 写出，避免中文在重定向文件中乱码
    void WriteUtf8(const std::wstring& text) {
        const int source_length = static_cast<int>(text.size());
        const int needed = ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), source_length, nullptr, 0,
                                                 nullptr, nullptr);
        if (needed <= 0) {
            return;
        }
        std::string bytes(static_cast<std::size_t>(needed), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, text.c_str(), source_length, &bytes[0], needed, nullptr,
                              nullptr);
        DWORD written = 0;
        ::WriteFile(handle_, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;
    bool unicode_ = false;
    bool owned_ = false;
};

// 无输出通道时改用弹窗，否则失败无任何反馈
void ShowError(Console& console, const std::wstring& message) {
    if (console.Available()) {
        console.WriteLine(std::wstring(kAppName) + L" 错误：" + message);
    } else {
        ::MessageBoxW(nullptr, message.c_str(), kAppName, MB_OK | MB_ICONERROR);
    }
}

// 返回值等于缓冲区大小时表示路径可能被截断，需扩容重试
std::wstring ModulePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length =
            ::GetModuleFileNameW(nullptr, &buffer[0], static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::wstring();
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return buffer;
}

std::wstring ExecutableDirectory() {
    const std::wstring path = ModulePath();
    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return std::wstring();
    }
    return path.substr(0, separator);
}

// 帮助信息取真实文件名，避免产物重命名后提示与实际不符
std::wstring ExecutableName() {
    const std::wstring path = ModulePath();
    const std::size_t separator = path.find_last_of(L"\\/");
    if (separator == std::wstring::npos) {
        return path;
    }
    return path.substr(separator + 1);
}

// 保留 "C:\" 这类根路径，只去除多余的尾部反斜杠
std::wstring TrimTrailingSeparators(std::wstring path) {
    while (path.size() > 3 && (path[path.size() - 1] == L'\\' || path[path.size() - 1] == L'/')) {
        path.erase(path.size() - 1);
    }
    return path;
}

// 统一转为绝对路径，避免结果随启动时的工作目录变化
std::wstring AbsolutePath(const std::wstring& path) {
    if (path.empty()) {
        return path;
    }
    const DWORD needed = ::GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (needed == 0) {
        return path;
    }
    std::wstring buffer(static_cast<std::size_t>(needed), L'\0');
    const DWORD length = ::GetFullPathNameW(path.c_str(), needed, &buffer[0], nullptr);
    if (length == 0 || length >= needed) {
        return path;
    }
    buffer.resize(length);
    return buffer;
}

std::wstring UsageText() {
    return std::wstring(L"用法：") + ExecutableName() + L" [-o <目录>] [-h]\r\n"
           L"\r\n"
           L"  -o, --out <目录>  指定输出目录（默认：程序所在目录下的 Test）\r\n"
           L"  -h, --help        显示本帮助\r\n"
           L"\r\n"
           L"在输出目录下生成 全部 / 视频 / 音频 / 播放列表 四个分类文件夹，每个受支持的\r\n"
           L"扩展名对应一个 5 字节的空文件，用于检查文件类型关联与图标是否正确。\r\n";
}

struct Options {
    std::wstring root;
    bool help = false;
    std::wstring error;
};

bool ParseCommandLine(int argc, wchar_t** argv, Options& options) {
    const std::wstring out_prefix = L"--out=";

    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
            options.help = true;
            continue;
        }
        if (arg == L"-o" || arg == L"--out") {
            if (i + 1 >= argc) {
                options.error = L"选项 " + arg + L" 缺少目录参数";
                return false;
            }
            options.root = argv[++i];
            continue;
        }
        // 同时接受 --out=<目录> 形式
        if (arg.compare(0, out_prefix.size(), out_prefix) == 0) {
            const std::wstring value = arg.substr(out_prefix.size());
            if (value.empty()) {
                options.error = L"选项 --out 缺少目录参数";
                return false;
            }
            options.root = value;
            continue;
        }

        options.error = L"无法识别的参数：" + arg;
        return false;
    }
    return true;
}

void PrintReport(const etf::Report& report) {
    Console& console = Console::Instance();

    console.WriteLine(std::wstring(kAppName) + L" " + kAppVersion);
    console.WriteLine(L"输出目录：" + report.root);
    console.WriteLine();

    for (const etf::CategoryResult& category : report.categories) {
        std::wstring line = L"已完成 [" + category.name + L"] 分类文件创建，共 " +
                            std::to_wstring(category.created) + L" 个文件";
        if (category.failed > 0) {
            line += L"（失败 " + std::to_wstring(category.failed) + L" 个）";
        }
        console.WriteLine(line);
    }

    console.WriteLine();
    if (report.Succeeded()) {
        console.WriteLine(L"所有文件创建完成！共 " + std::to_wstring(report.created) + L" 个文件。");
        return;
    }

    console.WriteLine(L"生成结束：成功 " + std::to_wstring(report.created) + L" 个，失败 " +
                      std::to_wstring(report.failed) + L" 个。");
    if (!report.first_failure.empty()) {
        console.WriteLine(L"首个失败文件：" + report.first_failure);
    }
}

// 成功时提示完成；存在失败时改为统计与首个失败路径，避免误报成功
void ShowCompletionDialog(const etf::Report& report) {
    std::wstring text;
    UINT icon = MB_ICONINFORMATION;

    if (report.Succeeded()) {
        text = L"全部创建完毕";
    } else {
        icon = MB_ICONWARNING;
        text = L"部分文件创建失败：成功 " + std::to_wstring(report.created) + L" 个，失败 " +
               std::to_wstring(report.failed) + L" 个。";
        if (!report.first_failure.empty()) {
            text += L"\r\n\r\n首个失败文件：\r\n" + report.first_failure;
        }
    }

    ::MessageBoxW(nullptr, text.c_str(), kAppName, MB_OK | icon);
}

}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    Console& console = Console::Instance();

    int argc = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argv == nullptr) {
        ShowError(console, L"命令行解析失败");
        return 1;
    }

    // 默认输出到程序所在目录，双击与快捷方式启动结果一致
    Options options;
    options.root = etf::JoinPath(ExecutableDirectory(), kOutputFolderName);
    const bool parsed = ParseCommandLine(argc, argv, options);
    ::LocalFree(argv);

    if (!parsed) {
        ShowError(console, options.error + L"\r\n\r\n" + UsageText());
        return 2;
    }

    if (options.help) {
        if (console.Available()) {
            console.Write(UsageText());
        } else {
            ::MessageBoxW(nullptr, UsageText().c_str(), kAppName, MB_OK | MB_ICONINFORMATION);
        }
        return 0;
    }

    const std::wstring root = TrimTrailingSeparators(AbsolutePath(options.root));

    etf::Report report;
    std::wstring error;
    if (!etf::Generate(root, report, error)) {
        ShowError(console, error);
        return 1;
    }

    PrintReport(report);
    ShowCompletionDialog(report);

    return report.Succeeded() ? 0 : 1;
}
