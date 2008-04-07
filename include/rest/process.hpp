// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#ifndef REST_PROCESS_HPP
#define REST_PROCESS_HPP

#include "config.hpp"
#include <vector>
#include <string>

namespace rest { namespace process {

std::vector<char*> getargs(std::string const &path, std::string &data);
void restart();
bool set_gid(gid_t gid);
bool set_uid(uid_t uid);
void drop_privileges(utils::property_tree const &tree);
void maybe_daemonize(utils::property_tree const &tree);
void chroot(utils::property_tree const &tree);

}}

#endif
