#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <windows.h>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

// ===================== 工具函数 =====================

std::string get_current_time() {
    std::time_t t = std::time(nullptr);
    char buf[100];
    std::tm tm_info;
    localtime_s(&tm_info, &t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_info);
    return buf;
}

static inline std::string ltrim(std::string s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    return s;
}
static inline std::string rtrim(std::string s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), s.end());
    return s;
}
static inline std::string trim(std::string s) { return rtrim(ltrim(std::move(s))); }

static inline std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static inline std::vector<std::string> split_by_char(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string cur;
    std::stringstream ss(s);
    while (std::getline(ss, cur, delim)) out.push_back(cur);
    return out;
}

// 把扩展名标准化为 ".xxx"（小写）
static inline std::string normalize_ext(std::string ext) {
    ext = trim(to_lower(ext));
    if (ext.empty()) return ext;
    if (ext == "all") return ext;
    if (ext[0] != '.') ext = "." + ext;
    return ext;
}

// 输出目录名：去掉点，转大写。例如 ".aac" -> "AAC"
static inline std::string ext_to_folder_name(const std::string& extDotLower) {
    std::string s = extDotLower;
    if (!s.empty() && s[0] == '.') s.erase(s.begin());
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
    return s.empty() ? "OUT" : s;
}

// 捕获命令输出：这里通过 "2>&1" 把 stderr 合并进 stdout
std::string run_command(const std::string& cmd) {
    std::stringstream output;
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("无法执行命令: " + cmd);

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output << buffer;
    }

    int return_code = _pclose(pipe);
    if (return_code != 0) {
        std::string msg = "命令执行失败，错误代码: " + std::to_string(return_code) + "\n";
        msg += output.str();
        return msg;
    }
    return output.str();
}

// 统一用 ffmpeg 自动选择合适编码器（简单可靠）
// 如需更精细控制（比特率/采样率/codec），可以在这里按扩展名定制参数
void run_ffmpeg_convert(const std::string& input, const std::string& output, std::ofstream& log_file) {
    // 合并 stderr 到 stdout，确保能记录到日志
    std::string cmd = "ffmpeg -y -i \"" + input + "\" -vn \"" + output + "\" 2>&1";

    std::string out = run_command(cmd);
    // 判断是否失败：run_command 在 return_code !=0 时也会返回“失败文本”，但仍可写日志
    if (out.rfind("命令执行失败", 0) == 0) {
        log_file << get_current_time() << " - 转换失败: " << input << " -> " << output << "\n";
        log_file << "FFmpeg 输出:\n" << out << "\n";
        throw std::runtime_error("FFmpeg 转换失败: " + input);
    }

    log_file << get_current_time() << " - 转换成功: " << input << " -> " << output << "\n";
    log_file << "FFmpeg 输出:\n" << out << "\n";
}

// 判断是不是“音频文件”（用于 all）
bool is_audio_extension(const std::string& extLowerDot) {
    static const std::vector<std::string> exts = {
        ".mp3",".wav",".flac",".aac",".ogg",".m4a",".wma",".aiff",".amr",".opus",".ac3",".dts",
        ".mka",".alac",".tta",".speex",".mpc",".ape",".vqf"
    };
    return std::find(exts.begin(), exts.end(), extLowerDot) != exts.end();
}

// ===================== 指令解析与执行 =====================

struct Task {
    bool all = false;
    std::vector<std::string> fromExts; // 标准化为 ".xxx" 小写
    std::string toExt;                 // 标准化为 ".xxx" 小写
};

// 从一段文本里解析单个任务： "[from] to [to]"
Task parse_single_task(const std::string& raw) {
    std::string s = trim(raw);
    std::string lower = to_lower(s);

    // 找 " to " 或 "to"（尽量宽松）
    // 简单做法：用 lower 找到 "to"
    size_t pos = lower.find("to");
    if (pos == std::string::npos) throw std::runtime_error("任务缺少关键字 'to' ：" + raw);

    std::string left = trim(s.substr(0, pos));
    std::string right = trim(s.substr(pos + 2));

    left = to_lower(left);
    right = to_lower(right);

    if (left.empty() || right.empty()) throw std::runtime_error("任务格式错误（左右为空）： " + raw);

    Task t;
    if (trim(left) == "all") {
        t.all = true;
    }
    else {
        auto parts = split_by_char(left, ',');
        for (auto& p : parts) {
            std::string ext = normalize_ext(p);
            if (ext.empty() || ext == "all") throw std::runtime_error("原始格式无效： " + raw);
            t.fromExts.push_back(ext);
        }
        if (t.fromExts.empty()) throw std::runtime_error("原始格式为空： " + raw);
    }

    t.toExt = normalize_ext(right);
    if (t.toExt.empty() || t.toExt == "all") throw std::runtime_error("目标格式无效： " + raw);

    return t;
}

