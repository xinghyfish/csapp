# 8 Exceptional Control Flow
异常事件流出现在计算机系统的各个层面：
- 低级机制
  - Exception（异常）
    - 系统事件导致控制流变化（例如系统状态的改变）
    - 由硬件和OS共同实现
- 高级机制
  - Process Context Switch（进程上下文切换）
  - Signals（信号）
    - 由OS实现
  - Nonlocal jumps（非本地跳转）
    - `setjmp()`和`longjmp`

# 8.1 Exceptions
## 8.1.1 异常处理机制
- 异常实质上是将低级别的控制权转移到操作系统内核（驻留在内存的部分）
- 异常处理结束后，将会发生三种结果
  1. 控制器将控制权返回给当前指令`I_curr`
  2. 控制器将控制钱返回给下一条指令`I_next`
  3. 控制器舍弃被中断的程序

### 8.2.1 异常表
- 异常表存储各类异常处理器代码的地址，通过异常号`n`查询异常处理代码其实地址，执行相关代码

## 8.2 异常类型
1. **异步异常/中断**：处理器外部发生变化导致
  - 通过在处理器上设置引脚，向处理器通知来实现（中断引脚）
  - 控制钱返回控制权给下一条指令
  - 例如：计时器中断（每隔几毫秒执行一次）、外部设备的I/O
2. **同步异常**
   - **Trap（陷阱）**：程序故意引发的异常
     - 进行**系统调用**，从内核请求各种功能
     - 返回控制给`I_next`
   - **Fault（故障）**：程序无意引发的异常，可能可以恢复
     - 例如虚拟内存中的**页缺失**
     - 返回控制给`I_fault`
   - **Abort（中止）**：程序无意引发且**不可恢复**的异常
     - 中止程序运行

### 8.1.3 Linux/x86-64系统异常
在Linux/x86-64系统中，每一个系统调用都有一个唯一的**编号**，且Linux系统提供了相应的系统调用函数。
- `read()`：读文件
- `fork()`：创建进程

## 8.2 Process（进程）
进程的**定义**就是正在运行的文件。进程提供了两个重要的**抽象**：
1. 进程认为自己独占CPU和寄存器
2. 进程认为自己有自己的地址空间

当异常出现后，操作系统将当前进程的**寄存器**和**地址空间**复制到存储器中并存储起来；之后，操作系统安排下一次要执行的进程，将该进程寄存器保存的值从存储器复制到寄存器，同时切换到该进程的地址空间。这一过程就是**上下文切换**，也就是寄存器和地址空间的切换。

## 8.3 系统调用错误处理
出现错误时，Linux系统级函数通常会返回`-1`并将全局变量`errno`设置为对应错误码。

```c
void unix_error(char *msg) {
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
    exit(0);
}

pid_t Fork() {
    pid_t pid;

    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}
```

## 8.4 进程控制
### 8.4.1 进程状态
进程的三种状态：
1. Running（运行）：正在运行或等待被执行并最终会被内核调度
2. Stopped（停止）：进程被挂起，在进一步通知之前不会被调度
3. Terminated（中止）：进程被永久停止

### 8.4.2 进程的创建和中止
进程被终止的原因如下：
1. 接收到默认行为是中止的信号
2. 从`main`函数返回
3. 调用`exit`函数

进程创建和中止常常使用如下函数：
1. `void exit(int status)`
   - 功能：退出进程
   - 只被调用一次且不返回任何值
2. `int fork()`
   - 功能：创建子进程
   - 被调用一次，但是返回两次
   - 子进程返回值为`0`，父进程返回值为子进程的`PID`
   - 除了`PID`之外，子进程和父进程拥有相同的**虚拟地址空间**和**打开文件描述符**

### 8.4.3 子进程回收
子进程中之后，仍然占用一部分操作系统资源，例如**退出状态**、**各种OS表**等，此时该子进程被称为**僵尸进程**。僵尸进程需要等待父进程回收。父进程回收子进程，获得退出状态信息后，内核删除僵尸进程。

如果父进程没有回收子进程且父进程中止，则系统安排`PID=`的第一个进程`init()`回收子进程，这样的子进程也被称为**孤儿进程**。

如果不及时回收子进程，将会导致内存泄漏（memory leak）。因此，必须使用`wait()`或者`waitpid()`函数回收僵尸进程。

### 8.4.4 加载并运行
使用`execve()`打破当前的程序，但是保留了当前进程，只是执行了另一个程序。

`execve()`调用一次，永不返回（除非错误情况返回`-1`）。

## 8.5 Signals（信号）

启动系统后，第一个线程是`init`线程，系统上的其他进程都是`init`进程的子进程。`init`线程创建时会创建**守护进程**。

用户在命令行中输入的命令实质上是`shell`在子进程中创建并运行的。

对shell的简单实现可以参考源代码`shellex.c`。代码实现时存在一个问题：如果进程在后台运行，则进程运行完成后仅仅停止而没有被父进程回收。如果大量子进程进入这样的状态将会导致内存泄漏。因此，就需要异常控制流来解决这个问题。

内核将在子进程运行完成后通知shell，接下来shell会回收子进程。这个通知的机制就说**信号**。

信号本质上是一小段消息，由软件实现，内核用信号来通知进程系统中发生的事件，通过为目标进程上下文状态进行修改来实现。

### 8.5.1 信号相关术语

- **发送信号量**：信号量发送通常有两个原因：
  1. 内核检测到例如除零错误或者子进程终止这样的系统事件
  2. 进程调用`kill()`函数显式请求内核发送消息给其他进程
