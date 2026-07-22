# DO_PL — 程式語言課程專案

中原大學資工「程式語言(PL)」課程專案空間。核心作業是自訂的簡化 C 語言
**OurC** 直譯器,分 Project 1~4 漸進實作。

本 repo 目前包含兩部分:

| 目錄 / 檔案 | 內容 | 性質 |
| :--- | :--- | :--- |
| `ourc/` | **OurC 直譯器(自實作)** — 完成 Project 1~3(詞法 / 語法 / 語意 / 求值) | 本人作品 |
| `main.cpp` | OurScheme(Scheme/Lisp 方言直譯器,3,433 行) | 課程參考範例,非本作業本體 |

---

## OurC 直譯器(`ourc/`)— 本專案主體

OurC 是課程自訂的簡化版 C 語言。作業要求用 C++ 從零寫一個直譯器:逐句讀入
OurC 程式,做詞法分析、語法檢查、語意檢查與求值,並對錯誤分類回報行號。

`ourc/ourc.cpp` 為單一自足檔:Lexer + 遞迴下降 Parser(建 AST)+ 符號表 + 樹走訪求值器。

### Project 1:詞法 + 語法(已完成)

- **Lexer**:~50 種 token,含多字元運算子(`++ -- += -= *= /= %= == != >= <= && || >> <<`)、
  數字常數(`35`、`35.67`、`.35`、`35.`)、字元/字串常數、`//` 與 `/* */` 註解。
- **Parser**:依 OurC grammar 2016-05-05 版遞迴下降,建成 AST;完整運算子優先權(含 `?:`)。

### Project 2:語意檢查(已完成)

- 符號表以作用域堆疊(global / 函式參數 / 區塊)管理宣告,RAII 自動維持平衡。
- **未宣告使用**、**同作用域重複宣告** → 語意錯誤。
- 內建 `Done`、`ListVariable`、`ListAllVariables`、`ListFunction`、`ListAllFunctions`。

### Project 3:求值 + 控制流程(已完成)

樹走訪求值器,實際計算並輸出:

- **值型別**:int / float / bool / char / string;int↔float 依運算元自動提升。
- **運算式**:全部算術 / 關係 / 位元 / 移位運算、指派(`= += -= *= /= %=`)、
  前後置 `++ / --`、條件運算 `?:`、短路 `&& / ||`。
- **控制流程**:`if / else`、`while`、`do-while` 實際分支與迴圈。
- **陣列**:宣告、索引讀寫、越界檢查。
- **執行期錯誤**:除以零、模除以零、陣列索引越界。

**錯誤四分類**:詞法、語法、語意、執行期,皆附行號;遇錯自動復原續讀。

### 輸出模型

- 頂層運算式 → 印 `=> 值`;宣告 / 控制流程 / 函式定義 → 印 `接受`。
- `Done()` 結束批次;`ListAllVariables()` 列出目前變數與值。

```bash
cd ourc
g++ -std=c++17 -O2 -o ourc ourc.cpp     # 或:make ourc-test
./ourc < tests/p3_eval.in               # 求值示範
./ourc < tests/p1_err.in                # 詞法 / 語法錯誤
./ourc < tests/p2_sem.in                # 語意錯誤
```

`tests/p3_eval.in` 節錄(真實輸出):

```text
第 2 句: => 11          a = 3 + 4 * 2;
第 12 句: => 10         while 累加 0..4
第 14 句: => 100        if / else 分支
第 20 句: => 6          do-while 階乘 3!
第 30 句: => 30         arr[0] + arr[1]
第 32 句: => 1.5        3.0 / 2
第 35 句: 執行期錯誤 (除以零)      1 / 0;
第 36 句: 執行期錯誤 (陣列索引越界)  arr[5];
```

### OurC 特殊規則(已在 parser 中處理)

- `a++b` 是**錯的**:`++` 優先斷詞成 `PP`,要寫 `a+ +b` 才合法。
- sign(`+ - !`)與 `++ / --` 不可同時修飾同一個 ID(如 `-a++` 判為語法錯)。
- `.35`、`35.` 皆為合法常數;`string` 是內建型別(全小寫)。

### Roadmap

- [x] **P1** 詞法 + 語法(遞迴下降 → AST)
- [x] **P2** 符號表 + 語意檢查(未宣告、重複宣告)
- [x] **P3** 求值 + 控制流程(運算、if/while/do、陣列、執行期錯誤)
- [ ] **P4** 使用者函式呼叫堆疊(參數綁定、回傳值、遞迴)

> 目前呼叫使用者自訂函式會回報「將於 P4 實作」;函式定義本身已可解析與登錄。

同題目的公開參考實作:[HouHou0925/C-plus-interpreter](https://github.com/HouHou0925/C-plus-interpreter)(做到 P3)。

---

## 參考範例:OurScheme(`main.cpp`)

`main.cpp` 是另一支 **Scheme(Lisp 方言)直譯器**,單檔 3,433 行,涵蓋詞法分析、
遞迴下降解析、樹狀求值、內建原語與錯誤處理的完整 REPL。此檔為課程參考範例,
保留於 repo 供對照學習,**非 OurC 作業本體**。

```bash
make            # g++ -std=c++11 -O2 -o ourscheme main.cpp
./ourscheme     # 進入 Scheme REPL
```

課程原始說明保留於 [docs/COURSE_NOTES.md](docs/COURSE_NOTES.md)。
