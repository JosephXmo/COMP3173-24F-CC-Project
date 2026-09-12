# uslc UIC Set Language Compiler

本文档是 `StandardImplementation` 的唯一工程规范。后续设计、实现、测试和重构均以本文档为准；原始课程文档仅作为需求来源和历史参考。若两者存在冲突，以本文档中的“规范裁决”与明确规则为准。

`uslc` 是 UIC Set Language Compiler 的命令名和项目名。项目实现一个小型集合代数语言的编译器前端与解释器。实现过程强调编译原理中的直接方法：词法分析、文法表示、FIRST/FOLLOW 集、LR(0) 项目集族、SLR(1) 分析表、移进归约、语法树、静态类型检查和求值。工程使用 ISO C23 与 C standard library，不依赖第三方库、包管理器、代码生成器或外部运行时。

## 1. 工程目标

### 1.1 必须实现

1. 从文件或标准输入读取 Set Algebra 源程序。
2. 完成词法分析，生成带源位置的 token 流。
3. 根据本文档中的文法计算 FIRST/FOLLOW、构造 LR(0) 项目集族并生成 SLR(1) ACTION/GOTO 表。
4. 使用表驱动的移进归约算法完成语法分析，并构造完整 parse tree。
5. 将 parse tree 降低为更适合语义处理的 AST。
6. 完成符号表管理、作用域处理和静态类型检查。
7. 对正确程序进行求值，支持整数、布尔值和符号集合。
8. 输出稳定、结构化、可读的诊断、执行结果和可选 JSON artifacts。
9. 在 Linux 和 Windows 上通过简单 shell scripts 构建和测试。
10. 为所有核心模块提供不依赖第三方测试框架的自动化测试。

### 1.2 设计取向

- 朴素且理论化：核心算法直接对应教材定义，不隐藏在生成器或框架之后。
- 可解释：SLR 状态、ACTION/GOTO 表、parse tree 和 AST 均可按需打印。
- 可移植：只依赖 C23 和宿主编译器，不在产品代码中使用 POSIX、Win32 或编译器专有 API。
- 确定性：同一输入产生字节级稳定的文本和 JSON 输出。
- 内存安全：所有长度计算、整数运算和动态分配都检查溢出与失败。
- 清晰优先：模块边界、所有权和错误路径必须显式，不追求晦涩的微优化。

### 1.3 非目标

- 不实现 GUI、网络服务、IDE 插件或交互式 REPL。
- 不枚举无限集合；集合始终以特征谓词表示。
- 不实现错误恢复。遇到当前阶段的首个错误后停止后续阶段。
- 不实现字符串、浮点数、数组、集合的集合或用户自定义函数。
- 不要求兼容 AUTOLAB 的逐字输出、旧 `make.bat` 或原始启动代码 ABI。
- 不使用 CMake、Make、Ninja、Meson 或第三方测试/JSON/容器库。

## 2. 规范来源与裁决

需求整理自以下课程材料：

- `../COMP3173 24F Project Description.docx`
- `../Type Checker.docx`
- `../SLR Grammar.txt`
- `../SLR Parsing Table.csv`
- `../Startup Code/`

课程材料中存在若干不一致。本实现作出以下最终裁决：

1. `=` 是正式 token。虽然原项目 Phase 1 的 token 摘要漏列了它，文法、分析表和示例均使用它。
2. 原文中被修订的 `syntax_out.json` 统一为 `parser_out.json`。本项目默认不生成旧 AUTOLAB 文件，但兼容 artifact 模式使用新的清晰文件名。
3. 声明必须满足声明类型与初始化表达式类型一致。`let set x be 4 .` 是类型错误。
4. 未声明标识符是类型错误，不把它隐式创建成“未初始化变量”。集合定义中的 representative 是唯一允许的局部绑定变量。
5. 同一作用域中不能重复声明；声明的名字在初始化表达式完成检查和求值之后才进入符号表，因此不允许直接自引用。
6. 空格、制表符、CR 和 LF 都是空白。空白分隔 token，但不是语言中唯一的分隔方式。
7. 核心版本不接受源代码中的 `true` 和 `false` 字面量；它们可以出现在求值与集合化简输出中。
8. 核心版本严格使用第 5 节的 34 条产生式。Bonus 的 `simplify` 是后续扩展，不改变核心版本的验收标准。
9. 原 Bonus 文档中的区间合并例子存在逻辑上不等价的结果。本实现按标准整数集合与区间代数处理，不复制该错误。
10. 类型检查文档在规则 24 和 26 的伪代码中多写了悬空的 `and`。其含义按“当且仅当子表达式为 relation 时产生 predicate”解释。

## 3. 用户可见语言

### 3.1 程序结构

每个源程序由以下部分组成：

1. 零个或多个变量声明；
2. 恰好一个计算语句；
3. 计算语句之后只能出现空白和 EOF。

变量声明：

```text
let T id be E .
```

其中 `T` 是 `int` 或 `set`，`id` 是标识符，`E` 是与声明类型相同的表达式。

计算语句：

```text
show A .
```

`A` 可以产生整数、集合或布尔值。

示例：

```text
let int lower be 3 .
let set positive be { x : x > lower } .
show 5 @ positive .
```

### 3.2 字符与源文件

