// OurC P1+P2 — 詞法 + 語法 + 語意檢查器
// 依 OurC grammar 2016-05-05 版:遞迴下降 parser + 錯誤三分類(詞法/語法/語意)。
//   P1 詞法 + 語法;P2 符號表 + 作用域,檢查未宣告 / 重複宣告。
// 語意檢查範圍:變數 / 函式使用前需宣告;同一作用域不可重複宣告。
// (求值輸出屬 P3、函式呼叫堆疊屬 P4。)
//
// 用法:  ourc < input.in
// 逐一讀取頂層 (definition | statement),每句印出「接受」或錯誤分類 + 行號;
// 遇錯自動跳到下一個 ';' 後繼續,不中斷整批。
//
// 編譯:  g++ -std=c++17 -O2 -o ourc ourc.cpp

#include <cctype>
#include <iostream>
#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Token
// ---------------------------------------------------------------------------
enum TokType {
    T_IDENT, T_CONST,
    // keywords / type specifiers
    T_INT, T_FLOAT, T_CHAR, T_BOOL, T_STRING, T_VOID,
    T_IF, T_ELSE, T_WHILE, T_DO, T_RETURN,
    // brackets
    T_LPAREN, T_RPAREN, T_LBRACK, T_RBRACK, T_LBRACE, T_RBRACE,
    // single-char operators
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT, T_CARET,
    T_GT, T_LT, T_AMP, T_PIPE, T_ASSIGN, T_NOT,
    T_SEMI, T_COMMA, T_QUESTION, T_COLON,
    // multi-char operators
    T_GE, T_LE, T_EQ, T_NEQ, T_AND, T_OR,
    T_PE, T_ME, T_TE, T_DE, T_RE, T_PP, T_MM, T_RS, T_LS,
    T_EOF
};

struct Token {
    TokType type = T_EOF;
    std::string text;
    int line = 0;
};

// ---------------------------------------------------------------------------
// Errors — 三分類
// ---------------------------------------------------------------------------
struct LexError {
    int line;
    char ch;
};
struct SyntaxError {
    std::string msg;
    Token tok;
    SyntaxError(std::string m, Token t) : msg(std::move(m)), tok(std::move(t)) {}
};
struct SemError {                 // 語意錯誤(未宣告 / 重複宣告)
    std::string msg;
    Token tok;
    SemError(std::string m, Token t) : msg(std::move(m)), tok(std::move(t)) {}
};

// ---------------------------------------------------------------------------
// Lexer — on-demand, one-token lookahead
// ---------------------------------------------------------------------------
class Lexer {
public:
    explicit Lexer(std::string src) : mSrc(std::move(src)) {}

    Token peek() {
        if (!mHas) { mBuf = lexOne(); mHas = true; }
        return mBuf;
    }
    Token next() {
        Token t = peek();
        mHas = false;
        return t;
    }
    bool atEof() { return peek().type == T_EOF; }

    // Error recovery: drop buffered token, skip raw source past next ';' (or EOF).
    void recover() {
        mHas = false;
        while (mI < mSrc.size()) {
            char c = mSrc[mI++];
            if (c == '\n') mLine++;
            if (c == ';') break;
        }
    }

private:
    std::string mSrc;
    size_t mI = 0;
    int mLine = 1;
    Token mBuf;
    bool mHas = false;

    char cur() const { return mI < mSrc.size() ? mSrc[mI] : '\0'; }
    char at(size_t k) const { return (mI + k) < mSrc.size() ? mSrc[mI + k] : '\0'; }

    void skipWsAndComments() {
        for (;;) {
            char c = cur();
            if (c == '\n') { mLine++; mI++; continue; }
            if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v') { mI++; continue; }
            if (c == '/' && at(1) == '/') {           // line comment
                mI += 2;
                while (cur() && cur() != '\n') mI++;
                continue;
            }
            if (c == '/' && at(1) == '*') {           // block comment
                mI += 2;
                while (cur() && !(cur() == '*' && at(1) == '/')) {
                    if (cur() == '\n') mLine++;
                    mI++;
                }
                if (cur()) mI += 2;                   // consume closing */
                continue;
            }
            break;
        }
    }

    Token make(TokType ty, const std::string& tx, int ln) { return Token{ty, tx, ln}; }

