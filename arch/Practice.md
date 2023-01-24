# 4.1

```text
0x100: 30f30f00000000000000
0x10a: 2031
0x10c:
0x10c: 4013fdffffffffffffff
0x116: 6031
0x118: 700c0100000000000000
```


# 4.2

```Y86-64
A)  irmovq $-4, %rbx
	rmmovq %rsi, 0x800(%rbx)
B)  pushq  %rsi
	call   proc
	halt
proc:
	irmovq $10, %rbx
	ret
C)  mrmovq 7(%rsp), %rbp
	nop
.byte 0xf0
	popq   %rcx
D)  
loop:
	subq   %rcx, %rbx
	je     loop
	halt
E)  xorq   %rsi, %rdx
	.byte  0xa0  # pushq instruction
	.byte  0xf0
```


# 4.3

```Y86-64
sum:
	xorq   %rax
	andq   $rsi, %rsi
	jmp    test
loop:
	mrmovq (%rdi), %r10
	addq   %r10, %rax
	iaddq  8, %rdi
	iaddq  -1, %rsi
test:
	jne    loop
	ret
```


# 4.4

```Y86-64
rsum:
	xorq   %rax, %rax
	andq   %rsi, %rsi
	je     return
	pushq  %rbx
	mrmovq (%rdi), %rbx
	irmovq $-1, %r10
	addq   %r10, %rsi
	irmovq $8, %r10
	addq   %r10, %rdi
	call   rsum
	addq   %rbx, %rax
	popq   %rbx
return:
	ret
```


# 4.5

```Y86-64
absSum: 
		irmovq $8, %r8
		irmovq $1, %r9
		xorq   %rax, %rax
		andq   %rsi, %rsi
		jmp    test
loop:	
		mrmovq (%rdi), %r10
		xorq   %r11, %r11
		subq   %r10, %r11
		jle    pos
		rrmovq %r11, %r10 
pos:	
		addq   %r10, %rax
		addq   %r8, %rdi
		subq   %r9, %rsi
test:	
		jne    loop
		ret
```


# 4.6

```Y86-64
absSum: 
		irmovq $8, %r8
		irmovq $1, %r9
		xorq   %rax. %rax
		andq   %rsi, %rsi
		jmp
loop:	
		mrmovq (%rdi), %r10
		xorq   %r11, %r11
		subq   %r10, %r11
		cmovq  %r11, %r10
		addq   %r10, %rax
		addq   %r8, %rdi
		subq   %r9, %rsi
test:   
		jne loop
```


# 4.7

第一步将栈顶指针保存下来，第二步将栈顶指针的push到栈，第三步将栈顶指针弹出到rsp，最后比较。实验发现，`push %rsp`将保存原`%rsp`的值。


# 4.8

```Y86-64
popq %rsp <==> mrmovq (%rsp), %rsp`
```


# 4.9

```c
bool eq = (a && !b) || (!a && b) 
```


# 4.10

![[XOR.drawio.png]]


# 4.11

执行到第二个选择语句时已经可以确保A不是最小的，即A>B或者A>C。此时只需要确保B<C即可确保B最小，因此可以简化为
```HCL
word Min3 = [
		A <= B && A <= C: A;
		B <= C          : B;
		1               : C;
];
```


# 4.12

```HCL
Word Median3 = [
		B <= A && A <= C: A;
		C <= A && A <= B: A;
		A <= B && B <= C: B;
		C <= B && B <= A: B;
		1               : C;
];
```


# 4.13

|  Stage  |  Generic `irmovq V, rB`  |  Specific `irmovq $128, %rsp`  |
|  ---- |  ----  |  ----  |
| Fetch  | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[0x016] = 3:0`  |
|  | `rA:rB <- M1[PC+1]` | `rA:rB <- M1[0x017] = f:4` |
|  | `valC <- M8[PC + 2]` | `valC <- M8[0x018] = 128` |
|  | `valP <- PC+10` | `valP <- 0x016 + 10 = 0x020` |
| Decode | | |
| Execute | `valE <- 0 + valC` | `valE <- 0 + 128 = 128` |
| Memory | | |
| Write Back | `R[rB] <- valE` | `R[rB] <- valE = 128` |
| PC update | `PC <- valP` | `PC <- valP = 0x020` |


# 4.14