- 源文件按 UTF-8 读取，但核心语言的有效 token 全部位于 ASCII。
- 非 ASCII 字节序列不属于合法标识符，报告 lexical error。
- 接受空格、水平制表符、`\r`、`\n`、`\f` 和 `\v` 作为空白。
- 核心版本不支持注释。若以后增加注释，必须更新本文档、lexer tests 和语言版本号。
- 行号和列号从 1 开始；byte offset 从 0 开始。

源文件通过 `rb` 模式读取，保留原始 CRLF 与 byte offsets。stdin 使用宿主提供的标准输入流；Windows C runtime 可能先把 CRLF 转成 LF，因此 stdin 的 offset 指向实际读入的 source buffer。需要保留磁盘原始字节位置时使用源文件参数。

### 3.3 Token 规范

| 类别 | Token | Lexeme |
| --- | --- | --- |
| 关键字 | `LET` | `let` |
| 关键字 | `BE` | `be` |
| 关键字 | `INT` | `int` |
| 关键字 | `SET` | `set` |
| 关键字 | `SHOW` | `show` |
| 标识符 | `ID` | `[a-z]+` 且不是关键字 |
| 整数 | `NUM` | `0` 或 `[1-9][0-9]*` |
| 算术 | `PLUS MINUS STAR` | `+ - *` |
| 关系 | `LT GT EQ MEMBER` | `< > = @` |
| 逻辑 | `AND OR NOT` | `& | !` |
| 集合 | `UNION INTERSECTION` | `U I` |
| 标点 | `DOT LPAREN RPAREN LBRACE RBRACE COLON` | `. ( ) { } :` |
| 结束 | `EOF` | 输入结束 |

词法规则：

- 使用最长匹配。`letbe` 是一个 `ID`；`be123+a` 是 `BE NUM PLUS ID`。
- 关键字只在完整的小写字母串与关键字相等时识别。
- 除 `U` 和 `I` 外的大写字母非法。
- `012`、`00` 等前导零形式整体报告 lexical error，不拆成多个合法整数。
- `-` 永远是运算符，不是数字字面量的一部分。
- 数字在 lexer 中转换为 `int64_t`；超出范围报告 lexical error，禁止未定义行为。
- 每个 token 保存 kind、原始 lexeme、开始/结束位置以及 `NUM` 的已解析数值。

### 3.4 类型与值

用户可观察的类型只有：

- `integer`：使用 `int64_t` 表示。
- `boolean`：`true` 或 `false`。
- `set`：整数集合，以 representative 与 characteristic predicate 表示。

编译器内部还可以使用：

- `void`：没有运行时值的语法成分。
- `error`：已诊断的类型失败。
- `program`、`declaration`、`calculation` 等节点角色，但这些角色不能与值类型混用。

### 3.5 运算及优先级

由高到低：

1. 圆括号和集合字面量；
2. `!`；
3. `*` 与集合交 `I`；
4. `+`、`-` 与集合并 `U`；
5. `<`、`>`、`=`、`@`；
6. `&`；
7. `|`。

实际结合性由第 5 节文法决定。`+`、`-`、`*`、`U`、`I` 均左结合。

运算签名：

```text
integer + integer -> integer
integer - integer -> integer
integer * integer -> integer
set U set         -> set
set I set         -> set
integer < integer -> boolean
integer > integer -> boolean
integer = integer -> boolean
integer @ set     -> boolean
boolean & boolean -> boolean
boolean | boolean -> boolean
! boolean         -> boolean
```

### 3.6 集合语义

集合字面量格式：

```text
{ representative : predicate }
```

规则如下：

- representative 的作用域只覆盖同一个集合字面量的 predicate，静态类型固定为 `integer`。
- representative 可以遮蔽同名全局变量。
- 除 representative 外的标识符必须引用先前成功声明的全局变量。
- 集合值是符号值，不尝试枚举元素。
- 集合并把两个 characteristic predicates 连接为 OR。
- 集合交把两个 characteristic predicates 连接为 AND。
- 合并两个集合前必须进行 alpha-renaming，避免 representative 捕获。
- 普通 `show` 保留集合运算结构，只做保证正确求值所需的替换和常量计算，不进行完整逻辑化简。
- 成员测试把左侧整数绑定到集合 representative，再对 predicate 求值。
- 集合声明时，全局不可变值被解析到符号环境中；之后不存在赋值，因此引用语义不会随时间变化。

示例：

```text
show { x : x > 3 } U { y : y > 5 } .
```

规范化显示可以是：

```text
(set) { x : (x > 3) | (x > 5) }
```

不自动显示为 `{ x : x > 3 }`。

## 4. 词法分析规范

### 4.1 Lexer 接口职责

Lexer 接受不可变 source buffer，并按需返回下一个 token。建议接口：

```c
typedef struct UslcLexer UslcLexer;

void uslc_lexer_init(UslcLexer *lexer,
                     const UslcSource *source,
                     UslcDiagnostics *diagnostics);
UslcToken uslc_lexer_next(UslcLexer *lexer);
```

Lexer 不操作文件、不打印消息、不退出进程。它只维护扫描状态并向 diagnostics 模块报告错误。

### 4.2 Symbol table 不属于 lexer

