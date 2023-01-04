
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

#
