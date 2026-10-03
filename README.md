# uslc DirectParse

本文档是 `DirectParse` 的唯一工程规范。后续设计、实现、测试和重构均以本文档为准；原始课程文档仅作为需求来源和历史参考。若两者存在冲突，以本文档中的“规范裁决”与明确规则为准。

`uslc` 是 UIC Set Language Compiler 的命令名和项目名；`DirectParse` 是面向 Unix-like 系统的工具驱动实现。项目使用 Flex 生成 reentrant scanner、GNU Bison 生成 pure LALR(1) parser，并用 json-c 构造和序列化 JSON。业务逻辑使用 ISO C23，生成器输出与 CLI 运行在 POSIX 环境中。本版本明确依赖 Flex、Bison 和 json-c，不自行重造 lexer/parser generator 或 JSON serializer。

## 1. 工程目标

### 1.1 必须实现

1. 从文件或标准输入读取 Set Algebra 源程序。
2. 完成词法分析，生成带源位置的 token 流。
3. 使用 GNU Bison 根据本文档中的文法生成 LALR(1) parser，并检查 grammar conflicts。
4. 使用表驱动的移进归约算法完成语法分析，并构造完整 parse tree。
5. 将 parse tree 降低为更适合语义处理的 AST。
6. 完成符号表管理、作用域处理和静态类型检查。
7. 对正确程序进行求值，支持整数、布尔值和符号集合。
8. 使用 json-c 输出稳定、结构化、可读的诊断、执行结果和可选 JSON artifacts。
9. 在 Linux、BSD 和 macOS 等 Unix-like 系统上通过 POSIX shell scripts 构建和测试；当前 Windows 主机必须通过 WSL 2 执行。
10. 为所有核心模块提供不依赖第三方测试框架的自动化测试。

### 1.2 设计取向

- 工具驱动但可审计：lexer rules 和 grammar actions 明确写在 `.l`/`.y` 中，并保留 Bison state report；不把业务语义埋进生成代码。
- 可解释：token stream、Bison automaton report、parse tree 和 AST 均可按需检查。
- Unix-like 可移植：目标是 POSIX.1-2008 环境，支持 Linux、BSD、macOS 和 WSL 2；不支持原生 Windows。
- 确定性：同一输入产生字节级稳定的文本和 JSON 输出。
- 内存安全：所有长度计算、整数运算和动态分配都检查溢出与失败。
- 清晰优先：模块边界、所有权和错误路径必须显式，不追求晦涩的微优化。

### 1.3 非目标

- 不实现 GUI、网络服务、IDE 插件或交互式 REPL。
- 不枚举无限集合；集合始终以特征谓词表示。
- 不实现错误恢复。遇到当前阶段的首个错误后停止后续阶段。
- 不实现字符串、浮点数、数组、集合的集合或用户自定义函数。
- 不要求兼容 AUTOLAB 的逐字输出、旧 `make.bat` 或原始启动代码 ABI。
- 不使用 CMake、Make、Ninja 或 Meson。允许且只允许 Flex、GNU Bison、json-c 和 `pkg-config` 作为核心外部依赖；测试框架仍由少量 C 宏实现。

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

源文件通过 `rb` 模式读取，stdin 直接从标准输入流读取。在支持的 Unix-like 平台上不发生文本模式换行转换；source buffer 保留实际读入的 CRLF 与其他原始字节，所有 byte offset 均以该 buffer 为准。

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

## 4. Flex 词法分析规范

### 4.1 Scanner 文件与模式

词法规则写在 `src/lexer.l`，Flex 生成文件只进入 `build/generated/`，不提交到 Git。scanner 必须配置为 reentrant，并与 Bison 的 semantic values 和 locations 对接：

```lex
%option reentrant bison-bridge bison-locations
%option noyywrap nounput noinput nodefault
%option prefix="uslc_"
```

实际生成命令通过 `--outfile` 和 `--header-file` 把输出放到 build tree，不允许生成文件污染 `src/`。

### 4.2 Flex 规则

