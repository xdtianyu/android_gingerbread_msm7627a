; Copyright (C) 2008 The Android Open Source Project
;
; Licensed under the Apache License, Version 2.0 (the "License");
; you may not use this file except in compliance with the License.
; You may obtain a copy of the License at
;
;      http://www.apache.org/licenses/LICENSE-2.0
;
; Unless required by applicable law or agreed to in writing, software
; distributed under the License is distributed on an "AS IS" BASIS,
; WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
; See the License for the specific language governing permissions and
; limitations under the License.

.source T_invokestatic_10.java
.class public dxc/junit/opcodes/invokestatic/jm/T_invokestatic_10
.super java/lang/Object

.field static v I

.method static <clinit>()V
    .limit stack 2
    .limit locals 0

    ldc 1
    putstatic dxc.junit.opcodes.invokestatic.jm.T_invokestatic_10.v I
    return
.end method


.method public <init>()V
    aload_0
    invokespecial java/lang/Object/<init>()V
    return
.end method

.method public run()I
    invokestatic dxc/junit/opcodes/invokestatic/jm/T_invokestatic_10/<clinit>()V
    ireturn
.end method