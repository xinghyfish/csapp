
# Sequential Y86-64 Implementation

前文主要是一些指令、单元结构的介绍，需要注意的是，寄存器的write、内存的write操作都是需要等到时钟信号出现上升时才进行的。

介绍SEQ的目的是为了最终实现高效的流水线处理器做准备的。

## 1 指令处理的阶段

处理一条指令需要经过如下的线性阶段：

1. **取指令** (*Fetch*)：通过**程序计数器**（PC）指向的内存地址处取得指令，并
	- 解析前两个字节 `icode` （指令码）和 `ifun`（指令功能）；
	- 获取寄存器标志符 `rA` 和 `rB`
	- 获取常数 `valC`
	- 计算线性顺序上下一条指令的地址`valP`（PC + 本指令长度）
2. **解码** (*Decode*)：读取至多两个寄存器值 `valA` 和 `valB`，通常通过 `rA` 和 `rB` 字段来获取，部分指令也可以通过 `%rsp` 来获取。
3. **执行** (*Execute*)：ALU根据 `ifun` 执行指令，
	- 计算内存引用的有效地址
	- 栈指针的增减
	- 将计算结果存储为 `valE`
	- 设置条件码（condition code）
4. **读写内存** (*Memory*)：将从内存中读到的值记为 `valM`
5. **写回** (*Write back*)：将至多两个结果写回寄存器文件
6. **PC更新** (*PC update*)：将PC设置为下一条指令的地址

|  Stage   | `OPq rA, rB`  | `rrmovq rA, rB` | `irmovq V, rB` |
|  ----  | ----  | ---- | ---- |
| Fetch  | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[PC]` |
|  | `rA:rB <- M1[PC + 1]` | `rA:rB <- M1[PC + 1]` | `rA:rB <- M1[PC + 1]` |
|  | | | `valC <- M8[PC + 2]` |
|  | `valP <- PC + 2` | `valP <- PC + 2` | `valP <- PC + 10` |
| Decode | `valA <- R[rA]` | `valA <- R[rA]` | |
|  | `valB <- R[rB]` | | |
| Execute | `valE <- valA OP valB` | `valE <- 0 + valA` | `valE <- 0 + valC` |
|  | Set CC | | |
| Memory | | | |
| Write back | `R[rB] <- valE` | `R[rB] <- valE` | `R[rB] <- valE` |
| PC update | `PC <- valP` | `PC <- valP` | `PC <- valP` |

需要进行内存读写的指令执行过程如下：

| Stage | `rmmovq rA, D(rB)` | `mrmovq D(rB), rA` |
| - | - | - |
| Fetch | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[PC]` |
| | `rA:rB <- M1[PC + 1]` | `rA:rB <- M1[PC + 1]` |
| | `valC <- M8[PC + 2]` | `valC <- M8[PC + 2]` |
| | `valP <- PC + 10` | `valP <- PC + 10` |
| Decode | `valA <- R[rA]` | |
| | `valB <- R[rB]` | `valB <- R[rB]` |
| Execute | `valE <- valB + valC` | `valE <- valB + valC` |
| Memory | `M8[valE] <- valA` | `valM <- M8[valE]` |
| Write back | | `R[rA] <- valM` |
| PC update | `PC <- valP` | `PC <- valP` |

栈操作指令的执行过程如下：

|  Stage  |  `pushq rA`  |  `popq rA`  |
|  -  |  -  |  -  |
| Fetch | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[PC]` |
| | `rA:rB <- M1[PC + 1]` | `rA:rB <- M1[PC + 1]` |
| | `valP <- PC + 2` | `valP <- PC + 2` |
| Decode | `valA <- R[rA]` | `valA <- R[%rsp]` |
| | `valB <- R[%rsp]` | `valB <- R[%rsp]` |
| Execute | `valE <- valB + (-8)` | `valE <- valB + 8` |
| Memory | `M1[valE] <- valA` | `valM <- M8[valA]` |
| Write back | `R[%rsp] <- valE` | `R[%rsp] <- valE` |
| | | `R[rA] <- valM` |
| PC update | `PC <- PC + 2` | `PC <- PC + 2` |

跳转指令如下：

|  Stage  |  `jXX Dest`  |  `call Dest`  |  `ret`  |
|  -  |  -  |  -  |  -  |
|  Fetch  | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[PC]` | `icode:ifun <- M1[PC]` |
| | `valC <- M8[PC + 1]` | `valC <- M8[PC + 1]` | |
| | `valP <- PC + 9` | `valP <- PC + 9` | `valP <- PC + 1` |
| Decode | | | `valA <- R[%rsp]` |
| | | `valB <- R[%rsp]` | `valB <- R[%rsp]` | 
| Execute | | `valE <- valB + (-8)` | `valE <- valB + 8` |
| | `Cnd = Cond(CC, ifun)` | | | 
| Memory | | `M8[valE] <- valP` | `valM <- M8[valA]` |
| Write back | | `R[%rsp] <- valE` | `R[%rsp] <- valE` |
| PC update | `PC <- Cnd ? valC : valP` | `PC <- valC` | `PC <- valM` |