- `[ \t\r\n\f\v]+` 忽略，同时更新 line、column 和 offset。
- `let|be|int|set|show` 在 identifier rule 之前返回关键字 token。
- `[a-z]+` 返回 `ID`，lexeme 复制到 parse-owned arena 或 intern table。
- `0|[1-9][0-9]*` 返回 `NUM`，使用 checked conversion 得到 `int64_t`。
- `[0-9]+` fallback rule 捕获 `00`、`012` 等前导零错误。
- `U`、`I` 返回集合运算 token。
- 单字符运算符和标点分别返回对应 Bison token。
- `<<EOF>>` 返回 parser EOF。
- 最终 `.` rule 捕获任意非法字节并报告 lexical error。

Flex 采用最长匹配，因此 `letbe` 由 identifier rule 接收，而 `be123+a` 依次产生 `BE NUM PLUS ID`。

### 4.3 Location tracking

`YYLTYPE` 扩展为包含开始/结束 line、column 和 byte offset。使用 `YY_USER_ACTION` 在每次 rule action 前统一更新 span，避免每条 rule 重复位置代码。CRLF 必须只增加一次行号。

### 4.4 Scanner API 与职责

建议在 Flex 生成 API 外包装稳定接口：

```c
typedef struct UslcScanner UslcScanner;

UslcStatus uslc_scanner_init(UslcScanner *scanner,
                             const UslcSource *source,
                             UslcDiagnostics *diagnostics);
UslcStatus uslc_scanner_destroy(UslcScanner *scanner);
```

Flex scanner 不操作业务输出、不退出进程，也不管理 symbol table。它只负责 tokenization、semantic value、location 和 lexical diagnostics。

### 4.5 Scanner 不变量

- scanner 使用 `yyscan_t`，不依赖全局 `yyin`、`yytext` 或共享状态。
- 使用 `yy_scan_bytes` 读取已经由 source module 持有的 buffer，不要求 seekable FILE。
- lexical error 后记录第一个诊断并让 parser 终止。
- 所有 `ID` 内存的所有权必须转移到 parse context，或在 error path 释放。
- 生成代码不得直接修改。任何行为变化都通过 `lexer.l`、Flex options 或 wrapper 完成。

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

该文法保证：声明只能出现在计算语句之前；计算语句恰好一个；声明和程序都以 `.` 结束。`src/parser.y` 是可执行 grammar 的权威副本，必须保持 project production ID 与本节一致，并以 `%expect 0` 要求零冲突。Production 0 是 Bison 隐式生成的 augmented start rule，`parser.y` 中只写 production 1–33；project ID 不依赖 Bison 内部 rule number。

## 6. GNU Bison 语法分析

### 6.1 Parser 模式

`src/parser.y` 使用 Bison C skeleton，并启用 pure/reentrant parser、locations、详细错误与 LAC：

```bison
%define api.pure full
%define api.prefix {uslc_}
%define parse.error detailed
%define parse.lac full
%define lr.type lalr
%locations
%expect 0
```

parser 接收显式 context 和 scanner：

```bison
%parse-param { UslcParseContext *context }
%parse-param { yyscan_t scanner }
%lex-param   { yyscan_t scanner }
```

`%code requires` 只放生成 header 必需的 forward declarations 和小型 value types；业务实现放在普通 `.c` 文件中。

### 6.2 Semantic values

`%union` 至少支持：

```text
int64_t integer
UslcString identifier
UslcParseNode *node
```

每个 terminal 和 nonterminal 必须声明正确的 `%token`/`%type`。Bison destructor 负责在 syntax error 或 memory exhaustion 时释放尚未转移到 arena/context 的 semantic values：

```bison
%destructor { uslc_string_release(&$$); } <identifier>
```

### 6.3 Grammar actions

每条 reduction action 只执行以下工作：

1. 按 source order 收集 RHS nodes；
2. 创建对应 production id 的 parse-tree nonterminal；
3. 合并 `@$` source span；
4. 将节点赋给 `$$`。

Grammar actions 不执行名字绑定、类型检查、集合求值或 JSON 输出。这样生成器层与编译器语义保持分离。

标点 terminal 仍进入完整 parse tree；AST lowering 阶段再移除纯语法节点。

### 6.4 Generated artifacts

