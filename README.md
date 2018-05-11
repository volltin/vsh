# 编写 Shell 程序
- [编写 Shell 程序](#%E7%BC%96%E5%86%99-shell-%E7%A8%8B%E5%BA%8F)
    - [预备知识](#%E9%A2%84%E5%A4%87%E7%9F%A5%E8%AF%86)
        - [文件描述符](#%E6%96%87%E4%BB%B6%E6%8F%8F%E8%BF%B0%E7%AC%A6)
            - [简介](#%E7%AE%80%E4%BB%8B)
            - [特殊文件描述符](#%E7%89%B9%E6%AE%8A%E6%96%87%E4%BB%B6%E6%8F%8F%E8%BF%B0%E7%AC%A6)
        - [一些系统调用](#%E4%B8%80%E4%BA%9B%E7%B3%BB%E7%BB%9F%E8%B0%83%E7%94%A8)
            - [fork()](#fork)
            - [exec(file)](#execfile)
            - [open(file, flag)](#openfile-flag)
            - [close(fd)](#closefd)
            - [dup(fd)](#dupfd)
            - [pipe(int fds[2])](#pipeint-fds2)
            - [read(fd, buff, len)](#readfd-buff-len)
            - [write(fd, buff, len)](#writefd-buff-len)
            - [wait(status)](#waitstatus)
            - [waitpid(pid, status, options)](#waitpidpid-status-options)
        - [子进程的退出](#%E5%AD%90%E8%BF%9B%E7%A8%8B%E7%9A%84%E9%80%80%E5%87%BA)
    - [功能实现](#%E5%8A%9F%E8%83%BD%E5%AE%9E%E7%8E%B0)
    - [参考资料](#%E5%8F%82%E8%80%83%E8%B5%84%E6%96%99)

## 预备知识

这一小节主要是根据网上资料和我的一些理解编写的预备知识，同时提供一些简短的例子来理解这些知识。

### 文件描述符

文件描述符（file descriptor），是一个用于表述指向文件的引用的抽象化概念。

#### 简介

文件描述符在形式上是一个非负整数。实际上，它是一个索引值，指向内核为每一个进程所维护的该进程打开文件的记录表。当程序打开一个现有文件或者创建一个新文件时，内核向进程返回一个文件描述符。

#### 特殊文件描述符

**0, 1, 2** 分别代表了 **stdin**, **stdout** 和 **stderr**。

### 一些系统调用

#### fork()

创建一个新进程。

fork 系统调用会创建当前进程的另一个副本，并将新进程标记为调用 fork 的父进程的子进程。 这个系统调用在子进程中返回零，并在父进程中返回子进程的 pid，如果 fork 失败，会返回一个负值。 它复制了父进程包括文件描述符和虚拟内存在内的所有内容。 如果进程尝试写入虚拟内存，它将在写入时执行一个副本（COW）为该进程创建该页面的副本。

eg. 输出两次

```c
fork();
printf("hjy\n");
```

eg. 输出八次 (2^3 = 8)

```c
fork();
fork();
fork();
printf("hjy\n");
```

eg. 判断父进程还是子进程

```c
pid_t pid = fork();
if (pid == 0) {
    printf("child\n");
} else {
    printf("parent\n");
}
```

#### exec(file)

运行可执行文件。

用一个可执行的二进制文件覆盖当前进程。 例如。 如果运行 `exec("/bin/sh")` 。 它将用 `/bin/sh` 中的二进制覆盖当前内存中的代码并执行。并且文件描述符表保持与原始进程相同。

#### open(file, flag)

打开文件并创建与该文件关联的文件描述符。

并且，内核会分配一个当前没有用过的最小的 fd 编号。

eg. 打开一个文件，如果不存在就创建它

```c
int fd = open("hjy", O_RDONLY | O_CREAT); 
```

#### close(fd)

关闭打开的文件描述符

#### dup(fd)

复制已打开的 fd。

#### pipe(int fds[2])

创建管道。

读的 fd 写在 `fds[0]`，写的 fd 写在 `fds[1]`。

eg. 使用管道读写

```c
#define HJYSIZE 10
char* HJY = "hjyhhh123";

int p[2];

// 创建管道，如果失败则退出
if (pipe(p) < 0) exit(1);

// 写管道
write(p[1], HJY, HJYSIZE);

// 读管道
char buf[HJYSIZE];
read(p[0], buf, HJYSIZE);

printf("%s", buf); // should print HJY
```

注：由于 fork 的子进程和父进程共享相同的 fd 表，故还可以利用 pipe 来通信。

#### read(fd, buff, len)

从 `fd` 中读取 `len bytes` 的数据到 `buff`。

eg. 从标准输入读取 4 个字节

```c
read(0, &osh, 4);
```

#### write(fd, buff, len)

将 `buff` 中的 `len bytes` 数据写到 fd。 

eg. 向标准输出输出 4 个字节

```c
write(1, "osh\n", 4);
```

#### wait(status)

暂停调用 `wait()` 的进程，直到它的一个子进程结束。 

#### waitpid(pid, status, options)

暂停调用 `wait()` 的进程，直到 pid 那个进程状态改变。

pid 有以下取法：

- 小于 -1：等待进程组 ID 等于 -pid 的子进程。
- 等于 -1：等待任何子进程。
- 等于 0：等待进程组 ID 等于调用进程的子进程。
- 大于 0：等待等于 pid 值的子进程。

### 子进程的退出

`fork()` 可以用于创建子进程，子进程退出时应该把退出状态返回给父进程。

无论子进程是退出了还是停止了，父进程都会收到一个 `SIGCHLD` 信号，父进程可以用 `wait()` 或者  `waitpid()`  和一些宏（`WIFEXITED`, `WEXITSTATUS`）来得到子进程返回的情况。

- WIFEXITED(status)

  如果子进程正常终止返回 true。

- WEXITSTATUS(status)

  在子进程正常终止的情况下，返回退出状态。

eg. 获取子进程返回值

```c
pid_t pid = fork();

if (pid == 0) {
    // do something
}

int status;
waitpid(pid, &status, 0);

if (WIFEXITED(status)){
    int exit_status = WEXITSTATUS(status);
    printf("status: %d\n", exit_status);
}
```

## 功能实现

V Shell 实现了以下功能：

- 管道
- 内建命令
    - cd
    - pwd
    - exit
    - export
- 支持输入输出重定向
- 根据运行命令返回值改变提示符的颜色
- 更健壮和模块化的设计

## 思考

### 为什么 `cd` 不能做成外部命令而是内建命令？

「当前工作目录」这个概念是针对不同进程的，Shell 作为一个进程有自己的「当前工作目录」，所以更改这个属性也需要更改自身的状态，如果 `cd`  是一个外部程序则运行退出后不会影响 Shell 进程的状态，不能达到想要的目的。

## 参考资料

https://zh.wikipedia.org/wiki/%E6%96%87%E4%BB%B6%E6%8F%8F%E8%BF%B0%E7%AC%A6

https://www.ibm.com/developerworks/cn/linux/kernel/syscall/part1/appendix.html

https://www.geeksforgeeks.org/exit-status-child-process-linux