|  Stage  |  Generic `popq rA`  |  Specific `popq %rax`  |
|  -  |  -  |  -  |
| Fetch | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[0x02c] = b:0` |
| | `rA:rB <- M1[PC + 1]` | `rA:rB <- M1[0x02d] = 0:f` |
| | `valP <- PC + 2` | `valP <- 0x02c + 2 = 0x02e` |
| Decode | `valA <- R[%rsp]` | `valA <- R[%rsp] = 120` |
| | `valB <- R[%rsp]` | `valB <- R[%rsp] = 120` |
| Execute | `valE <- valB + 8` | `valE <- 120 + 8 = 128` |
| Memory | `valM <- M8[valA]` | `valM <- M8[120] = 9` |
| Write back | `R[%rsp] <- valE` | `R[%rsp] <- 128` |
| | `R[rA] <- valM` | `R[%rax] <- 9` |
| PC update | `PC <- valP` | `PC <- 0x02e` |


# 4.15

将原来的栈顶指针寄存器 `%rsp` 的值压到栈顶。


# 4.16

write-back阶段的两个复制操作都会修改 `%rsp` 寄存器存储的值。而根据Y86-64指令定义的顺序， `valM` 是有效值，因此这一操作等价于什么都没操作。


# 4.17

| Stage | `cmovXX rA, rB` |
| - | - |
| Fetch | `icode:ifun <- M1[PC]` |
| | `rA:rB <- M1[PC + 1]` |
| | `valP <- PC + 2` |
| Decode | `valA <- R[rA]` |
| Execute | `valE <- 0 + valA` |
| | `Cnd <- Cond(CC, ifun)` |
| Memory | |
| Write back | `if (Cnd) R[rB] <- valE` |
| PC update | `PC <- valP` |


# 4.18

|  Stage  |  Generic `call Dest` |  Specific `call 0x041`  |
|  -  |  -  |  -  |
| Fetch | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[0x037] = 8:0` |
| | `valC <- M8[PC + 1]` | `valC <- M8[0x038] = 0x041` |
| | `valP <- PC + 9` | `valP <- 0x040` |
| Decode | | |
| | `valB <- R[%rsp]` | `valB <- R[%rsp] = 128` |
| Execute | `valE <- valB + (-8)` | `valE <- 128 + (-8) = 120` |
| Memory | `M8[valE] <- valP` | `M8[120] <- 0x040` |
| Write back | `R[%rsp] <- valE` | `R[%rsp] <- 120` |
| PC update | `PC <- valC` | `PC <- 0x041` |


# 4.19

```HCL
bool need_valC = 
		icode in { IIRMOVQ, IMRMOVQ, IRMMOVQ, IJXX, ICALL };
```


# 4.20

```HCL
word srcB = [
		icode in { IRMMOVQ, IMRMOVQ, IOPQ } : rB;
		icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP
		1: RNOME;
];
```


# 4.21

```HCL
word dstM = [
		icode in { IMRMOVQ, IPOPQ } : rA
];
```

# 4.22

写入 `dstM` 的具有更高的优先级。


# 4.23

```HCL
word aluB = [
		icode in { IOPQ, IMRMOVQ, IPUSHQ, IPOPQ, 
					ICALL, IRET } : valB;
		icode in { IRRMOVQ, IRMMOVQ } : 0;
		# Other instructions don't need ALU
];
```


# 4.24

```HCL
word dstE = [
		icode in { IRRMOVQ } && Cnd : rB;
		icode in { IIRMOVQ, IOPQ } : rB;
		icode in { ICALL, IRET, IPUSHQ, IPOPQ } : RRSP,
		1 : RNOME;
];
```


# 4.25

```HCL
word mem_data = [
		icode in { IRMMOVQ, IPUSHQ } : valA;
		icode == ICALL : valP;
];
```


# 4.26

```HCL
bool mem_write = icode in { IRMMOVQ, IPUSH, ICALL };
```


# 4.27

```HCL
word stat = {
		imen_error || dmem_error : SADR;
		!instr_valid : SINS;
		icode == IHALT : SHLT;
		1 : SAOK;
}
```

# 4.28

A. 考虑到短板效应，应该将寄存器放在C和D之间，
- throughput = 1000/190 GIPS = 5.26 GIPS
- latency = 190 ps * 2 = 380 ps

B. 考虑到短板效应，应该将寄存器放在B和C、D和E之间，
- throughput = 1000/130 GIPS = 7.69 GIPS
- latency = 130 ps * 3 = 390 ps

C. 考虑到短板效应，应该将寄存器放在A和B、C和D、D和E之间，
- throughput = 1000/110 GIPS = 9.09 GIPS
- latency = 110 ps * 4 = 440 ps

D. 设计为五阶段寄存器，仅E和F之间不设置寄存器，
- throughput = 1000/100 GIPS = 10.00 GIPS
- latency = 100 ps * 5 = 500 ps


# 4.29

