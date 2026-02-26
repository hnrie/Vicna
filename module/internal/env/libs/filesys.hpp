#pragma once

#include <Windows.h>
#include <filesystem>
#include <internal/utils.hpp>
#include <lstate.h>
#include <lualib.h>
#include <shlobj.h>


namespace Filesystem {
static char g_workspace[MAX_PATH] = "";

static const char *g_blocked_extensions[] = {
    ".exe",         ".bat",      ".cmd",      ".com",    ".pif",     ".scr",
    ".vbs",         ".vbe",      ".js",       ".jse",    ".wsf",     ".wsh",
    ".msi",         ".msp",      ".scf",      ".lnk",    ".inf",     ".reg",
    ".dll",         ".sys",      ".drv",      ".cpl",    ".ocx",     ".hta",

    ".ps1",         ".ps1xml",   ".ps2",      ".ps2xml", ".psc1",    ".psc2",
    ".psd1",        ".psm1",     ".pssc",

    ".msh",         ".msh1",     ".msh2",     ".mshxml", ".msh1xml", ".msh2xml",
    ".application", ".gadget",   ".msc",      ".jar",    ".sh",      ".bash",
    ".command",     ".csh",      ".ksh",      ".run",

    ".apk",         ".ipa",      ".app",      ".deb",    ".rpm",     ".dmg",
    ".pkg",         ".AppImage",

    ".out",         ".elf",      ".bin",      ".o",      ".so",      ".dylib",

    ".zip",         ".rar",      ".7z",       ".tar",    ".gz",      ".bz2",
    ".xz",          ".tgz",      ".tbz2",     ".tar.gz", ".tar.bz2", ".tar.xz",
    ".zipx",        ".arj",      ".cab",      ".iso",    ".img",     ".vhd",
    ".vhdx",        ".vmdk",     ".ova",      ".ovf",

    ".docm",        ".dotm",     ".xlsm",     ".xltm",   ".pptm",

    ".svg",         ".xsl",      ".xslt",     ".dtd",    ".xhtml",   ".xaml",
    ".xul",         ".rdf",      ".xsd",      ".xml",

    ".ahk",         ".action",   ".workflow", ".widget", ".xbe",     ".xex",
    ".url",         ".chm",      ".hlp",      ".cpl",    ".msc",     ".job",
    ".ws",          ".wsb",      ".torrent"};

static bool InitializeWorkspace() {
  char appdata[MAX_PATH];

  if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
    lstrcpyA(g_workspace, appdata);
    lstrcatA(g_workspace, "\\Leafy\\");

    CreateDirectoryA(g_workspace, NULL);

    char workspace_path[MAX_PATH];
    lstrcpyA(workspace_path, g_workspace);
    lstrcatA(workspace_path, "workspace\\");
    CreateDirectoryA(workspace_path, NULL);

    lstrcpyA(g_workspace, workspace_path);

    return true;
  }

  lstrcpyA(g_workspace, "C:\\Leafy\\workspace\\");
  CreateDirectoryA("C:\\Leafy", NULL);
  CreateDirectoryA(g_workspace, NULL);

  return false;
}

static void Init() {
  if (lstrlenA(g_workspace) == 0) {
    InitializeWorkspace();
  }
}

static const char *GetWorkspacePath() {
  if (lstrlenA(g_workspace) == 0) {
    InitializeWorkspace();
  }
  return g_workspace;
}

static BOOL MakePath(const char *rel, char *out, DWORD outlen) {
  if (!rel || !out || outlen < MAX_PATH)
    return FALSE;

  if (lstrlenA(g_workspace) == 0) {
    InitializeWorkspace();
  }

  DWORD wslen = lstrlenA(g_workspace);
  DWORD rellen = lstrlenA(rel);

  if (wslen + rellen >= outlen)
    return FALSE;

  lstrcpyA(out, g_workspace);
  lstrcatA(out, rel);

  for (DWORD i = 0; out[i]; i++) {
    if (out[i] == '/')
      out[i] = '\\';
  }

  return TRUE;
}

