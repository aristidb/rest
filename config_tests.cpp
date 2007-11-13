// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
#include "rest/config.hpp"
#include <testsoon.hpp>

using namespace rest;
using namespace rest::utils;

TEST_GROUP(config) {
  TEST_GROUP(set) {
    TEST(string) {
      property_tree t;
      set(t, "foo", "foo");
      property_tree::property_iterator j = t.find_property("foo");
      Check(j != t.property_end());
      Equals(j->data(), "foo");
    }

    TEST(integer) {
      property_tree t;
      set(t, 10, "foo");
      property_tree::property_iterator j = t.find_property("foo");
      Check(j != t.property_end());
      Equals(j->data(), "10");
    }

    TEST(deep) {
      property_tree t;
      set(t, 10, "foo", "bar", "baz", "bam");
      property_tree::children_iterator j0 = t.find_children("foo");
      Check(j0 != t.children_end());
      property_tree::children_iterator j1 = (*j0)->find_children("bar");
      Check(j1 != (*j0)->children_end());
      property_tree::children_iterator j2 = (*j1)->find_children("baz");
      Check(j2 != (*j1)->children_end());
      property_tree::property_iterator j3 = (*j2)->find_property("bam");
      Check(j3 != (*j2)->property_end());
      Equals(j3->data(), "10");
    }

    TEST(override) {
      property_tree t;
      set(t, 10, "foo");
      property_tree::property_iterator j = t.find_property("foo");
      Check(j != t.property_end());
      Equals(j->data(), "10");
      set(t, 10, "foo");
      j = t.find_property("foo");
      Check(j != t.property_end());
      Equals(j->data(), "10");
    }
  }

  TEST_GROUP(add_path) {
    TEST(flat) {
      property_tree t;
      add_path(t, "foo");
      property_tree::children_iterator j = t.find_children("foo");
      Check(j != t.children_end());
    }

    TEST(deep) {
      property_tree t;
      add_path(t, "foo", "bar", "baz");
      property_tree::children_iterator j0 = t.find_children("foo");
      Check(j0 != t.children_end());
      property_tree::children_iterator j1 = (*j0)->find_children("bar");
      Check(j1 != (*j0)->children_end());
      property_tree::children_iterator j2 = (*j1)->find_children("baz");
      Check(j2 != (*j1)->children_end());
    }

    TEST() {
      property_tree t;
      add_path(t, "foo", "fou");
      property_tree::children_iterator j = t.find_children("foo");
      Check(j != t.children_end());
      property_tree::children_iterator k = (*j)->find_children("fou");
      Check(k != t.children_end());
      add_path(t, "foo", "bar", "baz");
      property_tree::children_iterator j0 = t.find_children("foo");
      Check(j0 != t.children_end());
      property_tree::children_iterator j1 = (*j0)->find_children("bar");
      Check(j1 != (*j0)->children_end());
      property_tree::children_iterator j2 = (*j1)->find_children("baz");
      Check(j2 != (*j1)->children_end());
    }
  }

  TEST_GROUP(get) {
    TEST() {
      property_tree t;
      set(t, 10, "foo");
      int got = get(t, 0, "foo");
      Equals(got, 10);
    }

    TEST() {
      property_tree t;
      int got = get(t, 112433, "foo", "bar", "baz");
      Equals(got, 112433);
    }
  }

  TEST_GROUP(misc) {
    TEST() {
      property_tree t;
      set(t, "4", "x",  "a");
      Equals(get(t, "x", "a"), "x");
    }
  }
}
