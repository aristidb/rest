// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/process.hpp>
#include <rest/utils/log.hpp>
#include <rest/utils/exceptions.hpp>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <grp.h>

std::vector<char*> rest::process::getargs(
    std::string const &path, std::string &data)
{
  std::ifstream in(path.c_str());
  data.assign(std::istreambuf_iterator<char>(in.rdbuf()),
              std::istreambuf_iterator<char>());

  std::vector<char*> ret;
  ret.push_back(&data[0]);
  std::size_t const size = data.size();

  for(std::size_t i = 0; i < size; ++i)
    if(data[i] == '\0' && i+1 < size)
      ret.push_back(&data[i+1]);
  ret.push_back(0);

  return ret;
}

void rest::process::restart() {
  utils::log(LOG_NOTICE, "server restart");

  std::string cmdbuffer;
  std::string envbuffer;

  char resolved_cmd[8192];
  int n = readlink("/proc/self/exe", resolved_cmd, sizeof(resolved_cmd) - 1);
  if (n < 0) {
    utils::log(LOG_ERR, "restart failed: readlink: %m");
    return;
  }
  resolved_cmd[n] = '\0';

  if(::execve(resolved_cmd, &getargs("/proc/self/cmdline", cmdbuffer)[0],
              &getargs("/proc/self/environ", envbuffer)[0]) == -1)
  {
    utils::log(LOG_ERR, "restart failed: execve: %m");
  }
}

bool rest::process::set_gid(gid_t gid) {
  if(::setgroups(1, &gid) == -1) {
    if(errno != EPERM)
      throw utils::errno_error("setgroups failed");
    return false;
  }
  return true;
}

bool rest::process::set_uid(uid_t uid) {
  if(::setuid(uid) == -1) {
    if(errno != EPERM)
      throw utils::errno_error("setuid failed");
    return false;
  }
  return true;
}

void rest::process::drop_privileges(utils::property_tree const &tree) {
  long gid = utils::get(tree, -1, "general", "gid");
  if(gid != -1)
    set_gid(gid);
  else
    utils::log(LOG_WARNING, "no gid set: group privileges not droped");

  long uid = utils::get(tree, -1, "general", "uid");
  if(uid != -1)
    set_uid(uid);
  else
    utils::log(LOG_WARNING, "no uid set: user privilieges not droped");
}

void rest::process::maybe_daemonize(utils::property_tree const &tree) {
  bool daemon = utils::get(tree, false, "general", "daemonize");
  utils::log(LOG_INFO, "starting daemon: %s", daemon ? "yes" : "no");
  if (!daemon)
    return;

  if(::daemon(1, 1) == -1)
    throw utils::errno_error("daemonizing the server failed (daemon)");
  utils::log(LOG_INFO, "daemon started");
}

void rest::process::chroot(utils::property_tree const &tree) {
  std::string dir = utils::get(tree, std::string(), "general", "chroot");
  if (dir.empty()) {
    utils::log(LOG_WARNING, "no chroot set");
    return;
  }

  if(::chroot(".") == -1) {
    if(errno != EPERM)
      throw utils::errno_error("chroot failed");
    else
      utils::log(LOG_WARNING, "could not chroot: insufficient permissions");
  }
}
