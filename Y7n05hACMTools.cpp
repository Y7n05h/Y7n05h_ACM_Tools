#include "DataGenerationTools.h"
#include <array>
#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
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
class File
{
    int fd = -1;

    std::array<unsigned char, SHA256_DIGEST_LENGTH> getSHA256() const
    {
        std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
        lseek(fd, 0, SEEK_SET);
        SHA256_CTX tmp;
#ifndef NDEBUG

        printf("fd:%d \n", fd);
#endif
        if (fd < 0)
        {
            throw std::runtime_error(strerror(errno));
        }
        SHA256_Init(&tmp);
        ssize_t size = 0;
        unsigned char buf[10];

        while ((size = read(fd, buf, sizeof(buf))) > 0)
        {
            printf("read:%s\n", buf);
            SHA256_Update(&tmp, buf, size);
        }

        SHA256_Final(hash.data(), &tmp);
        ::close(fd);
#ifndef NDEBUG
        for (auto i : hash)
        {
            printf("%02X", i);
        }
        printf("\n");
#endif
        return hash;
    }

public:
    void redirect(int from) const
    {
        if (dup2(fd, from) == -1)
        {
            throw std::runtime_error(strerror(errno));
        }
    }
    bool operator==(const File &other) const
    {
        return getSHA256() == other.getSHA256();
    }
    bool operator!=(const File &other) const
    {
        return !operator==(other);
    }
    File(const std::string &prefix, FILETYPE filetype)
    {
        std::string path = prefix;

        switch (filetype)
        {
        case IN:
            path += "_in.txt";
            fd = open(path.c_str(), O_RDONLY);
            break;
        case OUT:
            path += "_out.txt";
            fd = open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);

            break;
        case ERR:
            path += "_err.txt";
            fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            break;
        }
        if (fd == -1)
        {
            throw std::runtime_error("open file failed");
        }
    }

    explicit File(int fd) : fd(fd) {}
    int close() const
    {
        return ::close(fd);
    }
};
class SourceCode
{
    std::string sourceCodePath;
    bool cFile = false;
    void JudgmentType()
    {
        RE2 re(R"(\.c$)");
        assert(re.ok());
        cFile = RE2::PartialMatch(sourceCodePath, re);
    }
    friend class Compiler;

public:
    explicit SourceCode(char *path) : sourceCodePath(path)
    {
        JudgmentType();
    }
    explicit SourceCode(std::string path) : sourceCodePath(std::move(std::move(path)))
    {
        JudgmentType();
    }
};
class TestUnit
{
    const File &in;
    File out, err;

public:
    TestUnit(const std::string &path, const File &in) : in(in), out(path, OUT), err(path, ERR)
    {
    }

    bool operator==(TestUnit const &another) const //TODO
    {

        return out == another.out;
    }
    void redirect() const
    {
        in.redirect(STDIN_FILENO);
        out.redirect(STDOUT_FILENO);
        err.redirect(STDERR_FILENO);
    }
};

class Program
{
    std::string path;

public:
    status run(const TestUnit &unit) const
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            throw std::runtime_error("fork() failed");
        }
        if (pid > 0)
        {
            int exitStatus;
            clock_t end = clock() + CLOCKS_PER_SEC * TimeLimit;

            while (clock() < end)
            {
                int retStatus = waitpid(pid, &exitStatus, WNOHANG);
                if (retStatus == 0)
                {
                    continue;
                }
                if (retStatus == pid)
                {
                    if (WIFEXITED(exitStatus)) //正常终止则为真
                    {
                        return AC;
                    }
                    return RE;
                }
                throw std::runtime_error("waitpid failed");
            }
            kill(pid, SIGKILL);
            return TLE;
        }
        unit.redirect();
        execl(path.c_str(), path.c_str(), nullptr);
        throw std::runtime_error("execl failed");
    }
    explicit Program(std ::string path) : path(std::move(std::move(path))) {}
    explicit Program(const char *path) : path(path) {}
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

        //clang --version
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
    Program *compile(const std::vector<std::string> &compileParameter, const SourceCode &code) const
    {

        const char *argv[compileParameter.size() + 5];
        argv[0] = code.cFile ? c_compiler.c_str() : cpp_compiler.c_str();
        size_t i;
        for (i = 1; i <= compileParameter.size(); i++)
        {
            argv[i] = compileParameter[i].c_str();
        }
        argv[i++] = code.sourceCodePath.c_str();
        argv[i++] = "-o";
        std::string path = code.sourceCodePath;
        re2::RE2 regex(R"(\.c(c?|pp)$)");
        re2::RE2::Replace(&path, regex, ".out");
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
        auto *program = new Program(path);
        return program;
    }
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
class TestGroup
{
    File in;
    TestUnit target, currect;
    std::string prefix;

public:
    explicit TestGroup(const std::string &prefix)
        : in(RandDataGenerator::generate(prefix)), target(prefix + "target", in), currect(prefix + "currect", in)
    {
    }
    status run(const Program &targetProgram, const Program &currectProgram) const
    {
        status targetStatus = targetProgram.run(target);
        currectProgram.run(currect);
        if (targetStatus != AC)
        {
            return targetStatus;
        }
        if (target == currect)
        {
            return AC;
        }
        return WA;
    }
};