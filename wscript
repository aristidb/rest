import sys

VERSION='dev'
APPNAME='librest'

srcdir = '.'
blddir = 'build'

def init(): pass

def set_options(opt):
    opt.tool_options('g++')
    opt.tool_options('boost2')

def configure(conf):
    darwin = sys.platform.startswith('darwin')
    conf.check_message('platform', '', 1, sys.platform)

    conf.env['CXXFLAGS'] = '-pipe -Wno-long-long -Wall -W -pedantic -std=c++98 -DBOOST_SP_DISABLE_THREADS -Iinclude -Itestsoon/include '
    if darwin:
        conf.env['CXXFLAGS'] += ' -DAPPLE '
    conf.env['CXXFLAGS_DEBUG'] = '-g3 -ggdb3 -DDEBUG '
    conf.env['CXXFLAGS_OPTIMIZED'] = '-O3 -DNDEBUG '
    if darwin:
        conf.env['CXXFLAGS_OPTIMIZED'] += ' -fast '
    else:
        conf.env['FULLSTATIC'] = True
    
    conf.check_tool('g++')
    conf.check_tool('boost2')

    boostconf = conf.create_boost_configurator()
    boostconf.lib = ['iostreams', 'filesystem', 'system']
    if darwin:
        boostconf.static = 'nostatic'
    else:
        boostconf.static = 'onlystatic'
    boostconf.threadingtag = 'st'
    boostconf.run()
    
    pkgconf = conf.create_pkgconfig_configurator()
    pkgconf.uselib = 'GNUTLS'
    pkgconf.name = 'gnutls'
    if not darwin:
        pkgconf.static = True
    pkgconf.run()

    libconf = conf.create_library_configurator()
    libconf.uselib = 'GPGERR'
    libconf.name = 'gpg-error'
    libconf.path = ['/usr/lib', '/usr/local/lib', '/sw/lib', '/opt/local/lib']
    libconf.mandatory = 1
    libconf.static = True
    libconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.uselib = 'Z'
    libconf.name   = 'z'
    libconf.path = ['/usr/lib','/usr/local/lib','/sw/lib','/opt/local/lib']
    libconf.mandatory = 1
    libconf.static = True
    libconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.uselib = 'BZ2'
    libconf.name   = 'bz2'
    libconf.path = ['/usr/lib','/usr/local/lib','/sw/lib','/opt/local/lib']
    libconf.mandatory = 1
    libconf.static = True
    libconf.run()

def build(bld):
    bld.add_subdirs('src test sandbox')

def shutdown(): pass
