#!/bin/bash

sed -i 's/#define from_kuid_munged(user_ns, uid) ((uid))/#define from_kuid_munged(user_ns, uid) ((uid.val))/' /usr/share/systemtap/runtime/linux/runtime.h
sed -i 's/#define from_kgid_munged(user_ns, gid) ((gid))/#define from_kgid_munged(user_ns, gid) ((gid.val))/' /usr/share/systemtap/runtime/linux/runtime.h
sed -i 's/f_dentry/f_path.dentry/g' /usr/share/systemtap/runtime/linux/task_finder2.c


