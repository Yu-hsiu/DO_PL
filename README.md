# OurScheme 編譯器(DO_PL)

此程式為中原大學三下PL(程式語言)project 占分70% 分為 project1~4


用 C++ 從零實作的 Scheme(Lisp 方言)直譯器——中原大學資工「程式語言」課程專案。
單檔 3,433 行,涵蓋詞法分析、遞迴下降解析、樹狀求值、50+ 個內建原語與 25+ 種錯誤處理的完整 REPL。

## 這個專案在做什麼?

讀入一段 Scheme 程式(S-expression)→ 拆成 token → 建成語法樹 → 遞迴求值 → 印出結果,
不斷循環直到 `(exit)`。整個語言的「讀、懂、算、答」四件事,都在這一支程式裡自己實作。


Project1為 基礎的運算能力及變數宣告
\\ Project2為 C語言文法的check
\\ Project3為 2的進階 除了確認文法的正確性還要加上運算能力及輸出答案
\\ Porject4為 3的進階 要加入Call function的能力

## 快速開始

```bash
make            # 等同:g++ -std=c++11 -O2 -o ourscheme main.cpp
./ourscheme     # 進入 REPL
make test       # 跑 examples/demo.scm 範例輸入
```

實際執行(真實輸出):

```text
Welcome to OurScheme!
> (+ 1 2)
3
> (cons 1 (cons 2 nil))
( 1
  2
)
> (define x 10)
x defined
> (* x x)
100
> (if (> x 5) 'big 'small)
big
> (exit)
Thanks for using OurScheme!
```

## 架構:四個階段

| 階段 | 核心函式 | 職責 |
| :--- | :--- | :--- |
| Scanner(詞法) | `ReadToken()` | 逐字元讀入,判別 50+ 種 token 型別(數字、字串、符號、括號…) |
| Parser(語法) | `ReadSExp()` | 遞迴下降,把 token 流組成語法樹 |
| Evaluator(求值) | `EvalSExp()` | 遞迴走訪語法樹,分派到內建原語或使用者函式 |
| Printer(輸出) | `PrintSExp()` | 遞迴列印,含縮排與巢狀列表格式 |

核心資料結構:`Token` 節點以 `mPtrList` 指標串成樹;`map<string, TokenPtr>` 全域綁定表;
`union OurNumber` 讓整數與浮點共用儲存;`OurException` 搭配 try-catch 傳遞 25+ 種錯誤。

## 支援的語言特性

- 資料型別:整數、浮點、字串、符號、布林(`#t` / `nil`)、cons pair 與列表
- 特殊形式:`define`、`lambda`、`let`、`if`、`cond`、`and` / `or` / `not`、`begin`、`quote`
- 一級函式:函式可存入變數、當參數傳遞;支援使用者自訂函式與 `eval` 動態求值
- 錯誤處理:未定義符號、參數數量錯誤、除以零、括號不平衡等 25+ 種錯誤分類,錯誤後 REPL 繼續運作

## 這個專案證明了哪些能力

| 技能 | 程式落點 |
| :--- | :--- |
| 指標與動態記憶體 | 語法樹節點以 `new` 配置,`mPtrList` 指標串連;整棵樹靠指標走訪 |
| 遞迴 | `ReadSExp` / `EvalSExp` / `PrintSExp` 三大遞迴引擎,深度隨巢狀結構成長 |
| struct / class 封裝 | `Token`、`Scanner`、`OurFunction`、`OurException` 各司其職 |
| STL 容器 | `map` 實作變數綁定表(O(log n) 查詢)、`vector` 存參數列 |
| union | `OurNumber` 讓整數與浮點共用同一塊記憶體 |
| 例外處理 | 自訂例外類別 + try-catch,錯誤跨越多層遞迴正確上拋 |
| 字元/字串處理 | 詞法分析逐字元判讀,含字串跳脫序列(`\n`、`\"` 等) |
| 流程控制與函式拆分 | 50+ 個原語函式,switch/if 分派 |

## 設計取捨與已知限制

- **單一 main.cpp**:課程繳交系統限制單檔上傳;模組化拆分列於 Roadmap
- **語法樹不主動釋放**:REPL 每輪求值後樹交由行程結束回收——在互動式直譯器屬可接受取捨,但列入改進項
- 數值以 `==` 直接比較浮點,極端精度情境未涵蓋

## Roadmap

- [ ] 依四階段拆分模組(scanner / parser / evaluator / printer)
- [ ] 原語單元測試(邊界案例:空列表、深度巢狀、錯誤輸入)
- [ ] 記憶體釋放審查(智慧指標或引用計數)

## 其他文件

課程原始說明與開發心法保留於 [docs/COURSE_NOTES.md](docs/COURSE_NOTES.md)。