- **接收信号量**：目标进程接收到信号量之后，可以选择**忽略**、**中止**或调用用户级的**信号处理器**。

通常而言，已经发送但是尚未被接受不了的信号被称为**待收信号(pending signal)**。在任何时刻，同种类型的待收信号至多只能有一个，其余信号都将被丢弃。

同时进程也可以阻塞特定信号的接收。

注意到，每一种信号至多被接收一次。

内核在每一个进程的上下文中维护着`pending`和`block`位向量，上述有关的动作都通过这两个数据结构进行查询和维护。用户可以通过`sigprocmask()`函数来置位和清零`block`，因此`block`也被称为信号掩码。

### 8.5.2 发送信号：进程组

每一个进程都属于一个进程组。`kill`指令参数中进程号为负数则表示目标是一个进程组，因此，
```shell
kill -9 -15213
```
表示对进程组号为`15213`的所有进程发送`SIGKILL`信号（参数`-9`）。

### 8.5.3 接收信号：信号处理

可以通过`signal()`函数来修改进程接收到信号码`signum`之后的默认行为。函数的API如下：

```c
handler_t *signal(int signum, handler_t* handler);
```

 当进程接收到信号类型为`signum`的信号时，该函数将被调用，其中`handler`是用户级信号处理的地址；当函数调用完成后，将返回被中断的指令继续执行。

 信号处理是程序中执行的一个**并发逻辑流**，可以访问全局变量。同时，信号处理也是可以嵌套的。

 ### 8.5.4 阻塞和非阻塞信号

内核提供了显式的系统调用`sigprocmask(int how, const sigset_t *set, sigset_t *oldset)`函数，允许阻塞或解除一组信号。相关的支持函数如下：
- `sigemptyset()`：创建空集合
- `sigfillset()`：将每种信号量添加到集合
- `sigaddset()`：将信号量添加到集合中
- `sigdelset()`：将信号量从到集合中移除

注意到，该函数中存在一个隐藏变量`blocked`，我们将其存储在非`NULL`值的地址`oldset`。b
使用方法如下：

```c
sigset_t mask, prev_mask;

Sigemptyset(&mask);       /* create empty mask */
Sigaddset(&mask, SIGINT); /* add SIGINT to mask */

/* Block SIGINT and save previous blocked set */
Sigprocmask(SIG_BLOCK, &mask, &prev_mask);

// code region that will not be interrupted by SIGINT

/* Restore previous blocket set, unblocking SIGINT */
Sigprocmask(SIG_SETMASK, &prev_mask, NULL); 
```

### 8.5.5 自定义信号处理

0. 让处理工作尽可能简单（例如设置全局`flag`并返回）
1. 在处理工作中只调用**同步信号安全(async-signal-safe)**的函数
   - 例如`printf()`，`malloc()`，`exit()`等函数都是不安全的
2. 每次进入和退出都要保存并恢复`errno`
3. 通过临时阻塞所有信号来保护共享数据结构的访问 
   - 防止程序崩溃
4. 将信号处理程序和`main`函数之间共享的变量声明为`volatile`
   - 如果不使用关键字`volatile`，则全局变量存放在寄存器，`main`程序无法接收到更新
5. 将全局的`flag`声明为`volatile sig_atomic_t`
   - 原子执行，禁止中断

异步线程安全也是一个很重要的问题。例如，`printf()`必须为终端加锁，即每一时刻只有最多一个`printf()`函数拥有终端的锁。假如现在已经有一个`printf()`获得了终端锁，在信号处理中又调用了另一个`printf()`，这样就产生了阻塞，同时也是**死锁**。

注意到，待处理信号并不是存放在队列中，而是用一个位来表示，因此不能当作**事件**来处理。也就是说，信号不能用于对其他进程中事件的发生进行计数。例如，如果父进程使用计数器对子进程终止这一事件进行计数，子进程终止后向父进程发送一个`SIGCHLD`信号，父进程进程处理，得到的计数结果和实际子进程结束数量不符合。

正确的操作应该是：修改`SIGCHILD`的处理函数，让其每次被唤醒后使用`wait()`函数来回收**尽可能多**的子进程，即使用循环进行计数操作。如下：
```c
void handler2(int sig)
{
    int olderrno = errno;

    while (waitpid(-1, NULL, 0) > 0)
    {
        Sio_puts("Handler reaped child\n");
    }
    if (errno != ECHILD)
        Sio_error("waitpid error");
    Sleep(1);
    errno = olderrno;
}
```

事实上，信号存在一些问题，主要体现在不同的类Unix系统上信号处理存在不同的语义，例如
- 一些早起的系统在捕获信号后，会恢复默认操作，因此每一次都需要挂载
- 一些慢系统调用，例如`read()`，在调用结束前收到信号，内核将会终止调用并返回一个错误`errno == EINTR`
- 一些系统不阻塞和正在处理的信号同类型的信号

### 8.5.6 同步流和竞争

同步流中存在竞争关系，即可能会出现不一致的情况。如果子进程执行过快，父进程滞后，则信号处理中后置操作将会被忽略，而前置操作将会被后执行，这就导致了二者的不一致性。

为了解决这个问题，必须通过信号屏蔽谨慎处理，必须要在前置操作前**屏蔽信号**，等到前置操作完全执行完成后，接触屏蔽，等待信号处理中后置操作的执行。

### 8.5.7 显式等待信号

使用函数`sigsuspend()`代替`pause()`作为原语。