## 2 硬件结构

注意，SEQ的硬件结构由**组合逻辑** (*combinational logic*) 和两种存储设备**时钟寄存器** (*clocked register*) 和 **随机访问存储器** (*random access memories*)组成：
- 时钟寄存器：program counter(PC) + condition code register
- 随机访问存储器：register file + instruction memory + instruction memory + data memory

>重要原则：不进行**回读** (*read back*)
>为了完成指令，处理器不必回读指令更新过的状态。

## 3 SEQ时序

时钟信号由低到高，处理器才开始执行一条新的指令。
组合逻辑不需要任何控制，当输出改变时，数值就沿着数字电路进行传递。
时钟寄存器则只有在时钟信号由低电平升到高电平时才会把新的值写到寄存器内。
随机访问存储器在逻辑上假设和组合逻辑一致。

## 4 SEQ阶段实现

### 4.1 Fetch

`icode`的HCL如下：
```HCL
bool need_regids = 
		icode in { IRRMOVQ, IOPQ, IPUSHQ, IPOPQ,
				   IIRMOVQ, IRMMOVQ, IMRMOVQ };
```

### 4.2 Decode and Write-Back Stages

`src`的HCL如下：
```HCL
word srcA = [
		icode in { IRRMOVQ, IRMMOVQ, IOPQ, IPUSHQ } : rA;
		icode in { IPOPQ, IRET } : RRSP;
		1: RNOME; # DOn't need register
];
```

### 4.3 Execute
```HCL
word aluA = [
		icode in { IRRMOVQ, IOPQ } : valA;
		icode in { IRMMOVQ, IMRMOVQ, IIRMOVQ } : valC;
		icode in { ICALL, IPUSHQ } : -8;
		icode in { IRET, IPOPQ } : 8;
		# Other instructions don't need ALU
];
```

```HCL
word alufun = [
		icode == IOPQ : ifun;
		1 : ALUADD;
];
```

```HCL
bool set_cc = icode in { IOPQ };
```

### 4.4 Memory

```HCL
word mem_addr = [
		icode in { IRMMOVQ, IMRMOVQ, ICALL, IPUSHQ } : valE;
		icode in { IRET, IPOPQ } : valA;
		# Other instructions don't need address
];
```

### 4.5 PC Update

```HCL
word new_pc = [
		icode == ICALL : valC;
		icode == IJXX && Cnd : valC;
		icode == IRET : valM;
		1 : valP;
];
```

---

# Pipelined Y86-64 Implementation

和Sequential化的处理器相比，Pipelined（流水线）化的处理器的吞吐量更高，因此效率更高。对SEQ版本进行改进后，我们得到了PIPE-版本。

在这一版本中，每一个阶段都添加了一个寄存存储下一个时钟周期进入对应状态的数据，例如`M_valM` 表示即将进入内存访问阶段时指令的 `valM` 的值；而控制逻辑单元产生的信号量则用小写字母前缀的符号来表达，例如 `e_valE` 表示执行阶段产生的 `valE` 的信号值。

尽管Pipeline更加高效，但是会带来一些问题，例如 `PC` 的值和条件跳转指令 `jXX`、`ret` 指令。在这一节中将会进行探究。

## 1 地址预测

关于条件语句跳转时，下一条指令的地址预测的功能被称为**分支预测** (*branch prediction*)。本书采用简单的方法，将条件跳转/分支跳转 `jXX` 的指令的预测下一条指令地址设为 `valC`。

对于 `ret` 语句不采用分支预测，直到 `ret` 语句完成 write-back 环节。

## 2 Pipeline Hazard

这个词汇的中文翻译比较奇怪，Pipeline指的是流水线；Hazard指的是危害、冒险，但我觉得这里更多指的是流水线中出现的问题，两个词合起来翻译很奇怪，因此这里保留原文。Pipeline Hazard主要由**数据依赖**和**控制依赖**引起。

### 2.1 Data Hazard和解决方案

Data Hazard的原因在教材上呈现地非常详细，这里简单概括为：新的数据没有及时更新到寄存器中，然而同一时刻后面的指令需要读取寄存器，这样的一个不同步将会导致出现数据不一致的问题。

解决方案通常有：1. 延后(Stalling)；2. 传递(Forward)。