Bison 生成：

```text
build/generated/parser.c
build/generated/parser.h
build/generated/parser.output
```

Flex 生成：

```text
build/generated/lexer.c
build/generated/lexer.h
```

`parser.output` 保留在 build tree，供 review、测试和冲突诊断使用。Build script 检查 Bison exit code，并拒绝任何未经 `%expect` 明确声明的 conflict。

### 6.5 Error handling

`yyerror`/`uslc_error` 把 Bison location、unexpected token 和 expected-token information 转换为 `UslcDiagnostic`。不把 Bison 原始英文消息直接作为稳定 API；稳定 error code 与主要消息由项目 formatter 产生，Bison detail 可以作为 note。

本项目不启用 grammar error recovery production。出现 syntax error 后立即终止 parse，exit code 为 2。

### 6.6 Parse tree

每个节点至少包含：

```text
symbol
production_id       # terminal 为 none
source_span
children[]
token               # 仅 terminal
```

children 保持源程序顺序。parse tree 可以完整输出到 `parser_out.json`。生成 parser 的生命周期由 parse context 管理，`yyscan_t` 始终在 parser 返回后销毁。

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
--compact-json               artifact JSON 不缩进；默认两空格缩进
--help                       打印帮助
--version                    打印版本
```

`--` 结束选项解析，之后可以使用以 `-` 开头的文件名。Parser trace 写入 stderr，成功报告写 stdout，错误报告写 stderr。Bison automaton 与 conflict 信息使用 build 时生成的 `build/generated/parser.output` 审查，不在运行时重新实现 grammar-table dump。

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
- 所有 JSON object、array、string、integer 和 boolean 都必须通过 json-c API 创建；禁止手写 escaping 或自行拼接 JSON syntax。

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
T3001 undefined identifier
T3002 duplicate declaration
T3003 incompatible operand types
T3004 initializer type mismatch
T3005 invalid show value

E4001 integer overflow
E4002 unbound set representative
E4090 evaluator invariant failure

I9001 out of memory
I9003 malformed parse tree
I9004 nesting or tree-depth limit exceeded
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

严格 C23 构建使用 `max_align_t` 作为 arena block 的基础对齐，并通过 `alignof` 验证每种实际节点类型。项目不声明 over-aligned node types。树高及递归处理深度限制为 255，超过限制报告 `I9004`（exit 70），避免宿主栈耗尽；该限制也适用于极长的左结合表达式、声明列表与嵌套符号集合。

可增长 arrays、diagnostics 和 symbol table 使用明确的 `init/free` API。每个 owning struct 的头文件注释必须说明所有权。

### 12.3 禁止事项

- 不返回指向临时栈对象的指针。
- 不依赖 signed overflow、越界指针或未初始化内存。
- 不把 `realloc` 结果直接覆盖原指针。
- 不以 `exit()` 代替库函数错误返回。
- 不在 headers 中定义可变全局对象。

## 13. 计划目录结构

```text
DirectParse/
├── README.md
├── .gitignore
├── .gitattributes
├── include/
│   └── uslc/
│       ├── arena.h
│       ├── ast.h
│       ├── diagnostic.h
│       ├── evaluator.h
│       ├── parse_context.h
│       ├── printer.h
│       ├── serialization.h
│       ├── source.h
│       ├── symbol_table.h
│       └── type_checker.h
├── src/
│   ├── lexer.l
│   ├── parser.y
│   ├── arena.c
│   ├── ast.c
│   ├── diagnostic.c
│   ├── evaluator.c
│   ├── main.c
│   ├── parse_context.c
│   ├── printer.c
│   ├── serialization.c
│   ├── source.c
│   ├── symbol_table.c
│   └── type_checker.c
├── scripts/
│   ├── check-tools.sh
│   ├── build.sh
│   ├── clean.sh
│   └── test.sh
├── examples/
│   ├── arithmetic.set
│   ├── sets.set
│   └── errors.set
├── tests/
│   ├── test_main.c
│   ├── test_scanner.c
│   ├── test_parser.c
│   ├── test_type_checker.c
│   ├── test_evaluator.c
│   ├── test_serialization.c
│   └── fixtures/
└── build/
    ├── generated/            # Flex/Bison output
    ├── debug/
    └── release/