static void MakeDirs(const char *path) {
  if (!path)
    return;

  char tmp[MAX_PATH];
  lstrcpyA(tmp, path);

  for (DWORD i = 0; tmp[i]; i++) {
    if (tmp[i] == '\\' || tmp[i] == '/') {
      char c = tmp[i + 1];
      tmp[i + 1] = 0;
      CreateDirectoryA(tmp, NULL);
      tmp[i + 1] = c;
    }
  }
}

static bool IsExtensionBlocked(const char *path) {
  if (!path)
    return true;

  size_t len = lstrlenA(path);
  if (len < 4)
    return false;

  const char *dot = strrchr(path, '.');
  if (!dot)
    return false;

  for (const char *ext : g_blocked_extensions) {
    if (_stricmp(dot, ext) == 0) {
      return true;
    }
  }

  return false;
}

static int fs_readfile(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    luaL_error(L, ("readfile: argument 1 must be a string"));
    return 0;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    luaL_error(L, ("readfile: invalid path"));
    return 0;
  }

  HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  if (h == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
      luaL_error(L, ("readfile: file not found"));
    } else if (err == ERROR_ACCESS_DENIED) {
      luaL_error(L, ("readfile: access denied"));
    } else {
      luaL_error(L, ("readfile: failed to open file"));
    }
    return 0;
  }

  DWORD sz = GetFileSize(h, NULL);

  if (sz == INVALID_FILE_SIZE) {
    CloseHandle(h);
    luaL_error(L, ("readfile: failed to get file size"));
    return 0;
  }

  if (sz > 100 * 1024 * 1024) {
    CloseHandle(h);
    luaL_error(L, ("readfile: file too large"));
    return 0;
  }

  if (sz == 0) {
    CloseHandle(h);
    lua_pushlstring(L, "", 0);
    return 1;
  }

  LPVOID buf = VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!buf) {
    CloseHandle(h);
    luaL_error(L, ("readfile: out of memory"));
    return 0;
  }

  DWORD rd = 0;
  BOOL success = ReadFile(h, buf, sz, &rd, NULL);
  CloseHandle(h);

  if (!success || rd == 0) {
    VirtualFree(buf, 0, MEM_RELEASE);
    luaL_error(L, ("readfile: read failed"));
    return 0;
  }

  lua_pushlstring(L, (const char *)buf, rd);
  VirtualFree(buf, 0, MEM_RELEASE);
  return 1;
}

static int fs_writefile(lua_State *L) {
  if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
    luaL_error(L, ("invalid arguments"));
    return 2;
  }

  const char *rel = lua_tostring(L, 1);
  size_t len = 0;
  const char *data = lua_tolstring(L, 2, &len);

  char path[MAX_PATH];
  if (!MakePath(rel, path, MAX_PATH)) {
    luaL_error(L, ("invalid path"));
    return 2;
  }

  if (IsExtensionBlocked(path)) {
    luaL_error(L, ("file extension blocked for security"));
    return 2;
  }

  MakeDirs(path);

  HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    luaL_error(L, ("failed to create file"));
    return 2;
  }

  DWORD wr = 0;
  BOOL success = WriteFile(h, data, (DWORD)len, &wr, NULL);
  CloseHandle(h);

  if (!success || wr != len) {
    luaL_error(L, ("write failed"));
    return 2;
  }

  lua_pushboolean(L, 1);
  return 1;
}

static int fs_appendfile(lua_State *L) {
  if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
    lua_pushboolean(L, 0);
    luaL_error(L, ("invalid arguments"));
    return 2;
  }

  const char *rel = lua_tostring(L, 1);
  size_t len = 0;
  const char *data = lua_tolstring(L, 2, &len);

  char path[MAX_PATH];
  if (!MakePath(rel, path, MAX_PATH)) {
    lua_pushboolean(L, 0);
    luaL_error(L, ("invalid path"));
    return 2;
  }

  if (IsExtensionBlocked(path)) {
    lua_pushboolean(L, 0);
    luaL_error(L, ("file extension blocked for security"));
    return 2;
  }

  HANDLE h = CreateFileA(path, FILE_APPEND_DATA, 0, NULL, OPEN_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    lua_pushboolean(L, 0);
    luaL_error(L, ("failed to open file"));
    return 2;
  }

  DWORD wr = 0;
  BOOL success = WriteFile(h, data, (DWORD)len, &wr, NULL);
  CloseHandle(h);

  if (!success) {
    lua_pushboolean(L, 0);
    luaL_error(L, ("write failed"));
    return 2;
  }

  lua_pushboolean(L, 1);
  return 1;
}

