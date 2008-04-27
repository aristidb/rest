#!/usr/bin/env python

VERSION='dev'
APPNAME='librest'

srcdir = '.'
blddir = 'build'

def init(): pass

def set_options(opt):
    opt.tool_options('g++')
    opt.tool_options('boost2')

def configure(conf):
    conf.env['CXXFLAGS'] = '-pipe -Wno-long-long -Wall -W -pedantic -std=c++98 -DBOOST_SP_DISABLE_THREADS -Iinclude -Itestsoon/include'
    conf.env['CXXFLAGS_DEBUG'] = '-g3 -ggdb3 -DDEBUG'
    conf.env['CXXFLAGS_OPTIMIZED'] = '-O3 -DNDEBUG'
    
    conf.check_tool('g++')
    conf.check_tool('boost2')

    boostconf = conf.create_boost_configurator()
    boostconf.lib = ['iostream', 'filesystem', 'system']
    boostconf.static = 'onlystatic'
    boostconf.run()
    
    pkgconf = conf.create_pkgconfig_configurator()
    pkgconf.uselib = 'GNUTLS'
    pkgconf.name = 'gnutls'
    pkgconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.uselib = 'Z'
    libconf.name   = 'z'
    libconf.path = ['/usr/lib','/usr/local/lib','/sw/lib','/opt/local/lib']
    libconf.mandatory = 1
    libconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.uselib = 'BZ2'
    libconf.name   = 'bz2'
    libconf.path = ['/usr/lib','/usr/local/lib','/sw/lib','/opt/local/lib']
    libconf.mandatory = 1
    libconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.uselib = 'GCRYPT'
    libconf.name   = 'gcrypt'
    libconf.path = ['/opt/local/lib','/usr/lib','/usr/local/lib','/sw/lib']
    libconf.mandatory = 1
    libconf.run()

def build(bld):
    bld.add_subdirs('sandbox test')
    #bld.add_subdirs('src test sandbox')

def shutdown(): pass
