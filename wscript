import sys, os

VERSION='dev'
APPNAME='librest'

srcdir = '.'
blddir = 'build'

def init(): pass

def set_options(opt):
    opt.tool_options('g++')
    opt.tool_options('boost')

def configure(conf):
    darwin = sys.platform.startswith('darwin')
    conf.check_message('platform', '', 1, sys.platform)

    conf.env['CXXFLAGS'] = '-pipe -Wno-long-long -Wall -W -pedantic -std=c++98'
    conf.env.append_unique('CXXDEFINES', 'BOOST_SP_DISABLE_THREADS')
    if darwin:
        conf.env.append_unique('CXXDEFINES', 'APPLE')
    conf.env['CXXFLAGS_DEBUG'] = '-g3 -ggdb3 -DDEBUG'
    conf.env['CXXFLAGS_OPTIMIZED'] = '-O3 -DNDEBUG'
    if darwin:
        conf.env['CXXFLAGS_OPTIMIZED'] += ' -fast'
    else:
        conf.env['FULLSTATIC'] = True
    
    conf.check_tool('g++')
    conf.check_tool('misc')
    conf.check_tool('boost')

    boostconf = conf.create_boost_configurator()
    boostconf.lib = ['iostreams', 'filesystem', 'system']
    if darwin:
        boostconf.static = 'nostatic'
    else:
        boostconf.static = 'onlystatic'
    boostconf.threadingtag = 'st'
    boostconf.run()
    
    pkgconf = conf.create_pkgconfig_configurator()
    pkgconf.name = 'gnutls'
    if not darwin:
        pkgconf.static = True
    pkgconf.run()

    libconf = conf.create_library_configurator()
    libconf.name = 'gcrypt'
    libconf.path = ['/usr/lib', '/usr/local/lib', '/sw/lib', '/opt/local/lib']
    libconf.mandatory = 1
    if not darwin:
        libconf.static = True
    libconf.run()

    libconf = conf.create_library_configurator()
    libconf.name = 'gpg-error'
    libconf.path = ['/lib', '/usr/lib', '/usr/local/lib', '/sw/lib', '/opt/local/lib']
    libconf.mandatory = 1
    if not darwin:
        libconf.static = True
    libconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.name   = 'z'
    libconf.path = ['/usr/lib','/usr/local/lib','/sw/lib','/opt/local/lib']
    libconf.mandatory = 1
    if not darwin:
        libconf.static = True
    libconf.run()
    
    libconf = conf.create_library_configurator()
    libconf.name   = 'bz2'
    libconf.path = ['/usr/lib','/usr/local/lib','/sw/lib','/opt/local/lib']
    libconf.mandatory = 1
    if not darwin:
        libconf.static = True
    libconf.run()

def build_pkgconfig(bld):
    obj = bld.new_task_gen('subst')
    obj.source = 'rest.pc.in'
    obj.target = 'rest.pc'
    obj.dict = {
        'PREFIX': bld.env['PREFIX'],
        'LIBDIR': os.path.normpath(bld.env['PREFIX'] + '/lib'),
        'INCLUDEDIR': os.path.normpath(bld.env['PREFIX'] + '/include'),
        'VERSION': VERSION
        }
    obj.inst_var = bld.env['PREFIX']
    obj.inst_dir = '/lib/pkgconfig/'
    obj.apply()
    

def build(bld):
    bld.add_subdirs('src test sandbox')
    build_pkgconfig(bld)
    bld.install_files('PREFIX', 'include/rest/', 'include/rest/*.hpp')
    bld.install_files('PREFIX', 'include/rest/utils/', 'include/rest/utils/*.hpp')
    bld.install_files('PREFIX', 'include/rest/utils/', 'include/rest/utils/*.h')
    bld.install_files('PREFIX', 'include/rest/encodings/', 'include/rest/encodings/*.hpp')
    bld.install_files('PREFIX', '/lib/pkgconfig', 'rest.pc')

def shutdown(): pass