```

所有 generated C/header/report files 都位于 `build/`，不提交到 Git。发布 source archive 必须包含 `.l`、`.y` 和 build scripts，因此用户可从规范源重新生成 parser/scanner。

## 14. C 编码规范

- 使用 C23，公开符号统一以 `uslc_` 前缀命名。
- 类型使用 `UslcName`，函数和变量使用 `uslc_name`，enum constants 使用 `USLC_NAME`。
- 每个 header 有 include guard，并能独立 include。
- 对外优先使用 opaque structs；简单 immutable value structs 可以公开。
- include 顺序：对应 header、C/POSIX headers、Flex/Bison generated headers、json-c headers、项目 headers。
- 不使用 VLAs、`alloca`、非标准 `strdup` 或实现定义的位域布局。
- 使用 `<stdint.h>`、`<stdbool.h>`、`<stddef.h>` 和 `<inttypes.h>`。
- 长度和容量使用 `size_t`；源位置 offset 使用 `size_t`；语言整数使用 `int64_t`。
- 函数返回 status 或 tagged result，不用 sentinel pointer 混淆“未找到”和“内存失败”。
- `const` 表达不可变借用；所有权转移必须在 API 注释中写明。
- 正常用户错误不得触发 assertion。Assertions 只检查开发期内部不变量。
- 人类可读输出经过 printer；JSON 经过 serialization module 与 json-c，不在算法代码中散落格式字符串或 JSON syntax。

## 15. Unix-like 构建规范

### 15.1 支持范围

本版本只支持 Unix-like 系统：

- Linux，包括当前机器的 WSL 2 Ubuntu 24.04；
- FreeBSD、OpenBSD、NetBSD；
- macOS；
- 其他提供 POSIX shell、C23 compiler、Flex、GNU Bison、json-c 和 pkg-config 的 POSIX 环境。

不提供原生 Windows PowerShell、MSVC 或 `.cmd` 构建。当前 Windows 主机上的所有 DirectParse 构建和测试必须从 `Ubuntu-24.04` WSL 2 运行。

### 15.2 依赖与版本

```text
C compiler: Clang 18+ or GCC 14+, with -std=c23
Flex:       2.6.4+
GNU Bison:  3.8.2+
json-c:     0.16+
pkg-config: 0.29+ or pkgconf-compatible
shell:      POSIX sh
```

Flex/Bison 是 build-time dependencies；生成的 scanner/parser 编译进 `uslc`。json-c 是 compile-time 与 runtime dependency。除此之外不引入第三方产品库。

Ubuntu/Debian：

```sh
sudo apt-get update
sudo apt-get install clang flex bison libjson-c-dev pkg-config
```

Fedora：

```sh
sudo dnf install clang flex bison json-c-devel pkgconf-pkg-config
```

FreeBSD：

```sh
sudo pkg install llvm flex bison json-c pkgconf
```

macOS with Homebrew：

```sh
brew install llvm flex bison json-c pkg-config
```

Homebrew 的 Flex/Bison/LLVM 可能是 keg-only。调用 build script 前把相应 `bin` 加入 PATH，或显式设置 `CC`、`FLEX`、`BISON`。

### 15.3 当前 WSL 2 基线

```text
distribution: Ubuntu 24.04.5 LTS (WSL 2)
kernel:       6.18.33.2-microsoft-standard-WSL2
clang:        18.1.3
flex:         2.6.4
bison:        3.8.2
pkg-config:   1.8.1
json-c dev:   0.17
```

当前 `pkg-config` 版本为 1.8.1，并可正常解析 json-c 0.17。所有必需的生成、编译与链接工具均已具备；M0 实现后可直接进行首次完整 build。

当前机器从 PowerShell 调用：

```powershell
wsl.exe -d Ubuntu-24.04 -- bash -lc `
  'cd "/mnt/c/Users/qhdqs/source/workspace/COMP3173 24F Project/DirectParse" && ./scripts/build.sh debug && ./scripts/test.sh debug'
```