static int fs_isfile(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    lua_pushboolean(L, 0);
    return 1;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    lua_pushboolean(L, 0);
    return 1;
  }

  DWORD a = GetFileAttributesA(path);
  lua_pushboolean(
      L, (a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY)) ? 1
                                                                           : 0);
  return 1;
}

static int fs_isfolder(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    lua_pushboolean(L, 0);
    return 1;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    lua_pushboolean(L, 0);
    return 1;
  }

  DWORD a = GetFileAttributesA(path);
  lua_pushboolean(
      L,
      (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) ? 1 : 0);
  return 1;
}

static int fs_makefolder(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    return 0;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    return 0;
  }

  MakeDirs(path);
  CreateDirectoryA(path, NULL);

  return 0;
}

static int fs_listfiles(lua_State *L) {
  lua_newtable(L);

  if (!lua_isstring(L, 1)) {
    return 1;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    return 1;
  }

  char search[MAX_PATH];
  lstrcpyA(search, path);
  lstrcatA(search, "\\*");

  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(search, &fd);

  if (h == INVALID_HANDLE_VALUE) {
    return 1;
  }

  int idx = 1;
  do {
    if (lstrcmpA(fd.cFileName, ".") == 0 || lstrcmpA(fd.cFileName, "..") == 0) {
      continue;
    }

    char out[MAX_PATH];
    lstrcpyA(out, rel);

    DWORD outlen = lstrlenA(out);
    if (outlen > 0 && out[outlen - 1] != '/' && out[outlen - 1] != '\\') {
      lstrcatA(out, "/");
    }

    lstrcatA(out, fd.cFileName);

    lua_pushstring(L, out);
    lua_rawseti(L, -2, idx++);

  } while (FindNextFileA(h, &fd));

  FindClose(h);
  return 1;
}

static int fs_delfile(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    return 0;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    return 0;
  }

  DeleteFileA(path);
  return 0;
}

static bool DeleteDirectoryRecursive(const char *dir_path) {
  char search_path[MAX_PATH];
  sprintf_s(search_path, "%s\\*", dir_path);

  WIN32_FIND_DATAA fd;
  HANDLE hFind = FindFirstFileA(search_path, &fd);

  if (hFind == INVALID_HANDLE_VALUE) {
    return false;
  }

  do {
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
      continue;
    }

    char full_path[MAX_PATH];
    sprintf_s(full_path, "%s\\%s", dir_path, fd.cFileName);

    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      DeleteDirectoryRecursive(full_path);
    } else {
      DeleteFileA(full_path);
    }

  } while (FindNextFileA(hFind, &fd));

  FindClose(hFind);
  return RemoveDirectoryA(dir_path) != 0;
}

static int fs_delfolder(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    return 0;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    return 0;
  }

  DeleteDirectoryRecursive(path);
  return 0;
}

static int fs_loadfile(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    lua_pushnil(L);
    luaL_error(L, ("expected string"));
    return 2;
  }

  const char *rel = lua_tostring(L, 1);
  char path[MAX_PATH];

  if (!MakePath(rel, path, MAX_PATH)) {
    lua_pushnil(L);
    luaL_error(L, ("path error"));
    return 2;
  }

  HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    lua_pushnil(L);
    luaL_error(L, ("not found"));
    return 2;
  }

  DWORD sz = GetFileSize(h, NULL);
  if (sz == INVALID_FILE_SIZE || sz == 0) {
    CloseHandle(h);
    lua_pushnil(L);
    luaL_error(L, ("empty"));
    return 2;
  }

  if (sz > 100 * 1024 * 1024) {
    CloseHandle(h);
    lua_pushnil(L);
    luaL_error(L, ("too large"));
    return 2;
  }

  LPVOID buf = VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!buf) {
    CloseHandle(h);
    lua_pushnil(L);
    luaL_error(L, ("memory"));
    return 2;
  }

  DWORD rd = 0;
  ReadFile(h, buf, sz, &rd, NULL);
  CloseHandle(h);

  int res = luau_load(L, rel, (const char *)buf, rd, 0);
  VirtualFree(buf, 0, MEM_RELEASE);

  if (res != 0) {
    lua_pushnil(L);
    lua_insert(L, -2);
    return 2;
  }

  return 1;
}