原课程说明要求 lexer 遇到 `id` 时将其放入 symbol table。标准实现不采用这一耦合：lexer 只识别和保留标识符文本；声明、查找、作用域和类型由 semantic analysis 管理。这样可避免把拼写识别与名字绑定混为一谈。

### 4.3 Token 流不变量

- 成功扫描后恰好产生一个 `EOF`。
- token spans 单调递增且不重叠。
- lexeme 是 source buffer 的 slice，默认不为每个 token 单独分配字符串。
- lexical error 后可以生成一个 `ERROR` token 供诊断，但 driver 立即停止，不进入 parser。

## 5. 核心文法

终结符使用第 3.3 节的 lexeme 写法，`$` 表示 EOF。编号是稳定 ABI，用于 parse trace、测试和 JSON 输出。

```text
 0. S'  -> S
 1. S   -> D' C .
 2. S   -> C .
 3. D'1 -> D D'2
 4. D'  -> D
 5. D   -> let T id be E .
 6. T   -> int
 7. T   -> set
 8. E   -> E'
 9. E1  -> E2 U E'
10. E1  -> E2 + E'
11. E1  -> E2 - E'
12. E'  -> E''
13. E'1 -> E'2 I E''
14. E'1 -> E'2 * E''
15. E'' -> num
16. E'' -> id
17. E'' -> ( E )
18. E'' -> { Z P }
19. Z   -> id :
20. P1  -> P2 | P'
21. P   -> P'
22. P'1 -> P'2 & P''
23. P'  -> P''
24. P'' -> R
25. P'' -> ( P )
26. P'' -> ! R
27. R   -> E1 < E2
28. R   -> E1 > E2
29. R   -> E1 = E2
30. R   -> E1 @ E2
31. C   -> show A
32. A   -> E
33. A   -> P
```

实现中不把下标写入 symbol name。下标仅用于说明同一产生式中相同非终结符的不同实例。

该文法保证：声明只能出现在计算语句之前；计算语句恰好一个；声明和程序都以 `.` 结束。

## 6. SLR(1) 生成与语法分析

### 6.1 不加载课程 CSV

标准实现不在运行时读取 `SLR Parsing Table.csv`，也不手工维护一张不可审计的大数组。程序从第 5 节对应的静态 production definitions 直接构造 SLR(1) 表。课程 CSV 只用于开发阶段的交叉验证。

### 6.2 SLR generator

必须显式实现：

1. grammar symbol、production 和 LR item 的数据结构；
2. nullable、FIRST 和 FOLLOW 集；
3. LR(0) `closure(I)`；
4. LR(0) `goto(I, X)`；
5. canonical LR(0) item set collection；
6. ACTION/GOTO 表构造；
7. shift/reduce 与 reduce/reduce conflict 检测；
8. 可选的 grammar、item sets 和 table dump。

虽然当前文法没有 epsilon production，FIRST/FOLLOW 实现仍应正确处理 epsilon，以保证算法完整并便于测试。

ACTION 规则：

- item `[A -> alpha . a beta]` 且 `a` 为 terminal：在 `ACTION[state, a]` 放入 shift。
- item `[A -> alpha .]` 且 `A != S'`：对每个 `a in FOLLOW(A)` 放入 reduce `A -> alpha`。
- item `[S' -> S .]`：在 `ACTION[state, EOF]` 放入 accept。
- 任何单元格出现不同动作即为 grammar conflict，属于 internal/specification error。

### 6.3 Parser

Parser 同时维护：

- state stack；
- parse-node stack；
- 一个 lookahead token。

操作：

- shift：压入 terminal node 与目标 state，然后读取下一个 token。
- reduce：弹出 RHS 长度对应的 states/nodes，按原顺序创建 nonterminal node，通过 GOTO 压回。
- accept：返回 `S` 根节点。
- error：根据当前 state 中存在动作的 terminals 生成 expected-token set。

Parser 不直接执行类型检查或求值。课程原文允许归约时同步执行 L-attributed semantics；本实现将阶段分离，以便每个阶段独立测试、打印和诊断。

### 6.4 Parse tree

每个节点至少包含：

```text
symbol
production_id       # terminal 为 none
source_span
children[]
token               # 仅 terminal
```

children 保持源程序顺序，不能为了求值方便重排。parse tree 可以完整输出到 `parser_out.json`。

## 7. AST 降低

Parse tree 完整反映文法，AST 则去除纯标点和单子产生式。建议节点：

```text
Program
Declaration
ShowCommand
IntegerLiteral
Name
SetLiteral
BinaryArithmetic
BinarySet
Comparison
Membership
LogicalBinary
LogicalNot
```

每个 AST node 保存：

- kind；
- source span；
- child pointers；
- type-check 后的 `UslcType`；
- evaluation 后的可选 `UslcValue`。

AST lowering 必须是纯结构转换，不查符号表，不执行常量折叠，也不打印错误。遇到不可能的 parse tree 结构时报告 internal error。

## 8. 名字绑定与类型检查

### 8.1 作用域

- 程序有一个全局作用域。
- 每个 set literal 创建一个仅包含 representative 的子作用域。
- representative 固定为 `integer`，只在 predicate 中可见。
- 内层 representative 可以遮蔽外层同名变量。
- 全局声明不可重复。
- 变量在其 initializer 完成类型检查之后才可见。

### 8.2 类型规则

