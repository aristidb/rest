// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
/*
  testsoon.hpp: "Test soon" testing framework.

  Copyright (C) 2006 Aristid Breitkreuz, Ronny Pfannschmidt and 
                     Benjamin Bykowski

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Aristid Breitkreuz aribrei@arcor.de
  Ronny Pfannschmidt Ronny.Pfannschmidt@gmx.de
  Benjamin Bykowski bennybyko@gmx.de
*/

#ifndef TESTSOON_XML_REPORTER_HPP
#define TESTSOON_XML_REPORTER_HPP

#include "../testsoon.hpp"

namespace testsoon {

class xml_reporter : public test_reporter {
  public:
    typedef stream_class stream;
    xml_reporter(stream &out) : out(out), indent(1) {}

    void start() {
      out << "<?xml version=\"1.0\"?>\n";
      out << "<testsoon>\n";
    }
    void stop() {
      out << "</testsoon>\n";
    }

    void begin_group(test_group const &group) {
      if (group.parent) {
        print_ind();
        ++indent;
        if (group.parent->parent)
          out << "<group";
        else
          out << "<file";
        out << " name=\"" << group.name << "\">\n";
      }
    }

    void end_group(test_group const &group) {
      if (group.parent) {
        --indent;
        print_ind();
        if (group.parent->parent)
          out << "</group>\n";
        else
          out << "</file>\n";
      }
    }

    void success(test_info const &i, string const &k) {
      print_ind() << "<success line=\"" << i.line << "\"";      
      if (*i.name)
        out << " name=\"" << i.name << "\"";
      if (k.empty())
        out << " />\n";
      else {
        out << ">\n";
        ++indent;
        print_ind() << "<generator>" << k << "</generator>\n";
        --indent;
        print_ind() << "</success>\n";
      }
    }
    
    void failure(test_info const &i, test_failure const &x, string const &k) {
      print_ind() << "<failure line=\"" << i.line << "\"";
      if (*i.name)
        out << " name=\"" << i.name << "\"";
      out << ">\n";
      ++indent;

      print_ind() << "<problem>" << x.message << "</problem>\n";
      if (!x.data.empty()) {
        print_ind() << "<rawdata>\n";
        ++indent;
        for (string_vector::const_iterator it = x.data.begin(); 
            it != x.data.end();
            ++it) {
          print_ind() << "<item>" << *it << "</item>\n";
        } 
        --indent;
        print_ind() << "</rawdata>\n";
      }

      if (!k.empty()) {
        print_ind() << "<generator>" << k << "</generator>\n";
      }

      --indent;
      print_ind() << "</failure>\n";
    } 

  private:
    stream_class &print_ind() {
      for (unsigned i = 0; i < indent; ++i)
        out << "  ";
      return out;
    }

    stream &out;
    unsigned indent;
};

}

#endif