    Token lexOne() {
        skipWsAndComments();
        int ln = mLine;
        char c = cur();
        if (c == '\0') return make(T_EOF, "", ln);

        // number: digits [ . digits ]  |  . digits
        if (std::isdigit((unsigned char)c)) {
            std::string s;
            while (std::isdigit((unsigned char)cur())) s += mSrc[mI++];
            if (cur() == '.') {                       // "35." and "35.67" both legal
                s += mSrc[mI++];
                while (std::isdigit((unsigned char)cur())) s += mSrc[mI++];
            }
            return make(T_CONST, s, ln);
        }
        if (c == '.' && std::isdigit((unsigned char)at(1))) {  // ".35"
            std::string s;
            s += mSrc[mI++];
            while (std::isdigit((unsigned char)cur())) s += mSrc[mI++];
            return make(T_CONST, s, ln);
        }

        // identifier / keyword
        if (std::isalpha((unsigned char)c) || c == '_') {
            std::string s;
            while (std::isalnum((unsigned char)cur()) || cur() == '_') s += mSrc[mI++];
            TokType kw = keyword(s);
            return make(kw, s, ln);
        }

        // char literal 'a'
        if (c == '\'') {
            std::string s;
            s += mSrc[mI++];
            while (cur() && cur() != '\'') {
                if (cur() == '\\' && at(1)) { s += mSrc[mI++]; }
                if (cur() == '\n') mLine++;
                s += mSrc[mI++];
            }
            if (cur() != '\'') throw LexError{ln, '\''};   // unterminated
            s += mSrc[mI++];
            return make(T_CONST, s, ln);
        }
        // string literal "..."
        if (c == '"') {
            std::string s;
            s += mSrc[mI++];
            while (cur() && cur() != '"') {
                if (cur() == '\\' && at(1)) { s += mSrc[mI++]; }
                if (cur() == '\n') mLine++;
                s += mSrc[mI++];
            }
            if (cur() != '"') throw LexError{ln, '"'};
            s += mSrc[mI++];
            return make(T_CONST, s, ln);
        }

        // operators / punctuation (maximal munch)
        auto two = [&](char a, char b) { return c == a && at(1) == b; };
        if (two('>', '=')) { mI += 2; return make(T_GE, ">=", ln); }
        if (two('<', '=')) { mI += 2; return make(T_LE, "<=", ln); }
        if (two('=', '=')) { mI += 2; return make(T_EQ, "==", ln); }
        if (two('!', '=')) { mI += 2; return make(T_NEQ, "!=", ln); }
        if (two('&', '&')) { mI += 2; return make(T_AND, "&&", ln); }
        if (two('|', '|')) { mI += 2; return make(T_OR, "||", ln); }
        if (two('+', '=')) { mI += 2; return make(T_PE, "+=", ln); }
        if (two('-', '=')) { mI += 2; return make(T_ME, "-=", ln); }
        if (two('*', '=')) { mI += 2; return make(T_TE, "*=", ln); }
        if (two('/', '=')) { mI += 2; return make(T_DE, "/=", ln); }
        if (two('%', '=')) { mI += 2; return make(T_RE, "%=", ln); }
        if (two('+', '+')) { mI += 2; return make(T_PP, "++", ln); }
        if (two('-', '-')) { mI += 2; return make(T_MM, "--", ln); }
        if (two('>', '>')) { mI += 2; return make(T_RS, ">>", ln); }
        if (two('<', '<')) { mI += 2; return make(T_LS, "<<", ln); }

        mI++;
        switch (c) {
            case '(': return make(T_LPAREN, "(", ln);
            case ')': return make(T_RPAREN, ")", ln);
            case '[': return make(T_LBRACK, "[", ln);
            case ']': return make(T_RBRACK, "]", ln);
            case '{': return make(T_LBRACE, "{", ln);
            case '}': return make(T_RBRACE, "}", ln);
            case '+': return make(T_PLUS, "+", ln);
            case '-': return make(T_MINUS, "-", ln);
            case '*': return make(T_STAR, "*", ln);
            case '/': return make(T_SLASH, "/", ln);
            case '%': return make(T_PERCENT, "%", ln);
            case '^': return make(T_CARET, "^", ln);
            case '>': return make(T_GT, ">", ln);
            case '<': return make(T_LT, "<", ln);
            case '&': return make(T_AMP, "&", ln);
            case '|': return make(T_PIPE, "|", ln);
            case '=': return make(T_ASSIGN, "=", ln);
            case '!': return make(T_NOT, "!", ln);
            case ';': return make(T_SEMI, ";", ln);
            case ',': return make(T_COMMA, ",", ln);
            case '?': return make(T_QUESTION, "?", ln);
            case ':': return make(T_COLON, ":", ln);
        }
        throw LexError{ln, c};   // unrecognized character, e.g. '@'
    }