```text
Gamma |- num : integer
Gamma(id) = T  =>  Gamma |- id : T

Gamma |- e1 : integer, Gamma |- e2 : integer
------------------------------------------------
Gamma |- e1 + e2 : integer
Gamma |- e1 - e2 : integer
Gamma |- e1 * e2 : integer

Gamma |- e1 : set, Gamma |- e2 : set
----------------------------------------
Gamma |- e1 U e2 : set
Gamma |- e1 I e2 : set

Gamma |- e1 : integer, Gamma |- e2 : integer
------------------------------------------------
Gamma |- e1 < e2 : boolean
Gamma |- e1 > e2 : boolean
Gamma |- e1 = e2 : boolean

Gamma |- e1 : integer, Gamma |- e2 : set
--------------------------------------------
Gamma |- e1 @ e2 : boolean

Gamma |- p : boolean, Gamma |- q : boolean
--------------------------------------------
Gamma |- p & q : boolean
Gamma |- p | q : boolean
Gamma |- !p : boolean

Gamma, x : integer |- p : boolean
-------------------------------------
Gamma |- { x : p } : set

Gamma |- e : T, id not in Gamma
-------------------------------------
Gamma |- let T id be e : declaration

Gamma |- a : integer | set | boolean
-------------------------------------
Gamma |- show a : calculation
```

所有不满足签名的运算都报告 type error。一个节点已有错误后，父节点可以标记 `error`，但不能为同一根因产生大量级联诊断。默认只报告第一个用户错误。

### 8.3 与原类型文档的有意差异

- 声明规则额外检查 `T == type(E)`；否则原规则会错误接受 `let set x be 4 .`。
- symbol table 在 semantic phase 建立，不在 lexical phase 建立。
- 对外统一使用 `boolean`，不把 `relation` 和 `predicate` 当作运行时类型。它们是 AST 节点类别。
- `program`、`declaration` 和 `calculation` 是语法角色，不参与普通运算符类型比较。

## 9. 求值语义

### 9.1 阶段前提

只有 lexical、syntax 和 type checking 全部成功才进入 evaluator。Evaluator 不承担修复类型错误的责任，但仍检查内部不变量并返回结构化错误。

### 9.2 整数

- 使用 `int64_t`。
- 加、减、乘必须通过 checked helpers 实现，不能触发有符号整数溢出的未定义行为。
- 发生溢出时报告 evaluation error，并指出运算符与 source span。
- 运算顺序按 AST 左到右。

### 9.3 布尔与短路

- `p & q`：`p` 为 false 时不求值 `q`。
- `p | q`：`p` 为 true 时不求值 `q`。
- `!p`：逻辑取反。
- `<`、`>`、`=` 比较整数。

### 9.4 符号集合

```c
typedef struct {
    UslcString representative;
    UslcPredicate *predicate;
} UslcSetValue;
```

集合并、交创建新的 predicate tree，不枚举整数。打印集合时执行 alpha-renaming，以稳定的 representative 名称显示，并插入足够括号保证含义明确。

成员测试 `n @ set` 在只包含局部 representative 与已捕获常量的环境中执行 predicate。若 evaluator 遇到未绑定名字，视为 internal invariant failure，因为 type checker 本应阻止它。

### 9.5 声明和 show

- 声明按源代码顺序求值。
- 声明值不可变。
- initializer 成功后才把 `(name, type, value)` 放入运行时环境。
- `show` 返回一个 `UslcValue`，driver 负责显示和 artifact 输出。

## 10. 命令行与输出

### 10.1 CLI

命令行：

```text
uslc [options] <source-file>
uslc [options] -
```

`-` 表示从 stdin 读取。

选项：

```text
--phase lex|parse|check|run   在指定阶段停止，默认 run
--emit-dir <path>            写出已经完成阶段的 JSON artifacts
--trace-parser               向 stderr 打印逐步 shift/reduce trace
--dump-grammar               打印 productions、FIRST 和 FOLLOW
--dump-slr                   打印 LR(0) item sets 与 ACTION/GOTO 表
--compact-json               artifact JSON 不缩进；默认两空格缩进
--help                       打印帮助
--version                    打印版本
```

`--` 结束选项解析，之后可以使用以 `-` 开头的文件名。`--dump-grammar` 和 `--dump-slr` 也可以不带源文件单独使用；dump 和 trace 写入 stderr。成功报告写 stdout，错误报告写 stderr。

核心 CLI 不读取配置文件或环境变量。所有影响语义和输出的选项都必须显式出现在命令行。

### 10.2 成功输出

默认 stdout 使用稳定的纯文本结构，不依赖 ANSI color：

```text
UIC Set Language Compiler
source: examples/basic.set
status: success

phases:
  lexical:  ok (18 tokens)
  syntax:   ok (42 parse nodes)
  typing:   ok
  evaluate: ok

result:
  type:  boolean
  value: true
```

集合结果：

```text
result:
  type:  set
  value: { x : (x > 3) & (x < 5) }
```

进度与 trace 写 stderr，最终用户结果写 stdout。

### 10.3 错误输出

```text
UIC Set Language Compiler
source: examples/bad-type.set
status: error

diagnostic:
  phase:    typing
  code:     T3003
  location: 2:14-2:15
  message:  operator '@' expects integer @ set
  found:    integer @ integer

2 | show x @ y .
  |          ^
```

