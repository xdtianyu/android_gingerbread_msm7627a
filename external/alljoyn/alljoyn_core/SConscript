# Copyright 2010 - 2011, Qualcomm Innovation Center, Inc.
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#        http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
# 

Import('env')

# Indicate that this SConscript file has been loaded already
env['_ALLJOYNCORE_'] = True

# Dependent Projects
common_hdrs, common_objs = env.SConscript(['../common/SConscript'])
if env['OS'] == 'windows' or env['OS'] == 'android':
    env.SConscript(['../stlport/SConscript'])

# manually add dependencies for xml to h, and for files included in the xml
env.Depends('inc/Status.h', 'src/Status.xml');
env.Depends('inc/Status.h', '../common/src/Status.xml');

# Add support for multiple build targets in the same workset
env.VariantDir('$OBJDIR', 'src', duplicate = 0)
env.VariantDir('$OBJDIR/test', 'test', duplicate = 0)
env.VariantDir('$OBJDIR/daemon', 'daemon', duplicate=0)
env.VariantDir('$OBJDIR/samples', 'samples', duplicate = 0)

# AllJoyn Install
env.Install('$OBJDIR', env.File('src/Status.xml'))
env.Status('$OBJDIR/Status')
env.Install('$DISTDIR/inc', env.File('inc/Status.h'))
env.Install('$DISTDIR/inc/alljoyn', env.Glob('inc/alljoyn/*.h'))
for d,h in common_hdrs.items():
    env.Install('$DISTDIR/inc/%s' % d, h)

# Header file includes
env.Append(CPPPATH = [env.Dir('inc')])

# Make private headers available
env.Append(CPPPATH = [env.Dir('src')])

# AllJoyn Libraries
libs = env.SConscript('$OBJDIR/SConscript', exports = ['common_objs'])
dlibs = env.Install('$DISTDIR/lib', libs)
env.Append(LIBPATH = [env.Dir('$DISTDIR/lib')])
env.Prepend(LIBS = dlibs)

# AllJoyn Daemon
daemon_progs = env.SConscript('$OBJDIR/daemon/SConscript')
env.Install('$DISTDIR/bin', daemon_progs)

# Test programs
progs = env.SConscript('$OBJDIR/test/SConscript')
env.Install('$DISTDIR/bin', progs)

# Sample programs
progs = env.SConscript('$OBJDIR/samples/SConscript')
env.Install('$DISTDIR/bin/samples', progs)

# Release notes and misc. legals
env.Install('$DISTDIR', 'docs/ReleaseNotes.txt')
env.InstallAs('$DISTDIR/README.txt', 'docs/README.android')

env.Install('$DISTDIR', 'README.md')
env.Install('$DISTDIR', 'NOTICE')

# Whitespace policy
if env['WS'] != 'off' and not env.GetOption('clean'):
    import sys
    sys.path.append('../build_core/tools/bin')
    import whitespace
    
    def wsbuild(target, source, env):
        print "Evaluating whitespace compliance..."
        print "Note: enter 'scons -h' to see whitespace (WS) options"
        return whitespace.main(env['WS'])
        
    env.Command('ws', Dir('$DISTDIR'), wsbuild)

# Build docs
if env['DOCS'] == 'html':
    print("*******************************************************************")
    print("* The files created by doxygen will not be removed when using     *")
    print("* scons' -c option. The docs/html folder must manually be deleted *")
    print("* to remove the files created by doxygen.                         *")
    print("*******************************************************************")
    doxy_bld = Builder(action = 'cd ${SOURCE.dir} && doxygen ${SOURCE.file}')
    env.Append(BUILDERS = {'Doxygen': doxy_bld})
    # the target directory 'docs/tmp' is never built this will cause doxygen 
    # to run every time DOCS == 'html'
    env.Doxygen(source='docs/Doxygen_html', target=Dir('docs/tmp'))
elif env['DOCS'] == 'dev':
    print("*******************************************************************")
    print("* The files created by doxygen will not be removed when using     *")
    print("* scons' -c option. The docs/html folder must manually be deleted *")
    print("* to remove the files created by doxygen.                         *")
    print("*******************************************************************")
    doxy_bld = Builder(action = 'cd ${SOURCE.dir} && doxygen ${SOURCE.file}')
    env.Append(BUILDERS = {'Doxygen': doxy_bld})
    # the target directory 'docs/tmp' is never built this will cause doxygen 
    # to run every time DOCS == 'html'
    env.Doxygen(source='docs/Doxygen_dev', target=Dir('docs/tmp'))
elif env['DOCS'] == 'pdf':
    print("*******************************************************************")
    print("* The files created by doxygen will not be removed when using     *")
    print("* scons' -c option. The docs/latex folder must manually be deleted*")
    print("* to remove the files created by doxygen.                         *")
    print("*******************************************************************")
    doxy_bld = Builder(action = 'cd ${SOURCE.dir} && doxygen ${SOURCE.file}')
    env.Append(BUILDERS = {'Doxygen': doxy_bld})
    # the target directory 'docs/tmp' is never built this will cause doxygen 
    # to run every time DOCS == 'pdf' making refman.tex a target makes sure this
    # command is run before building the pdf output.
    env.Doxygen(source='docs/Doxygen_pdf', target=[Dir('docs/tmp'), File('docs/latex/refman.tex')])
    #copy the custom style to the latex folder generated by doxygen.
    Command('./docs/latex/quic.sty','./docs/quic.sty',Copy("$TARGET", "$SOURCE"))
    env.PDF('./docs/latex/refman.pdf', './docs/latex/refman.tex')
    