    static TokType keyword(const std::string& s) {
        if (s == "int") return T_INT;
        if (s == "float") return T_FLOAT;
        if (s == "char") return T_CHAR;
        if (s == "bool") return T_BOOL;
        if (s == "string") return T_STRING;   // 全小寫
        if (s == "void") return T_VOID;
        if (s == "if") return T_IF;
        if (s == "else") return T_ELSE;
        if (s == "while") return T_WHILE;
        if (s == "do") return T_DO;
        if (s == "return") return T_RETURN;
        if (s == "true" || s == "false") return T_CONST;  // bool constants
        return T_IDENT;
    }
};

// ---------------------------------------------------------------------------
// Parser — recursive descent over OurC grammar 2016-05-05
//   附符號表(scope stack)做 P2 語意檢查。
// ---------------------------------------------------------------------------
class Parser {
public:
    explicit Parser(Lexer& lx) : mLx(lx) {
        mScopes.emplace_back();                       // global scope
        for (const char* b : {"Done", "ListVariable", "ListAllVariables",
                              "ListFunction", "ListAllFunctions"})
            mScopes.front().insert(b);                // 內建指令
    }

    // one top-level unit: definition | statement
    void unit() {
        TokType t = mLx.peek().type;
        if (t == T_VOID || isTypeSpec(t)) definition();
        else statement();
    }

    bool atEof() { return mLx.atEof(); }
    void recover() { mLx.recover(); }
    void resetToGlobal() { mScopes.resize(1); }       // 錯誤後把作用域堆疊還原到全域

private:
    Lexer& mLx;
    std::vector<std::set<std::string>> mScopes;        // 作用域堆疊(front = global)

    // --- symbol table ------------------------------------------------------
    void pushScope() { mScopes.emplace_back(); }
    void popScope()  { if (mScopes.size() > 1) mScopes.pop_back(); }

    // RAII:離開作用域(含例外)時自動 pop,保持堆疊平衡
    struct ScopeGuard {
        Parser* p;
        explicit ScopeGuard(Parser* pp) : p(pp) { p->pushScope(); }
        ~ScopeGuard() { p->popScope(); }
        ScopeGuard(const ScopeGuard&) = delete;
        ScopeGuard& operator=(const ScopeGuard&) = delete;
    };

    void declare(const Token& id) {
        if (mScopes.back().count(id.text))
            throw SemError("重複宣告 '" + id.text + "'", id);
        mScopes.back().insert(id.text);
    }
    void useVar(const Token& id) {
        for (auto it = mScopes.rbegin(); it != mScopes.rend(); ++it)
            if (it->count(id.text)) return;
        throw SemError("使用未宣告的 '" + id.text + "'", id);
    }

    // --- token helpers -----------------------------------------------------
    static bool isTypeSpec(TokType t) {
        return t == T_INT || t == T_CHAR || t == T_FLOAT || t == T_STRING || t == T_BOOL;
    }
    static bool isSign(TokType t) { return t == T_PLUS || t == T_MINUS || t == T_NOT; }
    static bool isAssignOp(TokType t) {
        return t == T_ASSIGN || t == T_PE || t == T_ME || t == T_TE || t == T_DE || t == T_RE;
    }
    static bool startsExpr(TokType t) {
        return t == T_IDENT || t == T_PP || t == T_MM || isSign(t) || t == T_CONST || t == T_LPAREN;
    }

    bool accept(TokType ty) {
        if (mLx.peek().type == ty) { mLx.next(); return true; }
        return false;
    }
    Token expect(TokType ty, const char* what) {       // 回傳被消耗的 token
        Token t = mLx.peek();
        if (t.type != ty) throw SyntaxError(std::string("需要 ") + what, t);
        mLx.next();
        return t;
    }

    // --- definitions / declarations ---------------------------------------
    void definition() {
        if (accept(T_VOID)) {
            Token id = expect(T_IDENT, "識別字");
            declare(id);                               // void 函式名
            functionDefWithoutID();
            return;
        }
        typeSpecifier();
        Token id = expect(T_IDENT, "識別字");
        declare(id);                                   // 變數 / 函式名
        functionDefOrDeclarators();
    }