// 扫描 audio 目录，按任务筛选出待转换文件
std::vector<fs::path> collect_inputs(const fs::path& input_dir, const Task& task) {
    std::vector<fs::path> inputs;
    if (!fs::exists(input_dir) || !fs::is_directory(input_dir)) return inputs;

    for (const auto& entry : fs::directory_iterator(input_dir)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = to_lower(entry.path().extension().string());
        if (task.all) {
            if (is_audio_extension(ext)) inputs.push_back(entry.path());
        }
        else {
            if (std::find(task.fromExts.begin(), task.fromExts.end(), ext) != task.fromExts.end()) {
                inputs.push_back(entry.path());
            }
        }
    }
    return inputs;
}

void ensure_dir(const fs::path& p) {
    if (!fs::exists(p)) fs::create_directories(p);
}

void execute_task(const fs::path& base_dir, const Task& task, std::ofstream& log_file) {
    fs::path input_dir = base_dir / "audio";
    auto inputs = collect_inputs(input_dir, task);

    std::string folderName = ext_to_folder_name(task.toExt);
    fs::path output_dir = base_dir / folderName;
    ensure_dir(output_dir);

    log_file << get_current_time() << " - 开始任务: "
        << (task.all ? "all" : "[多格式]") << " to " << task.toExt
        << " | 输出目录: " << output_dir.string() << "\n";

    if (inputs.empty()) {
        log_file << get_current_time() << " - 任务无文件可转换（audio 目录未找到匹配文件）\n";
        std::cout << "[提示] 没有找到匹配文件：audio\\" << std::endl;
        return;
    }

    for (const auto& inPath : inputs) {
        fs::path outPath = output_dir / (inPath.stem().string() + task.toExt);
        try {
            std::cout << "正在转换: " << inPath.string() << " -> " << outPath.string() << std::endl;
            run_ffmpeg_convert(inPath.string(), outPath.string(), log_file);
            std::cout << "转换成功: " << outPath.string() << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "转换失败: " << inPath.string() << " | " << e.what() << std::endl;
            log_file << get_current_time() << " - 单文件失败: " << inPath.string() << " | " << e.what() << "\n";
        }
    }

    log_file << get_current_time() << " - 任务结束\n";
}

// ===================== 主程序 =====================

int main() {
    try {
        SetConsoleTitle(L"GOtobe 1.0.0—Copyright Tank37135");

        fs::path base_dir = fs::current_path();

        std::ofstream log_file("conversion_log.txt", std::ios::app);
        if (!log_file.is_open()) throw std::runtime_error("无法打开日志文件！\n");

        log_file << get_current_time() << " - 程序启动\n";

        std::cout << "转换指令示例（将MP3转化为acc）：.mp3 to .aac  \n";
        std::cout << "多种需求转换指令（示例：.mp3 to .aac | .aac to .wav）\n";
        std::cout << "转化全部文件至某格式：all to .xxx  将多种格式转化为特定格式，例如：  .mp3,.aac to .wav\n";
        std::cout << "输入 exit 退出。\n\n";

        while (true) {
            std::cout << "Go>>> ";
            std::string line;
            if (!std::getline(std::cin, line)) break;

            line = trim(line);
            if (line.empty()) continue;

            std::string lower = to_lower(line);
            if (lower == "exit" || lower == "quit") {
                std::cout << "退出程序。\n";
                log_file << get_current_time() << " - 用户退出\n";
                break;
            }

            // 多任务：用 '|' 分割
            auto pieces = split_by_char(line, '|');

            for (auto& piece : pieces) {
                std::string one = trim(piece);
                if (one.empty()) continue;

                try {
                    Task t = parse_single_task(one);
                    execute_task(base_dir, t, log_file);
                }
                catch (const std::exception& e) {
                    std::cerr << "任务解析/执行失败: " << one << "\n原因: " << e.what() << std::endl;
                    log_file << get_current_time() << " - 任务失败: " << one << "\n原因: " << e.what() << "\n";
                }
            }

            std::cout << "本次指令执行完成。\n\n";
        }

        log_file << get_current_time() << " - 程序结束\n";
        log_file.close();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "程序错误: " << e.what() << std::endl;
        return 1;
    }
}