inline int getcustomasset(lua_State *L) {
  if (!lua_isstring(L, 1)) {
    lua_pushnil(L);
    return 1;
  }

  try {
    const char *relPath = lua_tostring(L, 1);
    if (!relPath || strlen(relPath) == 0) {
      lua_pushnil(L);
      return 1;
    }

    char fullPath[MAX_PATH];
    if (!MakePath(relPath, fullPath, MAX_PATH)) {
      lua_pushnil(L);
      return 1;
    }

    std::filesystem::path FullPath = fullPath;

    if (!std::filesystem::exists(FullPath) ||
        !std::filesystem::is_regular_file(FullPath)) {
      lua_pushnil(L);
      return 1;
    }

    auto customAssetsDir =
        std::filesystem::current_path() / "ExtraContent" / "leafy";

    if (!std::filesystem::exists(customAssetsDir)) {
      std::filesystem::create_directories(customAssetsDir);
    }

    auto customAssetsFile = customAssetsDir / FullPath.filename();

    std::filesystem::copy_file(
        FullPath, customAssetsFile,
        std::filesystem::copy_options::overwrite_existing);

    std::string Final =
        "rbxasset://leafy/" + customAssetsFile.filename().string();
    lua_pushlstring(L, Final.c_str(), Final.size());
    return 1;
  } catch (...) {
    lua_pushnil(L);
    return 1;
  }
}

static int fs_getworkspace(lua_State *L) {
  const char *workspace = GetWorkspacePath();
  lua_pushstring(L, workspace);
  return 1;
}

inline void RegisterLibrary(lua_State *L) {
  if (!L)
    return;

  Init();

  Utils::RegisterAliases(
      L, fs_readfile,
      {"readfile", "read_file", "readfromfile", "read_from_file"});

  Utils::RegisterAliases(
      L, fs_writefile,
      {"writefile", "write_file", "writetofile", "write_to_file"});

  Utils::RegisterAliases(L, fs_appendfile,
                         {"appendfile", "append_file", "appendtofile",
                          "append_to_file", "addtofile", "add_to_file"});

  Utils::RegisterAliases(L, fs_isfile,
                         {"isfile", "is_file", "fileexists", "file_exists"});

  Utils::RegisterAliases(L, fs_isfolder,
                         {"isfolder", "is_folder", "folderexists",
                          "folder_exists", "isdirectory", "is_directory",
                          "directoryexists", "directory_exists", "isdir",
                          "is_dir"});

  Utils::RegisterAliases(L, fs_makefolder,
                         {"makefolder", "make_folder", "createfolder",
                          "create_folder", "makedirectory", "make_directory",
                          "createdirectory", "create_directory", "makedir",
                          "make_dir", "createdir", "create_dir"});

  Utils::RegisterAliases(L, fs_listfiles,
                         {"listfiles", "list_files", "getfiles", "get_files",
                          "listdir", "list_dir"});

  Utils::RegisterAliases(L, fs_delfile,
                         {"delfile", "del_file", "deletefile", "delete_file",
                          "removefile", "remove_file"});

  Utils::RegisterAliases(L, fs_delfolder,
                         {"delfolder", "del_folder", "deletefolder",
                          "delete_folder", "removefolder", "remove_folder",
                          "deldirectory", "del_directory", "deletedirectory",
                          "delete_directory", "removedirectory",
                          "remove_directory", "deldir", "del_dir", "deletedir",
                          "delete_dir", "removedir", "remove_dir"});

  Utils::RegisterAliases(L, fs_loadfile,
                         {"loadfile", "load_file", "loadfromfile",
                          "load_from_file", "dofile", "do_file"});

  Utils::RegisterAliases(
      L, getcustomasset,
      {"getcustomasset", "get_custom_asset", "getsynasset", "get_syn_asset"});

  Utils::RegisterAliases(L, fs_getworkspace,
                         {"getworkspace", "get_workspace", "getworkspacepath",
                          "get_workspace_path"});
}
} // namespace Filesystem