    void typeSpecifier() {
        if (!isTypeSpec(mLx.peek().type)) throw SyntaxError("需要型別", mLx.peek());
        mLx.next();
    }

    void functionDefOrDeclarators() {
        if (mLx.peek().type == T_LPAREN) functionDefWithoutID();
        else restOfDeclarators();
    }

    void restOfDeclarators() {
        if (accept(T_LBRACK)) { expect(T_CONST, "常數"); expect(T_RBRACK, "']'"); }
        while (accept(T_COMMA)) {
            Token id = expect(T_IDENT, "識別字");
            declare(id);                               // 逗號後的每個名字
            if (accept(T_LBRACK)) { expect(T_CONST, "常數"); expect(T_RBRACK, "']'"); }
        }
        expect(T_SEMI, "';'");
    }

    void functionDefWithoutID() {
        ScopeGuard g(this);                            // 參數作用域
        expect(T_LPAREN, "'('");
        if (accept(T_VOID)) {
            // empty param list
        } else if (isTypeSpec(mLx.peek().type)) {
            formalParamList();
        }
        expect(T_RPAREN, "')'");
        compound();
    }

    void formalParamList() {
        auto oneParam = [&]() {
            typeSpecifier();
            accept(T_AMP);                    // optional '&'
            Token id = expect(T_IDENT, "識別字");
            declare(id);                                // 參數名進參數作用域
            if (accept(T_LBRACK)) { expect(T_CONST, "常數"); expect(T_RBRACK, "']'"); }
        };
        oneParam();
        while (accept(T_COMMA)) oneParam();
    }

    void compound() {
        ScopeGuard g(this);                            // 區塊作用域
        expect(T_LBRACE, "'{'");
        while (mLx.peek().type != T_RBRACE && mLx.peek().type != T_EOF) {
            if (isTypeSpec(mLx.peek().type)) declaration();
            else statement();
        }
        expect(T_RBRACE, "'}'");
    }

    void declaration() {
        typeSpecifier();
        Token id = expect(T_IDENT, "識別字");
        declare(id);                                   // 區域變數
        restOfDeclarators();
    }

    // --- statement ---------------------------------------------------------
    void statement() {
        Token t = mLx.peek();
        if (t.type == T_SEMI) { mLx.next(); return; }        // null statement
        if (t.type == T_RETURN) {
            mLx.next();
            if (startsExpr(mLx.peek().type)) expression();
            expect(T_SEMI, "';'");
            return;
        }
        if (t.type == T_LBRACE) { compound(); return; }
        if (t.type == T_IF) {
            mLx.next();
            expect(T_LPAREN, "'('"); expression(); expect(T_RPAREN, "')'");
            statement();
            if (accept(T_ELSE)) statement();
            return;
        }
        if (t.type == T_WHILE) {
            mLx.next();
            expect(T_LPAREN, "'('"); expression(); expect(T_RPAREN, "')'");
            statement();
            return;
        }
        if (t.type == T_DO) {
            mLx.next();
            statement();
            expect(T_WHILE, "'while'");
            expect(T_LPAREN, "'('"); expression(); expect(T_RPAREN, "')'");
            expect(T_SEMI, "';'");
            return;
        }
        if (startsExpr(t.type)) { expression(); expect(T_SEMI, "';'"); return; }
        throw SyntaxError("這裡不能放這個 token", t);
    }

    // --- expression --------------------------------------------------------
    void expression() { basicExpression(); while (accept(T_COMMA)) basicExpression(); }

    void basicExpression() {
        Token t = mLx.peek();
        if (t.type == T_IDENT) { mLx.next(); useVar(t); restOfIdentStarted(); return; }
        if (t.type == T_PP || t.type == T_MM) {          // 前置 ++/-- 只能接 ID[索引]
            mLx.next();
            Token id = expect(T_IDENT, "識別字");
            useVar(id);
            if (accept(T_LBRACK)) { expression(); expect(T_RBRACK, "']'"); }
            romceAndRomloe();
            return;
        }
        if (isSign(t.type)) {                            // sign 可以疊任意多個
            while (isSign(mLx.peek().type)) mLx.next();
            signedUnaryExp();
            romceAndRomloe();
            return;
        }
        if (t.type == T_CONST) { mLx.next(); romceAndRomloe(); return; }
        if (t.type == T_LPAREN) {
            mLx.next(); expression(); expect(T_RPAREN, "')'");
            romceAndRomloe();
            return;
        }
        throw SyntaxError("這裡需要一個運算式", t);
    }