诊断必须包含 phase、稳定 error code、source span、简明消息和 source excerpt。路径或源码不可用时可以省略 excerpt，但不能省略 code 和 message。

### 10.4 Exit codes

| Code | Meaning |
| ---: | --- |
| 0 | 成功 |
| 1 | lexical error |
| 2 | syntax error |
| 3 | type error |
| 4 | evaluation error |
| 64 | CLI usage error |
| 66 | input file error |
| 70 | internal/specification error |
| 74 | output file error |

### 10.5 JSON artifacts

只有指定 `--emit-dir` 时才创建 artifacts；失败时只写成功阶段和 `diagnostics.json`，不创建空文件。

`--emit-dir` 指定的目录必须预先存在。ISO C 标准库没有创建目录的接口，因此由调用方的 shell 创建目录，产品代码只使用标准文件 I/O。每次调用会替换该目录中以下六个已知文件，并清理先前运行遗留的后续阶段 artifacts；其他文件保留。建议为每个源程序使用独立输出目录。`--phase parse` 包含纯结构的 AST lowering，因此成功时也写出 `ast.json`；`ast.json` 保存类型检查前的树，`typing_out.json` 保存绑定与类型检查后的树。

```text
tokens.json
parser_out.json
ast.json
typing_out.json
evaluation_out.json
diagnostics.json
```

所有 JSON：

- UTF-8、无 BOM；
- 默认两空格缩进；
- 最后一个字节为 newline；
- object key 顺序固定；
- 完整实现 JSON string escaping；
- 不依赖第三方 JSON 库。

Token 示例：

```json
{
  "schema": "uslc.tokens.v1",
  "source": "examples/three.set",
  "tokens": [
    {
      "kind": "SHOW",
      "lexeme": "show",
      "span": {
        "start": { "line": 1, "column": 1, "offset": 0 },
        "end": { "line": 1, "column": 5, "offset": 4 }
      }
    }
  ]
}
```

Evaluation 示例：

```json
{
  "schema": "uslc.evaluation.v1",
  "status": "success",
  "result": {
    "type": "integer",
    "value": 3
  }
}
```

JSON numbers 仅用于 `int64_t` 范围内整数；集合 predicate 使用结构化 AST，同时可以附加一个确定性的 display string。

## 11. Diagnostics

建议 error code：

```text
L1001 invalid character
L1002 leading zero in integer literal
L1003 integer literal out of range

P2001 unexpected token
P2002 unexpected end of input
P2090 grammar conflict

T3001 undefined identifier
T3002 duplicate declaration
T3003 incompatible operand types
T3004 initializer type mismatch
T3005 invalid show value

E4001 integer overflow
E4002 unbound set representative
E4090 evaluator invariant failure

I9001 out of memory
I9002 malformed internal grammar
I9003 malformed parse tree
```

Diagnostics 容器保存 code、phase、message、span 和可选 notes。底层模块不能直接 `printf` 用户错误；driver 统一格式化。

## 12. 内存与所有权

### 12.1 Source lifetime

整个编译过程持有完整 source buffer。Token lexemes 使用 `(offset, length)` 引用 source，不复制。

### 12.2 Arena

Parse tree、AST 与 symbolic predicate nodes 使用各自的 monotonic arena。单次编译结束后整体释放。Arena 必须：

- 正确对齐；
- 检查 `size + alignment` 与扩容溢出；
- 在 allocation failure 时返回错误，不调用 `abort()`；
- 允许空 arena 安全释放。

严格 C23 构建使用 `max_align_t`。Windows MSVC compatibility build 通过编译探针检测该类型；缺失时启用包含 `long double`、`int64_t`、对象指针和函数指针的对齐 union。测试单独验证所有 arena 节点类型的对齐要求。

当前通用文法容器支持最多 64 个符号、32 个终结符、64 条产生式和每条最多 8 个 RHS 符号，越界文法报告 `I9002`。运行时数组动态增长。树高及递归处理深度限制为 255，超过限制报告 `I9004`（exit 70），避免宿主栈耗尽；该限制也适用于极长的左结合表达式、声明列表与嵌套符号集合。

可增长 arrays、diagnostics 和 symbol table 使用明确的 `init/free` API。每个 owning struct 的头文件注释必须说明所有权。

### 12.3 禁止事项

- 不返回指向临时栈对象的指针。
- 不依赖 signed overflow、越界指针或未初始化内存。
- 不把 `realloc` 结果直接覆盖原指针。
- 不以 `exit()` 代替库函数错误返回。
- 不在 headers 中定义可变全局对象。

## 13. 计划目录结构