该命令在同一份 Windows checkout 上构建，不复制源码。Shell scripts 必须使用 LF line endings；仓库中的 `.gitattributes` 对 `*.sh`、`*.l`、`*.y`、`*.c` 和 `*.h` 强制 LF。

### 15.4 Build pipeline

```text
./scripts/check-tools.sh
./scripts/build.sh debug
./scripts/build.sh release
./scripts/test.sh debug
./scripts/clean.sh
```

`check-tools.sh` 验证命令存在、最低版本、C23 macro 和 `pkg-config --exists json-c`。它不安装软件。

`build.sh` 执行：

1. 定位 repository root，创建 `build/generated` 与 configuration directories；
2. 调用 Bison 生成 parser C/header/state report；
3. 调用 Flex 生成 reentrant scanner C/header；
4. 通过 `pkg-config --cflags --libs json-c` 获取 json-c flags；
5. 编译 handwritten 与 generated C；
6. 链接 `build/<configuration>/uslc`；
7. 打印 compiler、Flex、Bison 与 json-c 版本。

建议 Bison command：

```sh
bison --defines=build/generated/parser.h \
      --output=build/generated/parser.c \
      --report=state,solved \
      --report-file=build/generated/parser.output \
      src/parser.y
```

建议 Flex command：

```sh
flex --header-file=build/generated/lexer.h \
     --outfile=build/generated/lexer.c \
     src/lexer.l
```

Handwritten C flags：

```text
-std=c23 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
```

Generated Flex/Bison C 可以使用较窄的 warning set，避免第三方生成代码触发与项目无关的 warnings；handwritten code 仍使用完整 flags。Debug 增加 `-O0 -g3`，Release 增加 `-O2 -DNDEBUG`。

不链接 `libfl`：scanner 必须使用 `%option noyywrap`。Build script 使用参数列表与正常 shell quoting，不使用 `eval`。

### 15.5 Portability rules

业务模块尽量保持 C23 + standard library。CLI、file permissions 和 shell integration 可以使用 POSIX.1-2008。禁止依赖 glibc-only API、Linux procfs、GNU statement expressions 或 Apple-only frameworks。

路径由用户传入，不能假设 `/mnt/c`。文件名可以含空格。临时和 generated files 只能写入 `build/`。

## 16. 测试策略

### 16.1 测试框架

项目提供极小的自有 C test harness：

```text
USLC_TEST(name)
USLC_EXPECT(condition)
USLC_EXPECT_EQ_INT(expected, actual)
USLC_EXPECT_STREQ(expected, actual)
```

Harness 使用 C standard library；serialization tests 链接 json-c。每个失败打印 test name、文件、行号和差异，进程以非零状态结束。

### 16.2 Scanner tests

测试每种 token、无空白相邻 token、CRLF/LF、keyword/identifier、非法字符、大写字母、前导零、整数溢出、source spans 和 repeated scanner lifecycle。测试必须证明两个独立 `yyscan_t` 实例不共享状态。

### 16.3 Parser tests

测试无声明程序、多声明程序、优先级、左结合、集合、predicate、嵌套括号、缺失 token、额外 token 和提前 EOF。检查：

- `%expect 0` 且 Bison build 无 conflicts；
- `parser.output` 存在并包含预期 states；
- parse tree production ids 与第 5 节一致；
- syntax diagnostics 有稳定 code/location；
- error path 的 `%destructor` 不泄漏 semantic values。

### 16.4 Semantic and evaluator tests

覆盖所有类型签名、operand mismatch、initializer mismatch、undefined/duplicate/self-reference、representative shadowing、checked overflow、短路、集合 union/intersection、alpha-renaming、membership 和 deterministic printing。

### 16.5 json-c serialization tests

必须覆盖：

- object/array nesting 与固定 key insertion order；
- quote、backslash、control characters 和 UTF-8；
- `int64_t`、boolean、null 与 symbolic predicate AST；
- pretty/compact modes；
- json-c allocation/write failure propagation；
- 每个 `json_object` 的 ownership/reference count；
- 生成结果可由 json-c 再次 parse；
- golden JSON fixtures 的结构比较，而不仅是脆弱的空白比较。

