def options(opt):
    opt.load('compiler_cxx')


def configure(cfg):
    cfg.load('compiler_cxx')
    cfg.env.CXXFLAGS += [
        '-std=c++17', '-ggdb3', '-Wall', '-Wpedantic', '-Werror']
    p = dict(mandatory=True, args='--cflags --libs')
    cfg.check_cfg(package='libzmq', uselib_store='ZMQ', **p)
    cfg.check(features='cxx cxxprogram', lib=['pthread'],
              uselib_store='PTHREAD')
    assert('ZMQ_BUILD_DRAFT_API=1' in cfg.env.DEFINES_ZMQ)

def build(bld):
    houses=['grasslands', 'strawhouse', 'woodhouse',
            'stonehouse']
    for house in houses:
        bld.program(features = 'cxx',
                    source = [house+'.cpp'],
                    target = house,
                    ut_cwd = bld.path,
                    install_path = None,
                    includes = '.',
                    use = ['ZMQ', 'PTHREAD'])
