// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include <rest/process.hpp>
#include <rest/logger.hpp>
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

void rest::process::drop_privileges(logger *log, utils::property_tree const &tree) {
  long gid = utils::get(tree, -1, "general", "gid");
  if (gid != -1) {
    set_gid(gid);
    log->log(logger::notice, "gid", gid);
  } else {
    log->log(logger::warning, "no-gid-set");
  }

  long uid = utils::get(tree, -1, "general", "uid");
  if (uid != -1) {
    set_uid(uid);
    log->log(logger::notice, "uid", uid);
  } else {
    log->log(logger::warning, "no-uid-set");
  }

  log->flush();
}

void rest::process::maybe_daemonize(logger *log, utils::property_tree const &tree) {
  bool daemon = utils::get(tree, false, "general", "daemonize");

  log->log(logger::info, "daemon", daemon);
  log->flush();

  if (!daemon)
    return;

  if(::daemon(1, 1) == -1)
    throw utils::errno_error("daemonizing the server failed (daemon)");

  log->log(logger::info, "daemon-started", true);
  log->flush();
}

void rest::process::chroot(logger *log, utils::property_tree const &tree) {
  std::string dir = utils::get(tree, std::string(), "general", "chroot");
  if (dir.empty()) {
    log->log(logger::warning, "no-chroot-set");
    log->flush();
    return;
  }

  if(::chroot(".") == -1) {
    if(errno != EPERM)
      throw utils::errno_error("chroot failed");
    log->log(logger::warning, "chroot-error","insufficient permissions");
  }
}
