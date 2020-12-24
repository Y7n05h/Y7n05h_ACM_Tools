#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <error.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <re2/re2.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

class self_check
{
    std::string clang_version, gcc_version, clangtidy_version, cppcheck_version;

    enum software
    {
        gcc,
        clang,
        clangtidy
    };

    void check(const std::string &name)
    {
        std::string regex = R"(.{0,4}(version|版本).{0,4}([\d\.]{3,10}))";
        std::string cmd = "--version";
        cmd = name + cmd;
        FILE *fp = popen(cmd.c_str(), "r");
        regex = name + regex;
        re2::RE2 pattern(regex);
        int n = 10;
        while (n-- && feof(fp) == 0)
        {
            char line[2048];
            fgets(line, sizeof(line), fp);

            if (re2::RE2::Extract(line, pattern, R"(\2)", &clang_version))
            {
                pclose(fp);
                return;
            }
        }
        pclose(fp);
        throw std::runtime_error("read " + name + " failed");
    }
};