```text
StandardImplementation/
├── README.md
├── LICENSE                    # 确定许可证后添加
├── .gitignore
├── include/
│   └── uslc/
│       ├── arena.h
│       ├── ast.h
│       ├── diagnostic.h
│       ├── evaluator.h
│       ├── grammar.h
│       ├── json_writer.h
│       ├── lexer.h
│       ├── parser.h
│       ├── printer.h
│       ├── slr.h
│       ├── source.h
│       ├── symbol_table.h
│       ├── token.h
│       └── type_checker.h
├── src/
│   ├── arena.c
│   ├── ast.c
│   ├── diagnostic.c
│   ├── evaluator.c
│   ├── grammar.c
│   ├── json_writer.c
│   ├── lexer.c
│   ├── main.c
│   ├── parser.c
│   ├── printer.c
│   ├── slr.c
│   ├── source.c
│   ├── symbol_table.c
│   ├── token.c
│   └── type_checker.c
├── scripts/
│   ├── build.sh
│   ├── build.ps1
│   ├── clean.sh
│   ├── clean.ps1
│   ├── test.sh
│   └── test.ps1
├── examples/
│   ├── arithmetic.set
│   ├── sets.set
│   └── errors.set
├── tests/
│   ├── test_main.c
│   ├── test_lexer.c
│   ├── test_grammar.c
│   ├── test_slr.c
│   ├── test_parser.c
│   ├── test_type_checker.c
│   ├── test_evaluator.c
│   ├── test_json_writer.c
│   └── fixtures/
└── build/                     # generated, ignored
```

初始实现可以按里程碑逐步增加文件，但最终模块边界应与该结构一致。

## 14. C 编码规范

- 使用 C23，公开符号统一以 `uslc_` 前缀命名。
- 类型使用 `UslcName`，函数和变量使用 `uslc_name`，enum constants 使用 `USLC_NAME`。
- 每个 header 有 include guard，并能独立 include。
- 对外优先使用 opaque structs；简单 immutable value structs 可以公开。
- include 顺序：对应 header、C standard library、项目 headers。
- 不使用 VLAs、`alloca`、非标准 `strdup` 或实现定义的位域布局。
- 使用 `<stdint.h>`、`<stdbool.h>`、`<stddef.h>` 和 `<inttypes.h>`。
- 长度和容量使用 `size_t`；源位置 offset 使用 `size_t`；语言整数使用 `int64_t`。
- 函数返回 status 或 tagged result，不用 sentinel pointer 混淆“未找到”和“内存失败”。
- `const` 表达不可变借用；所有权转移必须在 API 注释中写明。
- 正常用户错误不得触发 assertion。Assertions 只检查开发期内部不变量。
- 所有输出经过 printer/json writer，不在算法代码中散落格式字符串。

## 15. 构建规范

### 15.1 共同规则

- 不下载任何依赖。
- 不生成 IDE project 或中间构建描述文件。
- script 在任意当前工作目录下调用时都先定位 repository root。
- script 打印即将执行的 compiler command。
- object files 放入 `build/<configuration>/obj/`。
- executable 放入 `build/<configuration>/uslc` 或 `uslc.exe`。
- Debug 默认开启调试信息和 assertions；Release 开启优化并定义 `NDEBUG`。
- 构建失败返回编译器的非零 exit code。

### 15.2 C23 与工具链策略

- 规范语言版本是 ISO/IEC 9899:2024，即 C23。
- Clang/GCC 的 strict build 必须使用 `-std=c23`，并通过 compile probe 确认 `__STDC_VERSION__ >= 202311L`；不静默退回 `-std=c2x`。
- Linux 与 Windows 的 reference compiler 均为 Clang 18 或更新版本，以便同一套 flags 和语言规则跨平台验证。
- GCC 14 或更新版本是第二条 conforming toolchain，用于发现 compiler-specific assumptions。
- MSVC 是 Windows compatibility toolchain。MSVC 19.51 只提供 `/std:clatest`，不声称完整 C23 conformance，因此不能单独作为 strict C23 release gate。
- 产品代码应保持为 GCC、Clang 和 MSVC 都能接受的 C23 子集。若使用较新的 C23 facility，必须有 feature detection、独立测试以及不改变语义的可移植写法。
- 编译器与 shell 是构建工具，不是 `uslc` binary 的运行时依赖。发布 executable 仍只依赖平台 C runtime/C standard library。

本机 WSL 2 验证基线：

```text
distribution: Ubuntu-24.04 (WSL 2)
release:      Ubuntu 24.04.4 LTS
kernel:       6.18.33.2-microsoft-standard-WSL2
clang:        18.1.3, -std=c23, __STDC_VERSION__ = 202311L
gcc/cc:       13.3.0, only the older -std=c2x draft mode
```

因此 Ubuntu 24.04 reference build 默认选择 `clang`。GCC 13 的 `-std=c2x` 只能用于额外兼容性实验，不能标记为 C23 验收通过。

本机 Windows 工具链包括 MSVC 19.51.36257，其 C language mode 列表为 `/std:c11`、`/std:c17` 和 `/std:clatest`。实现验收时，Visual Studio 的 x64 LLVM 目录已提供 Clang 22.1.3，可在配置好的 Developer PowerShell 中用于 Windows strict C23 gate；普通终端的 PATH 不一定包含它，可通过 `-Compiler` 显式指定路径。

### 15.3 Linux

```text
./scripts/build.sh debug
./scripts/build.sh release
./scripts/test.sh debug
./scripts/clean.sh
```

`build.sh` 优先使用显式 `CC`；未设置时依次探测 `clang` 和 `cc`。严格 C23 reference build 使用 Clang 18+ 或 GCC 14+。建议 flags：

```text
-std=c23 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
```

Debug 增加 `-O0 -g3`；Release 增加 `-O2 -DNDEBUG`。`-Werror` 不作为默认选项，避免不同编译器版本的新增 warning 阻塞普通构建；CI/strict 模式可以显式启用。

