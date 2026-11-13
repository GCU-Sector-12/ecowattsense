// temp_folder.h: helpers for tests that need a data folder below the system temporary folder.
// Used by the file tests (config, secret, configure, queue, history, identity, log). Each test makes its own folder
// and removes it at the end, so nothing is left behind.
#pragma once

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>

namespace ews_test {

/// Makes a new empty folder for one test.
/// @param name A short name of the test; it becomes part of the folder name together with the process id.
/// @return The path of the folder.
inline std::filesystem::path NewDataFolder(const char* name) {
    auto folder = std::filesystem::temp_directory_path() / (std::string("ews-io-test-") + name + "-" + std::to_string(GetCurrentProcessId()));
    std::filesystem::remove_all(folder);
    std::filesystem::create_directory(folder);
    return folder;
}

/// Removes a test folder with everything in it.
/// @param folder The folder from NewDataFolder.
inline void RemoveDataFolder(const std::filesystem::path& folder) {
    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);
}

/// Reads a whole file as bytes.
/// @param path The file.
/// @return The content, or an empty string when the file cannot be opened.
inline std::string ReadText(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
}

/// Writes bytes to a file, replacing it; the folder of the file is created first.
/// @param path The file.
/// @param text The bytes.
inline void WriteText(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
}

/// Gives one line of a list, or an empty text when the list is shorter (a failed test must not crash).
/// @param lines The list.
/// @param index The position.
/// @return The line, or "".
inline std::string LineAt(const std::vector<std::string>& lines, std::size_t index) {
    return index < lines.size() ? lines[index] : std::string();
}

}  // namespace ews_test
