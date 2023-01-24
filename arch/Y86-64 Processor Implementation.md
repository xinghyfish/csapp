
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

### 2.2 Control Hazard

地址预测的中提到，涉及跳转的部分主要为 `ret` 和 `jXX` 指令。

`ret` 指令的处理比较简单，当该指令进入Decode, Execute, Memory 和 Write back 阶段时自动在pipeline中插入 bubble，类似于stalling方法。这样，就能从返回的位置重新执行指令了。

`jXX` 指令则主要需要处理地址预测错误的情形。当地址预测错误时，`jXX` 处于 Execute 阶段，幸运的是后面两条指令都只处于 Decode 和 Fetch 阶段，从编程者可见视图并没有发生改变（这里响应跟书的标题使用了"in the programmer-visible state"），只有到达 Execute 阶段才会导致 condition code 改变。在这种情况下，通过插入 bubble，让误判取得的指令被后面的指令覆盖即可，并不会对编程者可见视图产生影响，唯一的缺点就是浪费了2个时钟周期。

## 3 异常处理

Y86-64指令集架构主要考虑三种异常：(1) `halt` 指令；(2) 错误的 `icode` 和 `ifun` 组合；(3) 尝试获取非法地址。

在流水线处理器中，异常处理包括了几个子类：
1. 多条指令可能同时触发异常，选择优先级高的异常告知操作系统。
2. 条件跳转指令误取的指令可能触发不应该有的异常，之后被取消。
3. 流水线处理器在不同的阶段更新系统状态的不同部分。引发异常的指令后面的指令可能会修改系统状态。

Y86-64处理器采用了简单有效的架构来解决这些问题：将 `Stat` 嵌入到流水线寄存器的字段中，因此错误状态能过顺着流水线的方向传递，这样就解决了优先级和状态修改问题；同样，通过 `W_stat` 和 `m_stat` 信号控制更新 CC 也可以确保异常指令后进入 Execute 阶段的指令不会修改 CC 的值。

## 4 PIPE阶段实现

### 4.1 PC选择和Fetch阶段

Select PC控制逻辑接收输入后发出 `f_pc` 信号指示真正的PC值后，去内存中取得对应的指令：

```HCL
word f_pc = [
		# Mispredicted branch.  Fetch at incremented PC
		M_icode == IJXX && !M_Cnd : M_valA;
		# Completion of RET instruction
		W_icode == IRET : W_valA;
		# Default: Use predicted value of PD
		1 : F_predPC;
];
```

当指令为 `call` 或者 `jump` 时PC预测逻辑则选择 `valC` 作为跳转目标地址：

```HCL
word f_predPC = [
		f_icode in { IJXX, ICALL } : f_valC;
		1 : f_valP;
];
```

### 4.2 Decode和Write back阶段

PIPE方案中将 `valA` 和 `valP` 合并，因为需要 `valP` 的都不需要从A口读数据，因此，

```HCL
word d_valA = [
		D_icode in { ICALL, IJXX } : D_valP; # Use incremented PC
		d_srcA == e_dstE : e_valE;    # Forward valE from execute stage
		d_srcA == M_dstM : m_valM;    # Forward valM from memory stage
		d_srcA == M_dstE : M_valE;    # Forward valE from memory stage
		d_srcA == W_dstM : W_valM;    # Forward valM from write-back stage
		d_srcA == W_dstE : W_valE;    # Forward valE from write-back stage
		1 : d_rvalA;  # Use value read from register file
];
```

上面的顺序编排是很有讲究的，任何企图调整顺序的行为~~终将绳之以法~~都会导致逻辑错误。顺序编排的原则是：
1. 就近原则：优先选择最新的值，从execute到write-back
2. 同一个stage的顺序编排是考虑了 `popq %rsp` 指令（只有该指令能同时使用2个相同的寄存器）

状态码寄存器则存储Write-back阶段的状态，如果是bubble则设置为`AOK`：

```HCL
word Stat = [
		W_stat == SBUB : SAOK;
		1 : W_stat;
];
```

### 4.3 Execute阶段

改阶段通过 `W_Stat` 值和 `m_stat` 信号来控制 Set CC ，从而确保异常指令后进入 Execute 阶段的指令不会修改 CC 的值。