可选的开发检查可以使用编译器 sanitizers，但 sanitizer 不是发布程序的运行时依赖，也不能成为基础测试通过的必要条件。

从 Windows 主机调用 WSL 2 验证时，使用明确的发行版和仓库路径。例如：

```powershell
wsl.exe -d Ubuntu-24.04 -- bash -lc `
  'cd "/mnt/c/path/to/StandardImplementation" && ./scripts/build.sh debug && ./scripts/test.sh debug'
```

脚本不得因为仓库位于 `/mnt/c` 而依赖 Windows path semantics；Linux 构建产物仍放在仓库的 `build/` 中。

### 15.4 Windows

```powershell
pwsh -NoProfile -File .\scripts\build.ps1 -Configuration Debug
pwsh -NoProfile -File .\scripts\build.ps1 -Configuration Release
pwsh -NoProfile -File .\scripts\test.ps1 -Configuration Debug
pwsh -NoProfile -File .\scripts\clean.ps1
```

`build.ps1` 优先使用 PATH 中的 `clang.exe`，其次尝试 `gcc.exe`，最后尝试 `cl.exe`。Clang/GCC 使用 `-std=c23`。MSVC 当前没有命名为 `/std:c23` 的选项，兼容构建使用：

```text
/nologo /std:clatest /W4 /permissive- /utf-8
```

Debug 增加 `/Od /Zi`；Release 增加 `/O2 /DNDEBUG`。若使用 `cl.exe`，用户需要在 Developer PowerShell 中运行，script 不搜索注册表、不调用 Visual Studio 安装器。

PowerShell script 使用参数数组调用编译器，不能拼接后交给 `Invoke-Expression`。

## 16. 测试策略

### 16.1 测试框架

项目提供极小的自有 C test harness：

```text
USLC_TEST(name)
USLC_EXPECT(condition)
USLC_EXPECT_EQ_INT(expected, actual)
USLC_EXPECT_STREQ(expected, actual)
```

Harness 只使用 C standard library。每个失败打印 test name、文件、行号和差异，进程以非零状态结束。

### 16.2 单元测试

Lexer：

- 每种 token；
- 无空白相邻 token；
- CRLF/LF；
- keyword 与长 identifier；
- 非法字符、大写字母、前导零、整数溢出；
- source spans。

Grammar/SLR：

- FIRST/FOLLOW 小型 fixture grammars；
- epsilon 与递归；
- closure/goto；
- canonical set 去重；
- conflict detection；
- 当前文法零冲突；
- 生成表与保留的课程 CSV 在开发期一致。

Parser：

- 无声明程序；
- 多声明程序；
- 运算优先级和左结合；
- 集合、predicate 与嵌套括号；
- 缺失 token、额外 token、提前 EOF；
- expected-token diagnostics；
- parse tree child order。

Type checker：

- 每个合法运算签名；
- 每个 operand mismatch；
- 声明 initializer mismatch；
- undefined、duplicate 和 self-reference；
- representative scope/shadowing；
- `@` 左 int 右 set；
- show integer/set/boolean。

Evaluator：

- 正负整数结果；
- checked overflow；
- 布尔短路；
- 集合并、交和 alpha-renaming；
- membership true/false；
- declaration environment；
- deterministic printing。

JSON writer：

- quote、backslash 和 control characters；
- UTF-8 passthrough；
- comma/indentation state；
- stable key order；
- write failure propagation。

### 16.3 集成测试

每个 fixture 包含：

```text
case-name.set
case-name.stdout
case-name.stderr
case-name.exit
case-name.artifacts/      # 需要时
```

Test scripts 构建 executable、运行 fixtures 并做字节级比较。Linux 使用系统 shell 基础能力；Windows 使用 PowerShell/.NET 文件 API。测试不依赖 Python、Perl、jq 或 diff 工具。

字节比较由自有 C test executable 实现。人类可读报告使用宿主 text stream 换行（Linux LF、Windows CRLF）；Windows 比较器将仓库中 LF 的 stdout/stderr 基准映射为 CRLF 后逐字节比较。JSON artifacts 在两平台均使用二进制文件流写入，严格比较相同的 LF 字节。`.gitattributes` 固定源文件和测试基准为 LF，保证 offset 稳定。fixture 的可选 `.args` 文件每行保存一个 CLI 参数。

### 16.4 最低验收矩阵

| Platform | Compiler | Debug | Release | Tests |
| --- | --- | --- | --- | --- |
| Linux | Clang 18+ | required | required | required |
| Windows | Clang 18+ | required | required | required |
| Linux | GCC 14+ | recommended | recommended | recommended |
| Windows | MSVC 19.5x `/std:clatest` | compatibility | compatibility | compatibility |

## 17. 实现里程碑

### M0 Repository foundation

- 建立目录与 headers/source skeleton。
- 完成 Linux/Windows build、clean、test scripts。
- 完成 source、diagnostic、arena 和 test harness。
- 验收：两平台能构建空 driver，并运行一个 smoke test。

### M1 Lexer

- 完成 token kinds、source spans、integer conversion 和 diagnostics。
- 完成 lexer unit tests 与 `--phase lex`。
- 验收：合法 token fixtures 全部稳定，非法输入产生精确位置和 exit code 1。

### M2 Grammar and SLR generator

- 编码 34 条 productions。
- 完成 FIRST/FOLLOW、LR(0) item sets、ACTION/GOTO 与 conflict checks。
- 完成 dump commands。
- 验收：当前 grammar 无冲突；关键状态与课程表交叉验证一致。

### M3 Parser and parse tree

- 完成 table-driven shift/reduce parser。
- 构建 parse tree，完成 syntax diagnostics 和 JSON。
- 验收：正确程序 accept，错误程序给出 expected tokens，exit code 2。

### M4 AST and type checker

- 完成 AST lowering、symbol table 和 scopes。
- 实现第 8 节全部类型规则。
- 验收：所有合法/非法运算组合、声明和 representative scope 测试通过。

### M5 Evaluator

- 完成 immutable environment、checked integer arithmetic、boolean 和 symbolic sets。
- 完成 deterministic value printer。
- 验收：课程示例和新增边界测试全部得到规范结果。

### M6 Structured output and hardening

- 完成 CLI、human printer、JSON writer 和 artifacts。
- 在 GCC、Clang、MSVC 上清理 warnings。
- 运行内存检查与故障注入测试。
- 验收：最低平台矩阵通过，输出稳定，无已知 leak/UB。

### M7 Optional simplify extension

- 在核心版本稳定后才开始。
- 为 grammar 增加 `simplify` command，并由同一个 SLR generator 重新生成表。
- 将 predicate 转换为 NNF，再转换为 DNF。
- 对关于 representative 的一次整数约束进行规范化与区间合并。
- 支持集合 membership substitution 与 distribution law。
- 高于一次的 representative expression 报告明确 semantic error。
- 更新语言版本、grammar dumps、类型规则、CLI 和 tests。

## 18. Bonus simplify 的规范方向

本节是 M7 的设计约束，不属于核心验收。

### 18.1 目标输出

- 空集统一显示 `{ x : false }`。
- 全集统一显示 `{ x : true }`。
- 普通结果转换为 DNF。
- conjunction 内约束顺序稳定；disjunction 中区间按下界排序。
- 相交或相邻的整数区间按数学上等价的方式合并。

### 18.2 线性限制

对 representative `x`，允许转换为 `a*x + b` 的整数算术表达式视为一次或零次。乘法只有在至少一个 operand 与 `x` 无关时仍保持一次；`x*x`、`x*x*x` 等为高阶。

建议 pipeline：

1. 用已声明常量替换 free identifiers；
2. 展开集合 membership；
3. 常量折叠；
4. 消除双重否定并用 De Morgan 推入 NOT；
5. 把比较转换成整数上下界约束；
6. 分配 AND over OR，得到 DNF；
7. 化简每个 minterm；
8. 删除空 minterm、合并区间、排序并格式化。

所有变换必须有语义等价测试。不得仅为了匹配原文示例而输出数学上不等价的区间。

## 19. Definition of Done

一个里程碑只有在以下条件全部满足时才完成：

1. README 中对应规范已实现；若设计改变，先更新 README。
2. 新功能有正常、边界和错误路径测试。
3. Linux GCC/Clang 与 Windows MSVC 的适用构建通过。
4. 编译没有项目自身可修复的 warnings。
5. 错误路径释放所有已拥有资源或由 arena 统一释放。
6. 输出、exit code 和 JSON schema 有稳定测试。
7. 不引入第三方或平台专有产品代码依赖。
8. `git status` 中不出现 build products、临时文件或测试输出。

## 20. 当前状态

核心 M0–M6 的模块和工程脚本已实现：source/diagnostics/arena、lexer、FIRST/FOLLOW、LR(0)/SLR、parse tree、AST、symbol table/type checker、符号集合 evaluator、文本和 JSON 输出。课程 CSV 的 66 个状态与生成表通过状态重编号后的逐单元格交叉验证；产品运行时不读取 CSV。M7 `simplify` 尚未实现，不属于核心语言。

快速运行（在 `StandardImplementation` 中）：

```sh
./scripts/build.sh debug
./scripts/test.sh debug
./build/debug/uslc examples/membership.set
mkdir -p build/artifacts
./build/debug/uslc --emit-dir build/artifacts examples/sets.set
```

```powershell
# 在 Developer PowerShell 中运行 MSVC compatibility build；有 Clang 时优先选择 Clang。
pwsh -NoProfile -File .\scripts\build.ps1 -Configuration Debug
pwsh -NoProfile -File .\scripts\test.ps1 -Configuration Debug
.\build\debug\uslc.exe examples/membership.set
New-Item -ItemType Directory -Force build/artifacts
.\build\debug\uslc.exe --emit-dir build/artifacts examples/sets.set
```

Windows 可使用 `-Compiler <path>` 显式选择编译器，`-Strict` 禁止退回 MSVC compatibility mode。测试脚本默认重新构建；已有同配置构建可使用 Linux `--no-build` 或 Windows `-NoBuild`。PowerShell 脚本要求 PowerShell 7。

验证记录见 `VALIDATION.md`。Linux 与 Windows 的 strict C23 gate 使用 Clang；MSVC compatibility 与 GCC 13 的额外草案模式实验单独记录，不替代 strict C23 gate。