项目禁止自行实现 JSON escaping、parser 或通用 serializer。

### 16.6 Integration tests

每个 fixture 包含 `.set`、stdout、stderr、exit code 和可选 artifacts。`scripts/test.sh` 负责 build、运行和比较。可使用 POSIX utilities；JSON 结构验证由链接 json-c 的 test executable 完成，不要求 `jq`。

Debug test 可选启用 AddressSanitizer/UndefinedBehaviorSanitizer。若系统提供 Valgrind，可作为附加检查，但不是基础依赖。

### 16.7 验收矩阵

| Environment | Compiler | Flex/Bison | json-c | Status |
| --- | --- | --- | --- | --- |
| WSL 2 Ubuntu 24.04 | Clang 18+ | required | required | mandatory on current machine |
| Linux native/container | Clang 18+ or GCC 14+ | required | required | recommended |
| FreeBSD | base Clang or LLVM port | ports packages | required | recommended |
| macOS | Apple Clang or Homebrew LLVM | Homebrew | required | recommended |

每次实现阶段至少必须通过 WSL 2 Ubuntu 24.04。发布前应再通过至少一个非-WSL Unix-like 环境。

## 17. 实现里程碑

### M0 Repository foundation

- 建立目录、C test harness、POSIX build/clean/test scripts 与 `.gitattributes`。
- 完成 `check-tools.sh`，验证 Clang C23、Flex、Bison、pkg-config 和 json-c。
- 使用 WSL 2 中已安装的 `libjson-c-dev` 与 `pkg-config` 完成 smoke build。
- 验收：WSL 2 可生成 parser/scanner、链接空 driver 并运行 smoke test。

### M1 Flex scanner

- 完成 `lexer.l`、reentrant wrapper、semantic values、locations 和 lexical diagnostics。
- 完成 scanner unit tests 与 `--phase lex`。
- 验收：合法 token fixtures 稳定；非法输入产生精确位置和 exit code 1；无 global scanner state。

### M2 Bison parser and parse tree

- 把 34 条 productions 编入 `parser.y`。
- 配置 pure parser、LAC、locations、destructors 和 `%expect 0`。
- semantic actions 构造完整 parse tree。
- 验收：Bison 无 conflicts；正确程序 accept；错误程序产生稳定 diagnostics；sanitizer 下 error paths 无 leak。

### M3 AST and type checker

- 完成 AST lowering、symbol table 和 scopes。
- 实现第 8 节全部类型规则。
- 验收：所有合法/非法运算、声明和 representative scope tests 通过。

### M4 Evaluator

- 完成 immutable environment、checked integer arithmetic、boolean 与 symbolic sets。
- 完成 deterministic value printer。
- 验收：课程示例和新增边界测试得到规范结果。

### M5 json-c artifacts and CLI

- 完成 serialization module、CLI、human printer 和所有 artifacts。
- 用 json-c round-trip 检查 JSON。
- 验收：pretty/compact JSON、write failures、reference ownership 和 schemas 全部有测试。

### M6 Unix portability and hardening

- WSL 2 Clang strict C23 必须通过。
- 增加 Linux GCC 14+、FreeBSD 或 macOS 中至少一个验证环境。
- 清理 handwritten warnings，检查 generated-code warning policy。
- 运行 ASan/UBSan 与可用的 Valgrind。
- 验收：最低平台矩阵通过，输出稳定，无已知 leak/UB。

### M7 Optional simplify extension

- 在核心版本稳定后为 `parser.y` 增加 `simplify` command。
- Bison 仍保持 `%expect 0`。
- 实现 NNF、DNF、一次整数约束、区间合并、membership substitution 与 distribution law。
- 高于一次的 representative expression 报告 semantic error。
- 更新 grammar、state report、类型规则、CLI 和 tests。

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
3. 当前机器的 WSL 2 Ubuntu 24.04 构建与测试通过；发布时至少再通过一个 Unix-like 环境。
4. 编译没有项目自身可修复的 warnings。
5. 错误路径释放所有已拥有资源或由 arena 统一释放。
6. 输出、exit code 和 JSON schema 有稳定测试。
7. 外部依赖保持在 Flex、GNU Bison、json-c 与 pkg-config 的明确 allowlist 内，不引入额外产品库。
8. `git status` 中不出现 build products、临时文件或测试输出。

