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

import os
Import('env')

# Platform specifics for common
if env['OS'] == 'windows':
    vars = Variables()
    vars.Add(PathVariable('OPENSSL_BASE', 'Base OpenSSL directory (windows only)', os.environ.get('OPENSSL_BASE')))
    vars.Update(env)
    Help(vars.GenerateHelpText(env))

    if '' == env.subst('$OPENSSL_BASE'):
        print 'Missing required build variable OPENSSL_BASE'
        Exit()
    else:
        env.Append(CPPPATH = ['$OPENSSL_BASE/include'])
        env.Append(LIBPATH = ['$OPENSSL_BASE/lib'])
        env.AppendUnique(LIBS = ['libeay32', 'ssleay32'])
        env.AppendUnique(LIBS = ['setupapi', 'user32', 'winmm', 'ws2_32', 'iphlpapi', 'secur32', 'Advapi32'])
elif env['OS'] == 'linux':
    env.AppendUnique(LIBS =['rt', 'stdc++', 'pthread', 'crypto'])
elif env['OS'] == 'android':
    env.AppendUnique(LIBS = ['m', 'c', 'stdc++', 'crypto', 'log', 'gcc'])
elif env['OS'] == 'android_donut':
    env.AppendUnique(LIBS = ['m', 'c', 'stdc++', 'crypto', 'log'])
elif env['OS'] == 'maemo':
    pass
else:
    print 'Unrecognized OS in common: ' + env.subst('$OS')
    Exit()

env.AppendUnique(CPPDEFINES = ['QCC_OS_GROUP_%s' % env['OS_GROUP'].upper()])

# Variant settings
env.VariantDir('$OBJDIR', 'src', duplicate = 0)
env.VariantDir('$OBJDIR/os', 'os/${OS_GROUP}', duplicate = 0)
env.VariantDir('$OBJDIR/test', 'test', duplicate = 0)

# Setup dependent include directorys
hdrs = { 'qcc': env.File(['inc/qcc/Log.h',
                          'inc/qcc/ManagedObj.h',
                          'inc/qcc/String.h',
                          'inc/qcc/atomic.h',
                          'inc/qcc/SocketWrapper.h',
                          'inc/qcc/platform.h']),
         'qcc/${OS_GROUP}': env.File(['inc/qcc/${OS_GROUP}/atomic.h',
                                      'inc/qcc/${OS_GROUP}/platform_types.h',
                                      'inc/qcc/${OS_GROUP}/unicode.h']) }

if env['OS'] == 'windows':
    hdrs['qcc/${OS_GROUP}'] += env.File(['inc/qcc/${OS_GROUP}/mapping.h'])

env.Append(CPPPATH = [env.Dir('inc')])

# Build the sources
srcs = env.Glob('$OBJDIR/*.cc') + env.Glob('$OBJDIR/os/*.cc')
objs = env.Object(srcs)

# Test programs
progs = env.SConscript('$OBJDIR/test/SConscript')
env.Install('$DISTDIR/bin', progs)

ret = (hdrs, objs)

Return('ret')
