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
    int fd = -1;

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
    file(std::string prefix, FILETYPE filetype) : path(std::move(prefix))
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
class Compiler
{
    std::string c_compiler;
    std::string cpp_compiler;

    std::string version;
    bool self_check()
    {

        std::string cmd = c_compiler + " --version";
        std::string regex = "(" + c_compiler + R"(|).{0,5}(version|版本)?.{0,5}(\d{1,2}\.\d{1,2}\.\d{1,2}))";
        re2::RE2 pattern(regex);
        assert(pattern.ok());
        FILE *fp = nullptr;
        fp = popen(cmd.c_str(), "r");
        if (fp == nullptr)
        {
            throw std::runtime_error("popen failed");
        }
        char buf[1024];
        fgets(buf, sizeof(buf), fp);
        pclose(fp);
        return re2::RE2::Extract(buf, pattern, R"(\3)", &version);
    }

public:
    const char *compilername(bool isCFile) const
    {
        if (isCFile)
        {
            return c_compiler.c_str();
        }
        return cpp_compiler.c_str();
    }
    explicit Compiler(const std::string &compilername)
    {
        if (compilername == "clang" || compilername == "clang++")
        {
            c_compiler = "clang";
            cpp_compiler = "clang++";
        }
        else if (compilername == "gcc" || compilername == "g++")
        {
            c_compiler = "gcc";
            cpp_compiler = "gcc++";
        }
        else
        {
            throw std::runtime_error("Unknow compiler");
        }
        self_check();
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
    void compile(const std::vector<std::string> &compileParameter, const std::string &path, const Compiler &compiler)
    {
        const char *argv[compileParameter.size() + 4];
        argv[0] = compiler.compilername(cFile);
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