## 20. 当前状态

核心 M0–M5 已实现：reentrant Flex scanner、pure LALR(1) Bison parser、完整 parse tree、AST lowering、静态类型检查、整数与符号集合求值、CLI 和 json-c artifacts。文法包含 project productions 1–33，Bison 隐式提供 production 0；当前 automaton 有 67 个 states，零 conflicts。

M6 的本机验证已通过 WSL 2 Ubuntu 24.04、Clang 18.1.3 的 strict C23 Debug、Release 和 ASan/UBSan 构建。测试包含自有 C harness 与 POSIX CLI integration suite，覆盖所有显式 production、独立 scanner 生命周期、类型与作用域、溢出与短路、JSON round-trip、分配失败和输出失败。`.github/workflows/ci.yml` 提供 Linux 和 macOS 验证任务；这两个非 WSL 环境尚未在本机执行，因此发布前的额外平台验收仍待 CI 完成。M7 `simplify` 是未实现的可选扩展。

### 20.1 构建与运行

在 Unix-like shell 中进入 `DirectParse` 后：

```sh
./scripts/build.sh debug
./build/debug/uslc examples/arithmetic.set
./build/debug/uslc examples/sets.set
printf 'show 1 + 2 * 3 .\n' | ./build/debug/uslc -

mkdir -p build/artifacts
./build/debug/uslc --emit-dir build/artifacts examples/sets.set
./build/debug/uslc --phase parse --trace-parser examples/sets.set

./scripts/test.sh debug
./scripts/test.sh release
SANITIZE=1 ./scripts/test.sh debug
```

`test.sh` 总是先重新构建相应 configuration。`SANITIZE=1` 将该 configuration 的产品与测试程序都编译为 ASan/UBSan 版本；要恢复普通 Debug，运行 `./scripts/build.sh debug`。测试输入、临时输出和生成代码全部位于 `build/`。`CC`、`FLEX`、`BISON` 和 `PKG_CONFIG` 只影响构建脚本，产品 CLI 不读取环境配置。每个变量应是单个可执行文件名或路径，不是带参数的 shell 命令。

当前 Windows 主机可从 PowerShell 执行：

```powershell
wsl.exe -d Ubuntu-24.04 -- sh -lc 'cd "/mnt/c/Users/qhdqs/source/workspace/COMP3173 24F Project/DirectParse" && ./scripts/test.sh debug'
wsl.exe -d Ubuntu-24.04 -- sh -lc 'cd "/mnt/c/Users/qhdqs/source/workspace/COMP3173 24F Project/DirectParse" && ./build/debug/uslc examples/sets.set'
```

### 20.2 实现说明

- Lexical phase 先扫描完整 source；只有整个阶段成功才进入 parser。Parser 使用新的 scanner 实例重放扫描，复用已有 terminal nodes。Token 计数包含 EOF，parse node 计数不包含 EOF；所有 spans 的结束位置均为 exclusive。
- Terminal lexemes 借用 source buffer；parse tree、AST 和 symbolic values 分属三个 arenas。Flex allocations 另由 scanner wrapper 跟踪，使生成器的 allocation-failure 路径也能通过 context 诊断并完整释放。
- 静态绑定为每个全局变量与 representative 分配独立 binding ID。符号集合替换基于 ID，显示时才使用稳定的 `x`、`y`、`z` 等名字；相同源码拼写不会造成捕获。
- 集合谓词保留逻辑结构，折叠可安全计算的常量算术。可能溢出的谓词表达式保持延迟，直到成员测试按短路规则实际访问该表达式时才报告 `E4001`。
- `ast.json` 在类型检查前写入，`typing_out.json` 在类型检查成功后写入。测试通过 json-c 比较 JSON 结构，同时用 golden fixtures 检查稳定文本和退出码。
- 注入分配失败的测试入口只在 `USLC_TESTING` 构建中存在，产品二进制不包含这些测试开关。
