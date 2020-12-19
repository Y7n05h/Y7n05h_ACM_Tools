#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <error.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <openssl/sha.h>
#include <re2/re2.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
constexpr int TimeLimit = 1;
constexpr int LEN = 9;
/**
 * 参数 -t  target  目标代码路径
 * 参数 -c correct 正确代码路径
 * 参数 -p path 手动指定本程序配置文件路径，无此参数则读取 ./acmtoolsconfig.json(暂未实现)
 * 参数 -e end 结束测试，清空测试时生成的文件
 * 参数 -T times 测试次数缺省值为10
  */
enum types
{
    target,
    currect
};
enum status
{
    AC,
    WA,
    CE,
    RE,
    TLE
};
char *SourcePath[2];
bool Options_t, Options_c, Options_p, Options_e;
bool compare(const char *file1, const char *file2);
int compile(char *filepath, const std::vector<std::string> &compileArgs);
char *getSHA256(const char *filename);

class TEST //一次测试，完成程序的运行、输入、输出、比较
{

    int input_fd;

    char *programPath[2]; //程序路径
    char *OutPath[2];     // 标准输出路径
    char *ErrPath[2];     // 错误输出路径

    static char *getSHA256(const char *filename)
    {
        char cmd[strlen(filename) + strlen("sha256sum ") + 1];
        strcpy(cmd, "sha256sum ");
        strcat(cmd, filename);
        FILE *fp = popen(cmd, "r");
        char *hash = nullptr;
        try
        {
            hash = new char[65];
        }
        catch (std::bad_alloc &err)
        {
            std::cout << err.what() << std::endl;
        }
        fscanf(fp, "%64s", hash);
        pclose(fp);
        return hash;
    }
    int run(types t) //返回值为运行状态
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            // 子进程
            int err_fd = open(ErrPath[t], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            int out_fd = open(OutPath[t], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (err_fd == -1 || out_fd == -1)
            {
                throw std::runtime_error(strerror(errno));
            }

            dup2(STDIN_FILENO, input_fd);
            dup2(STDOUT_FILENO, out_fd);
            dup2(STDERR_FILENO, err_fd);
            execl(programPath[t], nullptr);
        }
        // 父进程
        sleep(TimeLimit);
        int exitStatus;
        waitpid(pid, &exitStatus, WNOHANG);
        return WIFEXITED(exitStatus); //正常终止则为真
    }

public:
    bool compare()
    {
        char *fileHash[2];
        fileHash[currect] = getSHA256(OutPath[currect]);
        fileHash[target] = getSHA256(OutPath[target]);
        bool result = (strcmp(fileHash[currect], fileHash[target]) == 0);
        delete[] fileHash[currect];
        delete[] fileHash[target];
        return result;
    }
    status test()
    {
        if (run(currect) != 0)
        {
            fprintf(stderr, "Error.\n对照程序 CE\n");
            return CE;
        }
        if (run(target) != 0)
        {
            fprintf(stderr, "CE\n");
            return CE;
        }
        if (compare())
        {
            fprintf(stderr, "WA\n");
            return WA;
        }
    }
};

int main(int argc, char *argv[])
{
    int opt = 0;
    while ((opt = getopt(argc, argv, "t:c:T:")) != -1)
    {
        switch (opt)
        {
        case 't':
            SourcePath[target] = optarg;
            Options_t = true;
            break;
        case 'c':
            SourcePath[currect] = optarg;
            Options_c = true;
            break;
        default:
            printf("Unknow option:%c", opterr);
            exit(EXIT_FAILURE);
        }
    }
}

/*
Y7n05h 在此处使用了很多C语言的方式完成IO操作
是因为愚蠢的Y7n05h还没学明白C++里面的各种IO方式
Y7n05h承诺未来会改善他们
当然，有兴趣帮助Y7n05h改进程序的欢迎PR
*/

int compile(char *filepath, const std::vector<std::string> &compileArgs)
{
    RE2 re("\\.c$");
    assert(re.ok());
    bool cfile = RE2::PartialMatch(filepath, re);
    int argc = compileArgs.size() + 2;
    char *argv[argc];

    for (size_t i = 0; i < compileArgs.size(); i++)
    {
        try
        {
            argv[i + 1] = new char[compileArgs[i].size() + 1];
        }
        catch (std::bad_alloc err)
        {
            std::cout << err.what() << std::endl;
        }
        strcpy(argv[i + 1], compileArgs[i].c_str());
    }
    try
    {
        argv[0] = new char[LEN];
    }
    catch (std::bad_alloc err)
    {
        std::cout << err.what() << std::endl;
    }
    strcpy(argv[0], cfile ? "gcc" : "g++");
    argv[argc - 1] = nullptr;
    execvp(argv[0], argv);
    throw std::runtime_error("exec Failed");
    return 1;
}
class self_check
{
    std::string clang_version, gcc_version, clangtidy_version, cppcheck_version;
    void check_clang()
    {
        FILE *fp = popen("clang -v", "r");
        std::string regex = R"(clang.{0,4}(version|版本).{0,4}([\d\.]{3,10}))";
        re2::RE2 pattern(regex);
        int n = 10;
        while (n-- && feof(fp) == 0)
        {
            char line[2048];
            fgets(line, sizeof(line), fp);
            re2::RE2::Extract(line, pattern, R"(\2)", &clang_version);
        }
    }
    void check_gcc()
    {
        FILE *fp = popen("gcc -v", "r");
        std::string regex = R"(gcc.{0,4}(version|版本).{0,4}([\d\.]{3,10}))";
        re2::RE2 pattern(regex);
        int n = 10;
        while (n-- && feof(fp) == 0)
        {
            char line[2048];
            fgets(line, sizeof(line), fp);
            re2::RE2::Extract(line, pattern, R"(\2)", &clang_version);
        }
    }
    void check_clangtidy()
    {
        FILE *fp = popen("clang-tidy --version", "r");
        std::string regex = R"(LLVM.{0,4}(version|版本).{0,4}([\d\.]{3,10}))";
        re2::RE2 pattern(regex);
        int n = 10;
        while (n-- && feof(fp) == 0)
        {
            char line[2048];
            fgets(line, sizeof(line), fp);
            re2::RE2::Extract(line, pattern, R"(\2)", &clang_version);
        }
    }
};
void RAND()
{
}