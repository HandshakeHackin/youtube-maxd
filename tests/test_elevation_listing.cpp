/* ProsperoTV - optional-Lapy elevation regression, derived from ps5-native-app-boilerplate.
 * Copyright (C) 2026 BlackBearReloaded
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <string>
#include <string_view>

#include "../native/elevation/elevation.cpp"

namespace test
{
unsigned list_calls{};
unsigned socket_calls{};
unsigned replies{};
bool helper_read{};
std::string data;
} // namespace test

extern "C"
{
    pid_t getpid() noexcept
    {
        return 4242;
    }

    uid_t geteuid() noexcept
    {
        return 1000;
    }

    int seteuid(uid_t) noexcept
    {
        return 0;
    }

    int open(const char *path, int, ...)
    {
        const std::string_view name{path};
        if (name == "/download0/lapy_owned_result")
            return 10;
        if (name.starts_with("/download0/.elevate_proc."))
            return 11;
        if (name.starts_with("/data/.lapy_probe_"))
            return 12;
        if (name == "/app0/lapy.elf")
            return 13;
        errno = ENOENT;
        return -1;
    }

    ssize_t write(int descriptor, const void *buffer, size_t size)
    {
        if (descriptor == 12)
            test::data.append(static_cast<const char *>(buffer), size);
        return static_cast<ssize_t>(size);
    }

    ssize_t read(int descriptor, void *buffer, size_t size)
    {
        if (descriptor == 12)
        {
            const auto count = std::min(size, test::data.size());
            std::memcpy(buffer, test::data.data(), count);
            return static_cast<ssize_t>(count);
        }
        if (descriptor == 13 && !test::helper_read)
        {
            test::helper_read = true;
            constexpr std::string_view elf{"\x7f"
                                           "ELF"};
            const auto count = std::min(size, elf.size());
            std::memcpy(buffer, elf.data(), count);
            return static_cast<ssize_t>(count);
        }
        return descriptor == 13 ? 0 : -1;
    }

    off_t lseek(int descriptor, off_t offset, int whence) noexcept
    {
        return descriptor == 12 && offset == 0 && whence == SEEK_SET ? 0 : -1;
    }

    int close(int)
    {
        return 0;
    }

    int fchmod(int, mode_t) noexcept
    {
        return 0;
    }

    int rename(const char *, const char *) noexcept
    {
        return 0;
    }

    int unlink(const char *) noexcept
    {
        return 0;
    }

    int usleep(useconds_t)
    {
        return 0;
    }

    DIR *opendir(const char *path)
    {
        assert(std::strcmp(path, "/data") == 0);
        if (++test::list_calls <= 51)
        {
            errno = EPERM;
            return nullptr;
        }
        return reinterpret_cast<DIR *>(1);
    }

    int closedir(DIR *folder)
    {
        return folder == reinterpret_cast<DIR *>(1) ? 0 : -1;
    }

    int sceNetSocket(const char *, int, int, int)
    {
        ++test::socket_calls;
        return 20;
    }

    int sceNetSetsockopt(int, int, int, const void *, std::uint32_t)
    {
        return 0;
    }

    int sceNetConnect(int, const void *, std::uint32_t)
    {
        return 0;
    }

    int sceNetSend(int, const void *, std::size_t size, int)
    {
        return static_cast<int>(size);
    }

    int sceNetRecv(int, void *data, std::size_t size, int)
    {
        elevation::wire::Message reply{};
        reply.pid = 4242;
        reply.kind =
            test::replies++ == 0 ? elevation::wire::Kind::prepare : elevation::wire::Kind::response;
        const auto count = std::min(size, sizeof(reply));
        std::memcpy(data, &reply, count);
        return static_cast<int>(count);
    }

    int sceNetSocketClose(int)
    {
        return 0;
    }
}

int main()
{
    assert(elevation::request(elevation::Capability::filesystem) == elevation::Status::ok);
    assert(std::string_view{elevation::path()} == "helper");
    assert(test::socket_calls == 1);
    assert(test::list_calls == 52);
}
