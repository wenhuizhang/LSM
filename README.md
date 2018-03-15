# LSM
2.5.29 released patch for linux security module on July 27, 2002

## Patch Info
patch created between 2.5.28 and 2.5.29, 
```
$ diff -ur /Users/wenhuizhang/Downloads/linux-2.5.28 /Users/wenhuizhang/Downloads/linux-2.5.29 > 29.patch
$ sudo patch -p0 -d /Users/wenhuizhang/Downloads/linux-2.5.28 < 29.patch 
```

To see the diff: 
https://github.com/wenhuizhang/LSM/commit/904f3c7e371f71a4d4df06b2fffc32706bd2dd90

#### Memo
```
2.5.29 released July 27, 2002:

Strict address space accounting
Add Linux Security Module (LSM)

Thread-Local Storage (TLS) support
```