#### 2.1.1 Stalling

Stalling方案中，处理器将若干条指令拦在流水线中，直到Hazard的条件不再成立。这个方案等价于自动插入`nop`，形象化为Bubble。当一条指令在Decode过程中检测到读取的寄存器至少被execute/memory/write back阶段中一条指令修改时，将会保持在Decode阶段（只需要保持PC的值不变即可），直到所有的寄存器更新完成。

这一方案虽然比较简单，但是由于会有大量的数据依赖的情形，这样会导致系统的总体吞吐量显著降低，因此Forward方案就应运而生。

#### 2.1.2 Forwarding

和Stalling这样摆烂的方案不同的是，Forwarding方案则是选择勇敢面对问题。当出现Hazard时，Forward方案中采取用流水线寄存器(pipeline registers)，也就是PIPE-架构中新加入的寄存器，以及一些中间临时产生的信号量值，也就是在正式update之前的寄存器值或者信号量“传递”到Decode阶段指令解码得到的 `valA` 或 `valB`。

因此，总共有5种不同的Forwarding源：`e_valE`，`m_valM`，`M_valE`，`W_valM`，`W_valE`，和2中不同的Forward目的地：`valA` 和 `valB`。由于书上并没有说明5种Forwarding源对应的情况，经过不是那么缜密的思考，我总结出几种对应的情形：

1. `e_valE`：指令的decode阶段需要使用前一条指令更新的寄存器，例如教材给的例子。这种情况下当然是越快越好，因此和Decode距离最近的Execute就要发挥作用，在时钟信号重新回升之前更新 `valA` 或 `valB` 即可。
```Y86-64
# prog 4
0x000: irmovq $10, %rdx
0x00a: irmovq  $3, %rax
0x014: addq %rdx, %rax
0x016: halt
```
2. `m_valM`：将内存中读出的数据在更新给寄存器之前先传给临时寄存器。（PS：在构想这个例子的时候没有刚开始没有添加 `nop` 指令，导致出现Forwarding也无法解决的Data Hazard，貌似只能用Stalling用类似于加读锁的思想来解决，后面发现教材提供了这个case）
```Y86-64
# prog 5-
0x000: popq %rax
0x002: nop
0x003: rrmovq %rax, %rdx
```
3. `M_valE`：将还没有进入Memory阶段的指令得到的 `valE` 传递到后面两条指令的 `valA` 或者 `valB`，参考 prog 4。
4. `W_valM`：将还没来得及写回寄存器的从内存中读取的数据传递到后面两条指令的 `valA` 或者 `valB`，例如在 prog 5- 的 `popq` 指令后加塞一条 `nop` 指令。
5. `W_valE`：将还没来得及写回寄存器的计算值传输到后面三条指令的 `valA` 或者 `valB`，例子略。

逻辑部件通过对比要写回的寄存器IDs和源寄存器IDs `srcA` 和 `srcB` 之间的关系来检测Forwarding的情形。因此存在多个要写会的寄存器IDs和一个源IDs相匹配的情况，需要建立优先级序列来解决这个问题。

#### 2.1.3 Load/Use Data Hazards

上文在思考第2种情形时出现的无法解决的问题在这里提到了（严谨！），本质上是由于Memory阶段过于靠后，如果存在需要读内存的指令后面紧跟着需要读该指令目的寄存器的指令，例如
```Y86-64
# prog5
0x000: irmovq $128, %rdx
0x00a: irmovq   $3, %rcx
0x014: rmmovq %rcx, 0(%rdx)
0x01e: irmovq $10x %rbx
0x028: mrmovq 0(%rdx), %rax
0x032: addq %rbx, %rax
0x034: halt
```
在上面的程序中，执行到cycle7时需要decode指令6，但是指令5仍在Execute阶段，这就导致内存中的数据无法及时更新到目标寄存器，导致读取错误。

解决这种情形需要Forwarding和Stalling的组合使用。当decode检测到上文存在需要从内存中读取数据加载到的寄存器时，将会自动插入进行Stalling，之后在上文指令的Memory Stage产生`m_valM` 后立刻进行 Forward，回到2.1.2中的第2种情形。

难怪Figure 4.52 的说明中提到"handle most forms of data hazards"，也是对应着一小节的内容。这样的方案也被称为load interlock（暂且可以翻译为加载互锁，等于是加了一把对内存的读锁，直到加载完成发出对应的信号后才释放锁）。

论语有云“学而不思则罔，思而不学则怠”，在思考5种Forward源时我突然想到了Load/Use Data Hazard，并提出了和教材上一致的解决方案，说明在学习过程中进行适时的思考也是非常重要的。也侧面反映出我的智力水平还尚可😎。

