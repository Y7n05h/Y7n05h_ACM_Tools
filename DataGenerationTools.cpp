#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

class RandDataGenerator
{
    void generator()
    {
        constexpr int MAX = 10;
        timespec tm{};
        clock_gettime(CLOCK_REALTIME, &tm);
        srand(static_cast<unsigned>(tm.tv_nsec));
        int t = 10;
        printf("%d\n", t);
        while (t--)
        {
            printf("%d ", rand() % MAX);
        }
    }

public:
    int generate(const std::string &path)
    {

        if (chdir(path.c_str()) != 0)
        {
            throw std::runtime_error("bad path");
        }
        int fd = open("in.txt", O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (fd < 0)
        {
            throw std::runtime_error("open file failed");
        }
        int backup = dup(STDOUT_FILENO);
        if (backup < 0)
        {
            throw std::runtime_error("open file failed");
        }
        if (dup2(fd, STDOUT_FILENO) == -1)
        {
            throw std::runtime_error("dup2 failed");
        }
        RandDataGenerator();
        if (dup2(STDOUT_FILENO, backup))
        {
            throw std::runtime_error("dup2 failed");
        }
        close(backup);
        return fd;
    }
};