    void restOfIdentStarted() {
        if (accept(T_LPAREN)) {                           // 函式呼叫
            if (startsExpr(mLx.peek().type)) actualParamList();
            expect(T_RPAREN, "')'");
            romceAndRomloe();
            return;
        }
        if (accept(T_LBRACK)) { expression(); expect(T_RBRACK, "']'"); }
        if (isAssignOp(mLx.peek().type)) { mLx.next(); basicExpression(); return; }
        if (mLx.peek().type == T_PP || mLx.peek().type == T_MM) mLx.next();
        romceAndRomloe();
    }

    void actualParamList() { basicExpression(); while (accept(T_COMMA)) basicExpression(); }

    // --- unary --------------------------------------------------------------
    // signed_unary_exp : Identifier [ '(' [apl] ')' | '[' expression ']' ] | Constant | '(' expression ')'
    void signedUnaryExp() {
        Token t = mLx.peek();
        if (t.type == T_IDENT) {
            mLx.next(); useVar(t);
            if (accept(T_LPAREN)) {
                if (startsExpr(mLx.peek().type)) actualParamList();
                expect(T_RPAREN, "')'");
            } else if (accept(T_LBRACK)) {
                expression(); expect(T_RBRACK, "']'");
            }
            return;
        }
        if (t.type == T_CONST) { mLx.next(); return; }
        if (t.type == T_LPAREN) { mLx.next(); expression(); expect(T_RPAREN, "')'"); return; }
        throw SyntaxError("這裡需要一個 unary 運算元", t);
    }

    // unsigned_unary_exp : Identifier [ '(' [apl] ')' | [ '[' expression ']' ] [ PP|MM ] ] | Constant | '(' expression ')'
    void unsignedUnaryExp() {
        Token t = mLx.peek();
        if (t.type == T_IDENT) {
            mLx.next(); useVar(t);
            if (accept(T_LPAREN)) {
                if (startsExpr(mLx.peek().type)) actualParamList();
                expect(T_RPAREN, "')'");
            } else {
                if (accept(T_LBRACK)) { expression(); expect(T_RBRACK, "']'"); }
                if (mLx.peek().type == T_PP || mLx.peek().type == T_MM) mLx.next();
            }
            return;
        }
        if (t.type == T_CONST) { mLx.next(); return; }
        if (t.type == T_LPAREN) { mLx.next(); expression(); expect(T_RPAREN, "')'"); return; }
        throw SyntaxError("這裡需要一個 unary 運算元", t);
    }

    // unary_exp : sign {sign} signed_unary_exp | unsigned_unary_exp | (PP|MM) Identifier ['[' expression ']']
    void unaryExp() {
        Token t = mLx.peek();
        if (isSign(t.type)) {
            while (isSign(mLx.peek().type)) mLx.next();
            signedUnaryExp();
            return;
        }
        if (t.type == T_PP || t.type == T_MM) {
            mLx.next();
            Token id = expect(T_IDENT, "識別字");
            useVar(id);
            if (accept(T_LBRACK)) { expression(); expect(T_RBRACK, "']'"); }
            return;
        }
        unsignedUnaryExp();
    }

    // --- precedence cascade (romce_and_romloe) ----------------------------
    // "rest_of_*" continue an operand already consumed by the caller;
    // "maybe_*" parse a complete operand from scratch.
    void romceAndRomloe() {
        restOfLogicalOr();
        if (accept(T_QUESTION)) { basicExpression(); expect(T_COLON, "':'"); basicExpression(); }
    }

    void restOfLogicalOr()  { restOfLogicalAnd(); while (accept(T_OR))  maybeLogicalAnd(); }
    void maybeLogicalAnd()  { maybeBitOr();       while (accept(T_AND)) maybeBitOr(); }
    void restOfLogicalAnd() { restOfBitOr();      while (accept(T_AND)) maybeBitOr(); }
    void maybeBitOr()       { maybeBitXor();      while (accept(T_PIPE)) maybeBitXor(); }
    void restOfBitOr()      { restOfBitXor();     while (accept(T_PIPE)) maybeBitXor(); }
    void maybeBitXor()      { maybeBitAnd();      while (accept(T_CARET)) maybeBitAnd(); }
    void restOfBitXor()     { restOfBitAnd();     while (accept(T_CARET)) maybeBitAnd(); }
    void maybeBitAnd()      { maybeEquality();    while (accept(T_AMP)) maybeEquality(); }
    void restOfBitAnd()     { restOfEquality();   while (accept(T_AMP)) maybeEquality(); }