做完Practice Problem 4.35（参考[[Practice#4.35]]）时对 Execute 阶段的 `dstE` 寄存器的 HCL 有了更多的思考，推测应该是：

```HCL
word dstE = [
		icode in { IJXX, IRRMOVQ } && !e_Cnd : RNOME;
		1 : E_dstE;
];
```

这样就可以确保流水线中预测失误的语句对 Decode 阶段不造成影响。

### 4.4 Memory阶段

这部分除了部分信号量重命名，其他和SEQ几乎一致。

## 5 Pipeline控制逻辑

控制逻辑部分主要需要补充下面几个问题的控制逻辑：
- *Load/Use Hazards* : pipeline要在从内存中读数据的指令和使用该值的指令间 stall 一个时钟周期
- *Processing* `ret` : pipeline需要在 `ret` 语句到达 write-back ji阶段前一直 stall 
- *Mispredicted branches* : 当 `jXX` 没有跳转时，后面的指令需要被取消，而且 `jXX` 指令的下一条指令将进入 Fetch 阶段
- *Exceptions* : 触发了异常的指令后面的指令不能修改编程者可见状态。

下面将针对上面几种情况设计对应的控制逻辑来解决这些问题。

### 5.1 解决方案

#### 5.1.1 解决 Load/Use Hazards

能够触发这一问题的指令只有 `mrmovq` 和 `popq` 指令。当满足下列条件：
1. 任意一条上述指令在 Execute 阶段
2. Decode 阶段的指令需要上述指令的目的寄存器
满足上述条件时，pipeline控制逻辑保持 pipeline 寄存器 `F` 和 `D` 固定，在 Execute 阶段插入一个 bubble。

#### 5.1.2 解决 `ret` 跳转

由于不视图进行预测返回地址，因此在 HCL 实现的 PC 预测逻辑将顺序的下一条指令作为下一条 Fetch 的地址。进行 Decode 后，由于下一条指令已经进入 Fetch  阶段，因此下一条指令将被 bubble 替代进入 Decode 阶段；`ret` 指令后的指令都将保持在 Fetch 阶段，直到 `ret` 指令执行到 Write-back 阶段才获得跳转后的指令地址。

#### 5.1.3 解决 Mispredicted branches

在上文中已经提到，如果预测失败，则当 jump 指令执行到 Execute 阶段时发现这个问题，控制逻辑单元将会在 Decode 和下一个时钟周期的 Execute 阶段插入 bubble ，这样就可以取消后面的指令。在插入两个 bubble 后（也就是 jump 指令执行到 Memory 阶段的周期）pipeline 读取正确的指令到 Fetch 阶段。

#### 5.1.4 解决 Exceptions

实现 Exception 处理需要基于以下事实：
1. 异常在程序执行的 Fetch 阶段（非法指令异常）和 Memory 阶段（地址异常）被检测到；
2. 程序状态在 Execute 、 Memory 和 Write-back 阶段被修改。

因此，当指令执行到 Memory 阶段时检测到问题，我们需要避免后面的指令修改编程者可见状态，需要采取如下步骤：
1. Execute 阶段 `set_cc` 信号失效：检测 `m_stat` 和 `W_stat` 信号，如果有异常则设置 `set_cc = 0`
2. 在 Memory 阶段注入 bubble 并使任何写入数据内存的行为失效
3. Write-back 阶段出现异常指令则进行 stall

通过这样的设置，就可以做到：异常指令前的指令都可以完成；后面的指令不会修改编程者可见状态。

### 5.2 控制条件组合

具体的图标参考教材（包含前文），这里主要概括控制条件组合情况下的一些逻辑判断。

当上面的几种控制逻辑涉及的情况同时出现时，需要考虑Combination，主要有：
1. Mispredict(`jXX` in Execute) + `ret`(in Decode)。这种情况下，优先执行stall或者bubble，因此 `ret` 指令不被执行。
2. Load/Use(Load in Execute + Use in Decode) + `ret`(in Decode) 。这种情况下，Decode 阶段我们不期望出现 bubble 把 `ret` 指令清空。因此合并之后改成 Stall 。

