#include <array>
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
#include <utility>
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

enum FILETYPE
{
    IN,
    OUT,
    ERR
};
enum status
{
    AC,
    WA,
    CE,
    RE,
    TLE
};

bool Options_t, Options_c, Options_p, Options_e;

class file
{

    std::string path;
    int fd{};

    std::array<unsigned char, SHA256_DIGEST_LENGTH> getSHA256() const
    {
        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
        FILE *fp = fopen(path.c_str(), "rb");
        SHA256_CTX tmp;

        if (fp == nullptr)
        {
            throw std::runtime_error(strerror(errno));
        }
        SHA256_Init(&tmp);

        size_t size = 0;
        unsigned char buf[4096];

        while ((size = fread(buf, 1, 4096, fp)) != 0)
        {
            SHA256_Update(&tmp, buf, size);
        }
        SHA256_Final(hash.data(), &tmp);
        fclose(fp);
        return hash;
    }

public:
    void redirect(int from) const
    {
        if (dup2(from, fd) == -1)
        {
            throw std::runtime_error(strerror(errno));
        }
    }
    bool operator==(const file &other) const
    {
        return getSHA256() == other.getSHA256();
    }
    bool operator!=(const file &other) const
    {
        return !operator==(other);
    }
    file(std::string prefix, FILETYPE filetype) : path(std::move(std::move(prefix)))
    {
        switch (filetype)
        {
        case IN:
            path += "_in.txt";
            break;
        case OUT:
            path += "_out.txt";
            break;
        case ERR:
            path += "_err.txt";
            break;
        }
        if (filetype == IN)
        {
            fd = open(path.c_str(), O_RDONLY);
        }
        else
        {
            fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (fd == -1)
        {
            throw std::runtime_error("open file failed");
        }
    }
    explicit file(std::string filepath) : path(std::move(std::move(filepath)))
    {
        fd = open(path.c_str(), O_RDONLY);
    }
    explicit file(char *filepath) : path(filepath)
    {
        fd = open(path.c_str(), O_RDONLY);
    }
};
class SourceCode
{
    std::string sourceCodePath;
    bool cFile = false;
    std::string programPath;
    void JudgmentType()
    {
        RE2 re("\\.c$");
        assert(re.ok());
        cFile = RE2::PartialMatch(sourceCodePath, re);
    }

public:
    void compile(const std::vector<std::string> &compileParameter, const std::string &path)
    {
        const char *argv[compileParameter.size() + 4];
        if (cFile)
        {
            if (compileParameter[0] == "g++")
            {
                argv[0] = "gcc";
            }
            else if (compileParameter[0] == "clang++")
            {
                argv[0] = "clang";
            }
        }
        else
        {
            if (compileParameter[0] == "gcc")
            {
                argv[0] = "g++";
            }
            else if (compileParameter[0] == "clang")
            {
                argv[0] = "clang++";
            }
        }
        size_t i;
        for (i = 1; i < compileParameter.size(); i++)
        {
            argv[i] = compileParameter[i].c_str();
        }
        argv[i++] = sourceCodePath.c_str();
        argv[i++] = "-o";
        argv[i++] = path.c_str();
        argv[i] = nullptr;
        pid_t pid = fork();
        if (pid < 0)
        {
            throw std::runtime_error("fork failed");
        }
        if (pid == 0)
        {
            execvp(argv[0], (char *const *)argv);
        }
        else
        {
            int wstatus = 0;
            int ret = waitpid(pid, &wstatus, 0);
            if (ret == -1)
            {
                throw std::runtime_error("waitpid failed");
            }
            if (!WIFEXITED(wstatus))
            {
                throw std::runtime_error("CE");
            }
        }
    }
    explicit SourceCode(char *path) : sourceCodePath(path) {}
    explicit SourceCode(std::string path) : sourceCodePath(std::move(std::move(path))) {}
};

int main(int argc, char *argv[])
{
    std::string targetSourceCodePath;
    std::string currectSourceCodePath;
    int opt = 0;
    while ((opt = getopt(argc, argv, "t:c:T:")) != -1)
    {
        switch (opt)
        {
        case 't':
            targetSourceCodePath = optarg;
            Options_t = true;
            break;
        case 'c':
            currectSourceCodePath = optarg;
            Options_c = true;
            break;
        default:
            printf("Unknow option:%c", opterr);
            exit(EXIT_FAILURE);
        }
    }
    SourceCode currect(currectSourceCodePath), target(targetSourceCodePath);
}

/*
Y7n05h 在此处使用了很多C语言的方式完成IO操作
是因为愚蠢的Y7n05h还没学明白C++里面的各种IO方式
Y7n05h承诺未来会改善他们
当然，有兴趣帮助Y7n05h改进程序的欢迎PR
*/

/* class TEST //一次测试，完成程序的运行、输入、输出、比较
{

    int input_fd;

    char *programPath[2]; //程序路径
    char *OutPath[2];     // 标准输出路径
    char *ErrPath[2];     // 错误输出路径

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
}; */