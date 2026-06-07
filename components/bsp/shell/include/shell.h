/*
 * @version: no version
 * @LastEditors: qingmeijiupiao
 * @Description: 命令行界面类，用于处理用户输入和执行命令
 * @author: qingmeijiupiao
 * @LastEditTime: 2026-06-01 18:35:19
 */
#ifndef SHELL_H
#define SHELL_H

#include <functional>
#include <string>
#include <vector>
#include <memory>
extern "C" {
#include "esp_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
}

#ifndef SHELL_TASK_STACK_SIZE
#define SHELL_TASK_STACK_SIZE 8192
#endif

#ifndef SHELL_TASK_PRIORITY
#define SHELL_TASK_PRIORITY 6
#endif

class ShellCommand_t {
public:
    using CommandFunc = std::function<int(int, char**)>;
    using CommandFuncWithContext = std::function<int(void*, int, char**)>;
    
    ShellCommand_t(const std::string& name, 
                 const std::string& help = "", 
                 const std::string& hint = "",
                 CommandFunc func = nullptr,
                 CommandFuncWithContext func_with_context = nullptr,
                 void* context = nullptr)
        : name_(name), help_(help), hint_(hint), 
          func_(func), func_with_context_(func_with_context), context_(context) {}
    
    const std::string& name() const { return name_; }
    const std::string& help() const { return help_; }
    const std::string& hint() const { return hint_; }
    
    int execute(int argc, char** argv) {
        if (func_with_context_) {
            return func_with_context_(context_, argc, argv);
        } else if (func_) {
            return func_(argc, argv);
        }
        return -1;
    }
    
private:
    std::string name_;
    std::string help_;
    std::string hint_;
    CommandFunc func_;
    CommandFuncWithContext func_with_context_;
    void* context_;
};

class Shell {
public:
    enum class Mode {
        ESP_LOG,    // 默认模式，输出日志
        INTERACTIVE // 交互模式，运行REPL
    };
    
    static Shell& instance() {
        static Shell instance;
        return instance;
    }
    
    ~Shell();
    
    // 初始化Shell
    esp_err_t init();
    
    // 注册命令
    esp_err_t register_command(const ShellCommand_t& cmd);
    
    // 注销命令
    esp_err_t deregister_command(const std::string& name);
    
    // 运行命令
    esp_err_t run_command(const std::string& cmdline, int* ret = nullptr);
    
    // 启动交互模式
    esp_err_t start_interactive();
    
    // 停止交互模式
    esp_err_t stop_interactive();
    
    // 获取当前模式
    Mode get_mode() const { return mode_; }
    
    // 设置日志级别
    void set_log_level(esp_log_level_t level);
    
    // 获取原始REPL句柄
    esp_console_repl_t* get_repl() const { return repl_; }
    
private:
    Shell();
    Shell(const Shell&) = delete;
    Shell& operator=(const Shell&) = delete;
    
    // 监听任务函数
    static void listener_task(void* arg);
    
    // 帮助命令处理函数
    int help_command(int argc, char** argv);
    
    // ESP控制台命令包装函数
    static int esp_console_wrapper(int argc, char** argv);
    static int esp_console_wrapper_with_context(void* context, int argc, char** argv);
    
    Mode mode_;
    esp_console_repl_t* repl_;
    TaskHandle_t listener_task_handle_;
    std::vector<std::shared_ptr<ShellCommand_t>> commands_;
    esp_log_level_t original_log_level_;
    bool initialized_;
    
    // 监听配置
    static constexpr int LISTENER_TASK_STACK_SIZE = 2048;
    static constexpr int LISTENER_TASK_PRIORITY = 1;
    static constexpr int LISTENER_TIMEOUT_MS = 100;
};

#endif // SHELL_H