    void maybeEquality() {
        maybeRelational();
        while (mLx.peek().type == T_EQ || mLx.peek().type == T_NEQ) { mLx.next(); maybeRelational(); }
    }
    void restOfEquality() {
        restOfRelational();
        while (mLx.peek().type == T_EQ || mLx.peek().type == T_NEQ) { mLx.next(); maybeRelational(); }
    }

    bool isRel(TokType t) { return t == T_LT || t == T_GT || t == T_LE || t == T_GE; }
    void maybeRelational() {
        maybeShift();
        while (isRel(mLx.peek().type)) { mLx.next(); maybeShift(); }
    }
    void restOfRelational() {
        restOfShift();
        while (isRel(mLx.peek().type)) { mLx.next(); maybeShift(); }
    }

    void maybeShift() {
        maybeAdditive();
        while (mLx.peek().type == T_LS || mLx.peek().type == T_RS) { mLx.next(); maybeAdditive(); }
    }
    void restOfShift() {
        restOfAdditive();
        while (mLx.peek().type == T_LS || mLx.peek().type == T_RS) { mLx.next(); maybeAdditive(); }
    }

    void maybeAdditive() {
        maybeMult();
        while (mLx.peek().type == T_PLUS || mLx.peek().type == T_MINUS) { mLx.next(); maybeMult(); }
    }
    void restOfAdditive() {
        restOfMult();
        while (mLx.peek().type == T_PLUS || mLx.peek().type == T_MINUS) { mLx.next(); maybeMult(); }
    }

    void maybeMult() { unaryExp(); restOfMult(); }
    void restOfMult() {
        while (mLx.peek().type == T_STAR || mLx.peek().type == T_SLASH || mLx.peek().type == T_PERCENT) {
            mLx.next();
            unaryExp();
        }
    }
};

// ---------------------------------------------------------------------------
// Driver
// ---------------------------------------------------------------------------
int main() {
    std::string src((std::istreambuf_iterator<char>(std::cin)),
                    std::istreambuf_iterator<char>());
    Lexer lx(src);
    Parser p(lx);

    int n = 0, ok = 0, lexErr = 0, synErr = 0, semErr = 0;
    for (;;) {
        // Guarded EOF check: a bad char at a unit boundary must be reported,
        // not crash the loop condition. The offending char is already consumed
        // by the lexer, so no skip-to-';' recovery is needed here.
        try {
            if (p.atEof()) break;
        } catch (const LexError& e) {
            n++;
            std::cout << "第 " << n << " 句: 詞法錯誤 (第 " << e.line
                      << " 行 無法辨識的字元 '" << e.ch << "')\n";
            lexErr++;
            continue;
        }
        n++;
        try {
            p.unit();
            std::cout << "第 " << n << " 句: 接受\n";
            ok++;
        } catch (const LexError& e) {
            // bad char mid-unit: already consumed, continue without skipping ';'
            std::cout << "第 " << n << " 句: 詞法錯誤 (第 " << e.line
                      << " 行 無法辨識的字元 '" << e.ch << "')\n";
            lexErr++;
            p.resetToGlobal();
        } catch (const SyntaxError& e) {
            std::cout << "第 " << n << " 句: 語法錯誤 (第 " << e.tok.line
                      << " 行 " << e.msg << ", 出現 '"
                      << (e.tok.type == T_EOF ? std::string("<EOF>") : e.tok.text) << "')\n";
            synErr++;
            p.recover();
            p.resetToGlobal();
        } catch (const SemError& e) {
            std::cout << "第 " << n << " 句: 語意錯誤 (第 " << e.tok.line
                      << " 行 " << e.msg << ")\n";
            semErr++;
            p.recover();
            p.resetToGlobal();
        }
    }
    std::cout << "----\n總計 " << n << " 句:接受 " << ok
              << "、詞法錯 " << lexErr << "、語法錯 " << synErr
              << "、語意錯 " << semErr << "\n";
    return 0;
}