A.
$$
throughout = \frac{1,000}{300/k + 20}=\frac{1,000k}{300+20k}
$$

$$
latency = (300/k + 20) \cdot k = 300+20\cdot k
$$

B.
$$
ultimate\ throughout = \lim_{k \to \inf} = \frac{1,000}{200}=50 GIPS
$$


# 4.30

```HCL
word f_stat = [
		imem_error : SADDR;
		!instr_valid : SINS;
		f_icode == IHALT : SHLT;
		1 : SAOK;
];
```


# 4.31

```HCL
word d_dstE = [
		D_icode in { IOPQ, IRRMOVQ, IRRMOVQ } : D_rB;
		D_icode in { IPUSHQ, IPOPQ, ICALL, IRET } : RRSP;
		1 : RNOME;
]
```


# 4.32

如果这么操作，那么line 5的结果将会是 `M8[0x100] = 0x108`，和我们期望的 `M8[0x100] = 0x3` 不符。


# 4.33

摆烂做法：

```Y86-64
irmovq $5, %rdx
irmovq $0x100, %rsp
rmmovq %rdx, 0(%rsp)
popq %rsp
nop
nop
rrmovq %rsp, %rax
```


# 4.34

```HCL
word d_valB = [
		d_srcB == e_dstE : e_valE;    # Forward valE from execute stage
		d_srcB == M_dstM : m_valM;    # Forward valM from memory stage
		d_srcB == M_dstE : M_valE;    # Forward valE from memory stage
		d_srcB == W_dstM : W_valM;    # Forward valM from write-back stage
		d_srcB == W_dstE : W_valE;    # Forward valE from write-back stage
		1 : d_rvalB;  # Use value read from register file
];
```


# 4.35

```Y86-64
irmovq $0x1, %rax
irmovq $0x2, %rdx
xor    %rcx, %rcx
cmovne %rax, %rdx
addq   %rdx, %rdx
```

这个例子也说明`cmovXX` 指令是一个例外。如果改为 `E_dstE`，则在 `cmovne %rax, %rdx` 进入 Execute 阶段发现不满足条件，将会设置 `word e_dstE = RNOME`，但是 `E_dstE = RRAX` 不变。因此后面紧跟的 `addq %rdx, %rdx` 则匹配到 `E_dstE = %rdx` ，则将会把 `e_valE` 也就是 `R[%rax] = 0x1` 作为结果放入 aluA导致计算结果错误。 


# 4.36

```HCL
word m_stat = [
		d_mem_error : SADR;
		1 : M_stat;
];
```


# 4.37

```Y86-64
main:   irmovq Stack, %rsp
		irmovq rtnp, %rax
		pushq  %rax
		xorq   %rax, %rax
		jne proc
		irmovq $1, %rax
		halt
proc:   ret
		irmovq $2, %rbx
		halt
rtnp:   irmovq $3, %rdx
		halt
.pox 0x40
Stack:
```

刚开始并没有理解清楚题意，所以程序写的不是很清楚，这里主要指的是当两种情况同时出现的时候优先级的选取。默认考虑 `jXX` 优先级更高就没有想到潜在的问题。


# 4.38

```Y86-64
		irmovq mem, %rbx
		mrmovq 0(%rbx), %rsp
		ret
		halt
rtnpt:	irmovq %$5%, %rsi
		halt
.pos 0x40
mem:    .quad stack
.pos 0x50
stack:  .quad rtnpt
```


# 4.39

```HCL
bool D_stall = [
		# Conditions for a load/use hazard
		E_icode in { IMRMOVQ, IPOPQ } &&
		 E_dstM in { d_srcA, d_srcB };
];
```


# 4.40

```HCL
bool E_bubble = [
		# Conditions for a load/use hazard
		E_icode in { IMRMOVQ, IPOPQ } &&
		 E_dstM in { d_srcA, d_srcB } ||
		# Conditions for mispredicted branches
		(E_icode in { IJXX } && !e_Cnd);
];
```


# 4.41

```HCL
bool set_cc = [
		# normal stat
		!m_stat in { SADR, SINS, SHLT } && !W_stat in { SADR, SINS, SHLT };
];
```


# 4.42

```HCL
bool M_bubble = [
		# exception occurs
		m_stat in { SADR, SINS, SHLT } || W_stat in { SADR, SINS, SHLT };
];

bool W_stall = [
		# exception occurs
		W_stat in { SADR, SINS, SHLT };
];
```


# 4.43

$$
mp = 0.20\times 0.35\times2 = 0.14
$$
$$
CPI = 1 + 0.05 + 0.14+0.06=1.25
$$


