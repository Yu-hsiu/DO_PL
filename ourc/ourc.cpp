// OurC P1~P3 — 詞法 + 語法 + 語意檢查 + 求值直譯器
// 依 OurC grammar 2016-05-05 版。
//   P1 Lexer + 遞迴下降 Parser(建 AST)
//   P2 符號表 / 作用域:未宣告、重複宣告(語意錯誤)
//   P3 樹走訪求值:運算式、指派、++/--、?:、短路 && ||、if/while/do、陣列;執行期錯誤(除零/陣列界)
// (P4 使用者函式呼叫堆疊尚未實作——呼叫自訂函式會回報執行期錯誤。)
//
// 輸出:頂層運算式印 "=> 值";宣告 / 控制流程 / 函式定義印 "接受";錯誤附行號與分類。
// 內建:Done() 結束;ListAllVariables() 列出變數;ListVariable("x") 列單一變數。
//
// 編譯:  g++ -std=c++17 -O2 -o ourc ourc.cpp
// 用法:  ourc < input.in

#include <cctype>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

// ===========================================================================
// Token
// ===========================================================================
enum TokType {
    T_IDENT, T_CONST,
    T_INT, T_FLOAT, T_CHAR, T_BOOL, T_STRING, T_VOID,
    T_IF, T_ELSE, T_WHILE, T_DO, T_RETURN,
    T_LPAREN, T_RPAREN, T_LBRACK, T_RBRACK, T_LBRACE, T_RBRACE,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT, T_CARET,
    T_GT, T_LT, T_AMP, T_PIPE, T_ASSIGN, T_NOT,
    T_SEMI, T_COMMA, T_QUESTION, T_COLON,
    T_GE, T_LE, T_EQ, T_NEQ, T_AND, T_OR,
    T_PE, T_ME, T_TE, T_DE, T_RE, T_PP, T_MM, T_RS, T_LS,
    T_EOF
};

struct Token {
    TokType type = T_EOF;
    std::string text;
    int line = 0;
};

// ===========================================================================
// Errors
// ===========================================================================
struct LexError   { int line; char ch; };
struct SyntaxError{ std::string msg; Token tok; SyntaxError(std::string m, Token t):msg(std::move(m)),tok(std::move(t)){} };
struct SemError   { std::string msg; Token tok; SemError(std::string m, Token t):msg(std::move(m)),tok(std::move(t)){} };
struct RunError   { std::string msg; int line; RunError(std::string m,int l):msg(std::move(m)),line(l){} };
struct StopSignal {};   // Done()
// ReturnSignal 定義於 Value 之後(需帶回傳值)

// ===========================================================================
// Lexer — on-demand, one-token lookahead
// ===========================================================================
class Lexer {
public:
    explicit Lexer(std::string src) : mSrc(std::move(src)) {}
    Token peek() { if (!mHas) { mBuf = lexOne(); mHas = true; } return mBuf; }
    Token next() { Token t = peek(); mHas = false; return t; }
    bool atEof() { return peek().type == T_EOF; }
    void recover() {                       // skip raw source past next ';'
        mHas = false;
        while (mI < mSrc.size()) { char c = mSrc[mI++]; if (c=='\n') mLine++; if (c==';') break; }
    }
private:
    std::string mSrc; size_t mI = 0; int mLine = 1; Token mBuf; bool mHas = false;
    char cur() const { return mI < mSrc.size() ? mSrc[mI] : '\0'; }
    char at(size_t k) const { return (mI+k) < mSrc.size() ? mSrc[mI+k] : '\0'; }

    void skipWsAndComments() {
        for (;;) {
            char c = cur();
            if (c=='\n') { mLine++; mI++; continue; }
            if (c==' '||c=='\t'||c=='\r'||c=='\f'||c=='\v') { mI++; continue; }
            if (c=='/' && at(1)=='/') { mI+=2; while (cur() && cur()!='\n') mI++; continue; }
            if (c=='/' && at(1)=='*') { mI+=2; while (cur() && !(cur()=='*'&&at(1)=='/')) { if (cur()=='\n') mLine++; mI++; } if (cur()) mI+=2; continue; }
            break;
        }
    }
    Token make(TokType ty, const std::string& tx, int ln) { return Token{ty, tx, ln}; }

    Token lexOne() {
        skipWsAndComments();
        int ln = mLine; char c = cur();
        if (c=='\0') return make(T_EOF, "", ln);

        if (std::isdigit((unsigned char)c)) {
            std::string s;
            while (std::isdigit((unsigned char)cur())) s += mSrc[mI++];
            if (cur()=='.') { s += mSrc[mI++]; while (std::isdigit((unsigned char)cur())) s += mSrc[mI++]; }
            return make(T_CONST, s, ln);
        }
        if (c=='.' && std::isdigit((unsigned char)at(1))) {
            std::string s; s += mSrc[mI++];
            while (std::isdigit((unsigned char)cur())) s += mSrc[mI++];
            return make(T_CONST, s, ln);
        }
        if (std::isalpha((unsigned char)c) || c=='_') {
            std::string s;
            while (std::isalnum((unsigned char)cur()) || cur()=='_') s += mSrc[mI++];
            return make(keyword(s), s, ln);
        }
        if (c=='\'') {
            std::string s; s += mSrc[mI++];
            while (cur() && cur()!='\'') { if (cur()=='\\'&&at(1)) s += mSrc[mI++]; if (cur()=='\n') mLine++; s += mSrc[mI++]; }
            if (cur()!='\'') throw LexError{ln,'\''};
            s += mSrc[mI++]; return make(T_CONST, s, ln);
        }
        if (c=='"') {
            std::string s; s += mSrc[mI++];
            while (cur() && cur()!='"') { if (cur()=='\\'&&at(1)) s += mSrc[mI++]; if (cur()=='\n') mLine++; s += mSrc[mI++]; }
            if (cur()!='"') throw LexError{ln,'"'};
            s += mSrc[mI++]; return make(T_CONST, s, ln);
        }
        auto two = [&](char a, char b){ return c==a && at(1)==b; };
        if (two('>','=')) { mI+=2; return make(T_GE,">=",ln); }
        if (two('<','=')) { mI+=2; return make(T_LE,"<=",ln); }
        if (two('=','=')) { mI+=2; return make(T_EQ,"==",ln); }
        if (two('!','=')) { mI+=2; return make(T_NEQ,"!=",ln); }
        if (two('&','&')) { mI+=2; return make(T_AND,"&&",ln); }
        if (two('|','|')) { mI+=2; return make(T_OR,"||",ln); }
        if (two('+','=')) { mI+=2; return make(T_PE,"+=",ln); }
        if (two('-','=')) { mI+=2; return make(T_ME,"-=",ln); }
        if (two('*','=')) { mI+=2; return make(T_TE,"*=",ln); }
        if (two('/','=')) { mI+=2; return make(T_DE,"/=",ln); }
        if (two('%','=')) { mI+=2; return make(T_RE,"%=",ln); }
        if (two('+','+')) { mI+=2; return make(T_PP,"++",ln); }
        if (two('-','-')) { mI+=2; return make(T_MM,"--",ln); }
        if (two('>','>')) { mI+=2; return make(T_RS,">>",ln); }
        if (two('<','<')) { mI+=2; return make(T_LS,"<<",ln); }
        mI++;
        switch (c) {
            case '(': return make(T_LPAREN,"(",ln);  case ')': return make(T_RPAREN,")",ln);
            case '[': return make(T_LBRACK,"[",ln);  case ']': return make(T_RBRACK,"]",ln);
            case '{': return make(T_LBRACE,"{",ln);  case '}': return make(T_RBRACE,"}",ln);
            case '+': return make(T_PLUS,"+",ln);    case '-': return make(T_MINUS,"-",ln);
            case '*': return make(T_STAR,"*",ln);    case '/': return make(T_SLASH,"/",ln);
            case '%': return make(T_PERCENT,"%",ln); case '^': return make(T_CARET,"^",ln);
            case '>': return make(T_GT,">",ln);      case '<': return make(T_LT,"<",ln);
            case '&': return make(T_AMP,"&",ln);     case '|': return make(T_PIPE,"|",ln);
            case '=': return make(T_ASSIGN,"=",ln);  case '!': return make(T_NOT,"!",ln);
            case ';': return make(T_SEMI,";",ln);    case ',': return make(T_COMMA,",",ln);
            case '?': return make(T_QUESTION,"?",ln);case ':': return make(T_COLON,":",ln);
        }
        throw LexError{ln, c};
    }
    static TokType keyword(const std::string& s) {
        if (s=="int") return T_INT;    if (s=="float") return T_FLOAT; if (s=="char") return T_CHAR;
        if (s=="bool") return T_BOOL;  if (s=="string") return T_STRING; if (s=="void") return T_VOID;
        if (s=="if") return T_IF;      if (s=="else") return T_ELSE;   if (s=="while") return T_WHILE;
        if (s=="do") return T_DO;      if (s=="return") return T_RETURN;
        if (s=="true"||s=="false") return T_CONST;
        return T_IDENT;
    }
};

// ===========================================================================
// AST
// ===========================================================================
struct Node;
using NP = std::unique_ptr<Node>;

struct Node {
    enum K {
        // expr
        NUM, VAR, INDEX, ASSIGN, BINARY, UNARY, PREINC, POSTINC, TERNARY, CALL, COMMA,
        // stmt
        EXPRSTMT, NULLSTMT, RET, IF, WHILE, DOWHILE, BLOCK, DECL, FUNCDEF
    } k;
    Token tok;                                   // for line info + literal/name/op text
    TokType op = T_EOF;                           // BINARY/UNARY/ASSIGN/PRE/POSTINC
    std::string name;                             // VAR/INDEX/CALL name; DECL type name; FUNCDEF name
    std::vector<NP> kids;                         // operands / args / block stmts
    std::vector<std::pair<std::string, NP>> declist;  // DECL: (name, sizeExpr|null)
    std::vector<std::string> params;              // FUNCDEF param names
    bool floatType = false;                       // DECL float

    explicit Node(K kind) : k(kind) {}
};
static NP mk(Node::K k) { return std::make_unique<Node>(k); }

// ===========================================================================
// Parser — recursive descent building an AST
// ===========================================================================
class Parser {
public:
    explicit Parser(Lexer& lx) : mLx(lx) {}
    bool atEof() { return mLx.atEof(); }
    void recover() { mLx.recover(); }

    NP unit() {                                   // definition | statement
        TokType t = mLx.peek().type;
        if (t==T_VOID || isTypeSpec(t)) return definition();
        return statement();
    }
private:
    Lexer& mLx;

    static bool isTypeSpec(TokType t){ return t==T_INT||t==T_CHAR||t==T_FLOAT||t==T_STRING||t==T_BOOL; }
    static bool isSign(TokType t){ return t==T_PLUS||t==T_MINUS||t==T_NOT; }
    static bool isAssignOp(TokType t){ return t==T_ASSIGN||t==T_PE||t==T_ME||t==T_TE||t==T_DE||t==T_RE; }
    static bool startsExpr(TokType t){ return t==T_IDENT||t==T_PP||t==T_MM||isSign(t)||t==T_CONST||t==T_LPAREN; }

    bool accept(TokType ty){ if (mLx.peek().type==ty){ mLx.next(); return true; } return false; }
    Token expect(TokType ty, const char* what){
        Token t = mLx.peek();
        if (t.type!=ty) throw SyntaxError(std::string("需要 ")+what, t);
        mLx.next(); return t;
    }

    // --- definitions / declarations ---
    NP definition() {
        bool isVoid = accept(T_VOID);
        bool ft = (mLx.peek().type==T_FLOAT);
        if (!isVoid) typeSpecifier();
        Token id = expect(T_IDENT, "識別字");
        if (mLx.peek().type==T_LPAREN) {           // function definition
            auto fn = mk(Node::FUNCDEF); fn->tok=id; fn->name=id.text;
            parseFuncTail(*fn);
            return fn;
        }
        // variable declaration(s): first name already read
        auto d = mk(Node::DECL); d->tok=id; d->floatType=ft;
        NP sz; if (accept(T_LBRACK)) { sz = mk(Node::NUM); Token c=expect(T_CONST,"常數"); sz->tok=c; expect(T_RBRACK,"']'"); }
        d->declist.emplace_back(id.text, std::move(sz));
        while (accept(T_COMMA)) {
            Token n = expect(T_IDENT,"識別字"); NP s2;
            if (accept(T_LBRACK)) { s2 = mk(Node::NUM); Token c=expect(T_CONST,"常數"); s2->tok=c; expect(T_RBRACK,"']'"); }
            d->declist.emplace_back(n.text, std::move(s2));
        }
        expect(T_SEMI,"';'");
        return d;
    }
    void typeSpecifier(){ if (!isTypeSpec(mLx.peek().type)) throw SyntaxError("需要型別", mLx.peek()); mLx.next(); }

    void parseFuncTail(Node& fn) {                 // '(' [VOID|params] ')' compound
        expect(T_LPAREN,"'('");
        if (accept(T_VOID)) {}
        else if (isTypeSpec(mLx.peek().type)) {
            auto oneParam=[&](){
                typeSpecifier(); accept(T_AMP);
                Token id=expect(T_IDENT,"識別字"); fn.params.push_back(id.text);
                if (accept(T_LBRACK)) { expect(T_CONST,"常數"); expect(T_RBRACK,"']'"); }
            };
            oneParam(); while (accept(T_COMMA)) oneParam();
        }
        expect(T_RPAREN,"')'");
        fn.kids.push_back(compound());
    }

    NP compound() {                                // '{' { declaration | statement } '}'
        auto b = mk(Node::BLOCK); b->tok = mLx.peek();
        expect(T_LBRACE,"'{'");
        while (mLx.peek().type!=T_RBRACE && mLx.peek().type!=T_EOF) {
            if (isTypeSpec(mLx.peek().type)) b->kids.push_back(localDeclaration());
            else b->kids.push_back(statement());
        }
        expect(T_RBRACE,"'}'");
        return b;
    }
    NP localDeclaration() {                         // type Identifier rest_of_declarators
        bool ft = (mLx.peek().type==T_FLOAT);
        typeSpecifier();
        auto d = mk(Node::DECL); d->tok = mLx.peek(); d->floatType=ft;
        Token id = expect(T_IDENT,"識別字"); NP sz;
        if (accept(T_LBRACK)) { sz=mk(Node::NUM); Token c=expect(T_CONST,"常數"); sz->tok=c; expect(T_RBRACK,"']'"); }
        d->declist.emplace_back(id.text, std::move(sz));
        while (accept(T_COMMA)) {
            Token n=expect(T_IDENT,"識別字"); NP s2;
            if (accept(T_LBRACK)) { s2=mk(Node::NUM); Token c=expect(T_CONST,"常數"); s2->tok=c; expect(T_RBRACK,"']'"); }
            d->declist.emplace_back(n.text, std::move(s2));
        }
        expect(T_SEMI,"';'");
        return d;
    }

    // --- statements ---
    NP statement() {
        Token t = mLx.peek();
        if (t.type==T_SEMI) { mLx.next(); auto n=mk(Node::NULLSTMT); n->tok=t; return n; }
        if (t.type==T_RETURN) {
            mLx.next(); auto n=mk(Node::RET); n->tok=t;
            if (startsExpr(mLx.peek().type)) n->kids.push_back(expression());
            expect(T_SEMI,"';'"); return n;
        }
        if (t.type==T_LBRACE) return compound();
        if (t.type==T_IF) {
            mLx.next(); auto n=mk(Node::IF); n->tok=t;
            expect(T_LPAREN,"'('"); n->kids.push_back(expression()); expect(T_RPAREN,"')'");
            n->kids.push_back(statement());
            if (accept(T_ELSE)) n->kids.push_back(statement());
            return n;
        }
        if (t.type==T_WHILE) {
            mLx.next(); auto n=mk(Node::WHILE); n->tok=t;
            expect(T_LPAREN,"'('"); n->kids.push_back(expression()); expect(T_RPAREN,"')'");
            n->kids.push_back(statement()); return n;
        }
        if (t.type==T_DO) {
            mLx.next(); auto n=mk(Node::DOWHILE); n->tok=t;
            n->kids.push_back(statement());
            expect(T_WHILE,"'while'"); expect(T_LPAREN,"'('");
            n->kids.push_back(expression()); expect(T_RPAREN,"')'"); expect(T_SEMI,"';'");
            return n;
        }
        if (startsExpr(t.type)) {
            auto n=mk(Node::EXPRSTMT); n->tok=t;
            n->kids.push_back(expression()); expect(T_SEMI,"';'"); return n;
        }
        throw SyntaxError("這裡不能放這個 token", t);
    }

    // --- expressions (build AST) ---
    NP expression() {                               // basic { ',' basic } → COMMA (value = last)
        NP first = basicExpression();
        if (mLx.peek().type!=T_COMMA) return first;
        auto c=mk(Node::COMMA); c->tok=mLx.peek(); c->kids.push_back(std::move(first));
        while (accept(T_COMMA)) c->kids.push_back(basicExpression());
        return c;
    }

    // basic_expression: assignment | (leading unary) romce_and_romloe
    NP basicExpression() {
        Token t = mLx.peek();
        if (t.type==T_IDENT) {
            mLx.next();
            NP base = mk(Node::VAR); base->tok=t; base->name=t.text;
            if (mLx.peek().type==T_LPAREN) {         // call
                base = mk(Node::CALL); base->tok=t; base->name=t.text;
                mLx.next();
                if (startsExpr(mLx.peek().type)) { base->kids.push_back(basicExpression()); while (accept(T_COMMA)) base->kids.push_back(basicExpression()); }
                expect(T_RPAREN,"')'");
                return romce(std::move(base));
            }
            if (accept(T_LBRACK)) {                  // index
                auto idx=mk(Node::INDEX); idx->tok=t; idx->name=t.text;
                idx->kids.push_back(expression()); expect(T_RBRACK,"']'");
                base = std::move(idx);
            }
            if (isAssignOp(mLx.peek().type)) {        // assignment (lowest prec, no romce)
                Token opTok=mLx.next();
                auto a=mk(Node::ASSIGN); a->tok=opTok; a->op=opTok.type;
                a->kids.push_back(std::move(base));
                a->kids.push_back(basicExpression());
                return a;
            }
            if (mLx.peek().type==T_PP || mLx.peek().type==T_MM) {   // postfix
                Token pp=mLx.next(); auto po=mk(Node::POSTINC); po->tok=pp; po->op=pp.type;
                po->kids.push_back(std::move(base)); base=std::move(po);
            }
            return romce(std::move(base));
        }
        if (t.type==T_PP || t.type==T_MM) {           // prefix ++/-- Identifier[idx]
            mLx.next();
            Token id=expect(T_IDENT,"識別字");
            NP target=mk(Node::VAR); target->tok=id; target->name=id.text;
            if (accept(T_LBRACK)) { auto ix=mk(Node::INDEX); ix->tok=id; ix->name=id.text; ix->kids.push_back(expression()); expect(T_RBRACK,"']'"); target=std::move(ix); }
            auto pre=mk(Node::PREINC); pre->tok=t; pre->op=t.type; pre->kids.push_back(std::move(target));
            return romce(std::move(pre));
        }
        if (isSign(t.type)) {                         // sign {sign} signed_unary
            std::vector<Token> signs;
            while (isSign(mLx.peek().type)) signs.push_back(mLx.next());
            NP operand = signedUnary();
            for (auto it=signs.rbegin(); it!=signs.rend(); ++it) {
                auto u=mk(Node::UNARY); u->tok=*it; u->op=it->type; u->kids.push_back(std::move(operand)); operand=std::move(u);
            }
            return romce(std::move(operand));
        }
        if (t.type==T_CONST) { mLx.next(); auto n=mk(Node::NUM); n->tok=t; return romce(std::move(n)); }
        if (t.type==T_LPAREN) { mLx.next(); NP e=expression(); expect(T_RPAREN,"')'"); return romce(std::move(e)); }
        throw SyntaxError("這裡需要一個運算式", t);
    }

    // signed_unary_exp: Identifier [ '(' apl ')' | '[' expr ']' ] | Constant | '(' expr ')'
    NP signedUnary() {
        Token t=mLx.peek();
        if (t.type==T_IDENT) {
            mLx.next();
            if (mLx.peek().type==T_LPAREN) {
                auto c=mk(Node::CALL); c->tok=t; c->name=t.text; mLx.next();
                if (startsExpr(mLx.peek().type)) { c->kids.push_back(basicExpression()); while (accept(T_COMMA)) c->kids.push_back(basicExpression()); }
                expect(T_RPAREN,"')'"); return c;
            }
            if (accept(T_LBRACK)) { auto ix=mk(Node::INDEX); ix->tok=t; ix->name=t.text; ix->kids.push_back(expression()); expect(T_RBRACK,"']'"); return ix; }
            auto v=mk(Node::VAR); v->tok=t; v->name=t.text; return v;
        }
        if (t.type==T_CONST) { mLx.next(); auto n=mk(Node::NUM); n->tok=t; return n; }
        if (t.type==T_LPAREN) { mLx.next(); NP e=expression(); expect(T_RPAREN,"')'"); return e; }
        throw SyntaxError("這裡需要一個 unary 運算元", t);
    }

    // unsigned_unary_exp: Identifier [ '(' apl ')' | [ '[' expr ']' ] [ PP|MM ] ] | Constant | '(' expr ')'
    NP unsignedUnary() {
        Token t=mLx.peek();
        if (t.type==T_IDENT) {
            mLx.next();
            if (mLx.peek().type==T_LPAREN) {
                auto c=mk(Node::CALL); c->tok=t; c->name=t.text; mLx.next();
                if (startsExpr(mLx.peek().type)) { c->kids.push_back(basicExpression()); while (accept(T_COMMA)) c->kids.push_back(basicExpression()); }
                expect(T_RPAREN,"')'"); return c;
            }
            NP base=mk(Node::VAR); base->tok=t; base->name=t.text;
            if (accept(T_LBRACK)) { auto ix=mk(Node::INDEX); ix->tok=t; ix->name=t.text; ix->kids.push_back(expression()); expect(T_RBRACK,"']'"); base=std::move(ix); }
            if (mLx.peek().type==T_PP||mLx.peek().type==T_MM) { Token pp=mLx.next(); auto po=mk(Node::POSTINC); po->tok=pp; po->op=pp.type; po->kids.push_back(std::move(base)); base=std::move(po); }
            return base;
        }
        if (t.type==T_CONST) { mLx.next(); auto n=mk(Node::NUM); n->tok=t; return n; }
        if (t.type==T_LPAREN) { mLx.next(); NP e=expression(); expect(T_RPAREN,"')'"); return e; }
        throw SyntaxError("這裡需要一個 unary 運算元", t);
    }

    // unary_exp: sign{sign} signed | (PP|MM) Id[idx] | unsigned
    NP unaryOperand() {
        Token t=mLx.peek();
        if (isSign(t.type)) {
            std::vector<Token> signs;
            while (isSign(mLx.peek().type)) signs.push_back(mLx.next());
            NP operand=signedUnary();
            for (auto it=signs.rbegin(); it!=signs.rend(); ++it) { auto u=mk(Node::UNARY); u->tok=*it; u->op=it->type; u->kids.push_back(std::move(operand)); operand=std::move(u); }
            return operand;
        }
        if (t.type==T_PP||t.type==T_MM) {
            mLx.next(); Token id=expect(T_IDENT,"識別字");
            NP target=mk(Node::VAR); target->tok=id; target->name=id.text;
            if (accept(T_LBRACK)) { auto ix=mk(Node::INDEX); ix->tok=id; ix->name=id.text; ix->kids.push_back(expression()); expect(T_RBRACK,"']'"); target=std::move(ix); }
            auto pre=mk(Node::PREINC); pre->tok=t; pre->op=t.type; pre->kids.push_back(std::move(target)); return pre;
        }
        return unsignedUnary();
    }

    // romce_and_romloe: precedence-climb binary ops over `leading`, then optional ?:
    static int binPrec(TokType t) {
        switch (t) {
            case T_STAR: case T_SLASH: case T_PERCENT: return 10;
            case T_PLUS: case T_MINUS: return 9;
            case T_LS: case T_RS: return 8;
            case T_LT: case T_GT: case T_LE: case T_GE: return 7;
            case T_EQ: case T_NEQ: return 6;
            case T_AMP: return 5;
            case T_CARET: return 4;
            case T_PIPE: return 3;
            case T_AND: return 2;
            case T_OR: return 1;
            default: return -1;
        }
    }
    NP romce(NP leading) {
        NP node = binaryRHS(0, std::move(leading));
        if (mLx.peek().type==T_QUESTION) {
            Token q=mLx.next();
            NP th=basicExpression(); expect(T_COLON,"':'"); NP el=basicExpression();
            auto tn=mk(Node::TERNARY); tn->tok=q;
            tn->kids.push_back(std::move(node)); tn->kids.push_back(std::move(th)); tn->kids.push_back(std::move(el));
            node=std::move(tn);
        }
        return node;
    }
    NP binaryRHS(int minPrec, NP left) {
        for (;;) {
            TokType op=mLx.peek().type; int p=binPrec(op);
            if (p < minPrec || p < 0) break;
            Token opTok=mLx.next();
            NP right=unaryOperand();
            for (;;) { int np=binPrec(mLx.peek().type); if (np>p) right=binaryRHS(np, std::move(right)); else break; }
            auto b=mk(Node::BINARY); b->tok=opTok; b->op=op;
            b->kids.push_back(std::move(left)); b->kids.push_back(std::move(right));
            left=std::move(b);
        }
        return left;
    }
};

// ===========================================================================
// Values + runtime environment
// ===========================================================================
struct Value {
    enum K { VOID, INT, FLOAT, BOOL, CHAR, STR } k = VOID;
    long long i = 0; double f = 0; std::string s;
    double num() const { return k==FLOAT ? f : (double)i; }
    bool truth() const { return k==FLOAT ? f!=0 : (k==STR ? !s.empty() : i!=0); }
    bool isFloat() const { return k==FLOAT; }
    static Value Int(long long v){ Value r; r.k=INT; r.i=v; return r; }
    static Value Flt(double v){ Value r; r.k=FLOAT; r.f=v; return r; }
    static Value Bool(bool v){ Value r; r.k=BOOL; r.i=v?1:0; return r; }
};

struct Var {
    bool isArray = false; bool isFloat = false;
    Value scalar; std::vector<Value> arr;
};

struct ReturnSignal { Value v; };   // P4:return 帶回傳值,由 callUser 攔截

static std::string fmt(const Value& v) {
    std::ostringstream o;
    switch (v.k) {
        case Value::INT:  o << v.i; break;
        case Value::BOOL: o << (v.i ? "true" : "false"); break;
        case Value::CHAR: o << (char)v.i; break;
        case Value::STR:  o << v.s; break;
        case Value::FLOAT:{ o.precision(15); o << v.f; break; }
        default: o << "(void)"; break;
    }
    return o.str();
}

// ===========================================================================
// Evaluator — tree walk
// ===========================================================================
class Evaluator {
public:
    Evaluator() { mScopes.emplace_back(); }              // global scope
    void resetToGlobal() { mScopes.resize(1); }

    // returns the value to display for a top-level unit, or VOID for silent ones
    Value execTop(Node& n) {
        if (n.k==Node::EXPRSTMT) return eval(*n.kids[0]);   // display its value
        exec(n);                                            // decl / control / funcdef → silent
        return Value{};                                     // VOID
    }
private:
    std::vector<std::map<std::string, Var>> mScopes;
    std::map<std::string, Node*> mFuncs;                    // user functions (P4 executes; P3 stores)

    struct ScopeGuard { Evaluator* e; explicit ScopeGuard(Evaluator* p):e(p){ e->mScopes.emplace_back(); } ~ScopeGuard(){ e->mScopes.pop_back(); } };

    Var* findVar(const std::string& n) {
        for (auto it=mScopes.rbegin(); it!=mScopes.rend(); ++it) { auto f=it->find(n); if (f!=it->end()) return &f->second; }
        return nullptr;
    }

    // ---- statements ----
    void exec(Node& n) {
        switch (n.k) {
            case Node::NULLSTMT: return;
            case Node::EXPRSTMT: eval(*n.kids[0]); return;
            case Node::RET: { Value rv; if (!n.kids.empty()) rv=eval(*n.kids[0]); throw ReturnSignal{rv}; }
            case Node::BLOCK: { ScopeGuard g(this); for (auto& s : n.kids) exec(*s); return; }
            case Node::DECL: execDecl(n); return;
            case Node::FUNCDEF: {
                if (mScopes.back().count(n.name)) throw SemError("重複宣告 '"+n.name+"'", n.tok);
                if (mFuncs.count(n.name)) throw SemError("重複宣告 '"+n.name+"'", n.tok);
                mFuncs[n.name]=&n; return;
            }
            case Node::IF: {
                if (eval(*n.kids[0]).truth()) exec(*n.kids[1]);
                else if (n.kids.size()>2) exec(*n.kids[2]);
                return;
            }
            case Node::WHILE: while (eval(*n.kids[0]).truth()) exec(*n.kids[1]); return;
            case Node::DOWHILE: do { exec(*n.kids[0]); } while (eval(*n.kids[1]).truth()); return;
            default: eval(n); return;                        // expression used as statement
        }
    }
    void execDecl(Node& n) {
        for (auto& d : n.declist) {
            const std::string& nm=d.first;
            if (mScopes.back().count(nm) || mFuncs.count(nm)) throw SemError("重複宣告 '"+nm+"'", n.tok);
            Var v; v.isFloat=n.floatType;
            if (d.second) {                                  // array
                Value szv=eval(*d.second);
                long long sz=(long long)szv.num();
                if (sz<0) throw RunError("陣列大小為負", n.tok.line);
                v.isArray=true; v.arr.assign((size_t)sz, n.floatType?Value::Flt(0):Value::Int(0));
            } else {
                v.scalar = n.floatType?Value::Flt(0):Value::Int(0);
            }
            mScopes.back()[nm]=std::move(v);
        }
    }

    // ---- lvalue resolution ----
    Value* lvalue(Node& n) {
        if (n.k==Node::VAR) {
            Var* v=findVar(n.name); if (!v) throw SemError("使用未宣告的 '"+n.name+"'", n.tok);
            if (v->isArray) throw RunError("陣列名稱不能當純量使用: "+n.name, n.tok.line);
            return &v->scalar;
        }
        if (n.k==Node::INDEX) {
            Var* v=findVar(n.name); if (!v) throw SemError("使用未宣告的 '"+n.name+"'", n.tok);
            if (!v->isArray) throw RunError(n.name+" 不是陣列", n.tok.line);
            long long idx=(long long)eval(*n.kids[0]).num();
            if (idx<0 || (size_t)idx>=v->arr.size()) throw RunError("陣列索引越界: "+n.name+"["+std::to_string(idx)+"]", n.tok.line);
            return &v->arr[(size_t)idx];
        }
        throw RunError("不是可指派的左值", n.tok.line);
    }

    static Value applyArith(TokType op, const Value& a, const Value& b, int line) {
        bool fl = a.isFloat() || b.isFloat();
        switch (op) {
            case T_PLUS:  return fl?Value::Flt(a.num()+b.num()):Value::Int(a.i+b.i);
            case T_MINUS: return fl?Value::Flt(a.num()-b.num()):Value::Int(a.i-b.i);
            case T_STAR:  return fl?Value::Flt(a.num()*b.num()):Value::Int(a.i*b.i);
            case T_SLASH:
                if (fl) { if (b.num()==0) throw RunError("除以零", line); return Value::Flt(a.num()/b.num()); }
                if (b.i==0) throw RunError("除以零", line); return Value::Int(a.i/b.i);
            case T_PERCENT: if ((long long)b.num()==0) throw RunError("模除以零", line); return Value::Int((long long)a.num() % (long long)b.num());
            case T_AMP:   return Value::Int((long long)a.num() & (long long)b.num());
            case T_PIPE:  return Value::Int((long long)a.num() | (long long)b.num());
            case T_CARET: return Value::Int((long long)a.num() ^ (long long)b.num());
            case T_LS:    return Value::Int((long long)a.num() << (long long)b.num());
            case T_RS:    return Value::Int((long long)a.num() >> (long long)b.num());
            case T_LT:    return Value::Bool(a.num() <  b.num());
            case T_GT:    return Value::Bool(a.num() >  b.num());
            case T_LE:    return Value::Bool(a.num() <= b.num());
            case T_GE:    return Value::Bool(a.num() >= b.num());
            case T_EQ:    return Value::Bool(a.num() == b.num());
            case T_NEQ:   return Value::Bool(a.num() != b.num());
            default: throw RunError("未支援的二元運算子", line);
        }
    }

    // ---- expressions ----
    Value eval(Node& n) {
        switch (n.k) {
            case Node::NUM: return literal(n.tok);
            case Node::VAR: {
                Var* v=findVar(n.name); if (!v) throw SemError("使用未宣告的 '"+n.name+"'", n.tok);
                if (v->isArray) throw RunError("陣列名稱不能當純量使用: "+n.name, n.tok.line);
                return v->scalar;
            }
            case Node::INDEX: return *lvalue(n);
            case Node::COMMA: { Value r; for (auto& k : n.kids) r=eval(*k); return r; }
            case Node::UNARY: {
                Value o=eval(*n.kids[0]);
                if (n.op==T_PLUS)  return o.isFloat()?Value::Flt(+o.f):Value::Int(+o.i);
                if (n.op==T_MINUS) return o.isFloat()?Value::Flt(-o.f):Value::Int(-o.i);
                if (n.op==T_NOT)   return Value::Bool(!o.truth());
                throw RunError("未支援的一元運算子", n.tok.line);
            }
            case Node::PREINC: { Value* s=lvalue(*n.kids[0]); step(*s, n.op==T_PP?+1:-1); return *s; }
            case Node::POSTINC:{ Value* s=lvalue(*n.kids[0]); Value old=*s; step(*s, n.op==T_PP?+1:-1); return old; }
            case Node::ASSIGN: {
                Value* s=lvalue(*n.kids[0]);
                Value rhs=eval(*n.kids[1]);
                if (n.op==T_ASSIGN) *s=coerce(*s, rhs);
                else { TokType a = n.op==T_PE?T_PLUS:n.op==T_ME?T_MINUS:n.op==T_TE?T_STAR:n.op==T_DE?T_SLASH:T_PERCENT;
                       *s=coerce(*s, applyArith(a, *s, rhs, n.tok.line)); }
                return *s;
            }
            case Node::BINARY: {
                if (n.op==T_AND) { if (!eval(*n.kids[0]).truth()) return Value::Bool(false); return Value::Bool(eval(*n.kids[1]).truth()); }
                if (n.op==T_OR)  { if ( eval(*n.kids[0]).truth()) return Value::Bool(true);  return Value::Bool(eval(*n.kids[1]).truth()); }
                Value a=eval(*n.kids[0]), b=eval(*n.kids[1]);
                if ((n.op==T_EQ||n.op==T_NEQ) && (a.k==Value::STR||b.k==Value::STR)) {
                    bool eq=(a.k==Value::STR&&b.k==Value::STR&&a.s==b.s);
                    return Value::Bool(n.op==T_EQ?eq:!eq);
                }
                return applyArith(n.op, a, b, n.tok.line);
            }
            case Node::TERNARY: return eval(*n.kids[0]).truth() ? eval(*n.kids[1]) : eval(*n.kids[2]);
            case Node::CALL: return call(n);
            default: throw RunError("無法求值的節點", n.tok.line);
        }
    }

    static Value coerce(const Value& slot, const Value& rhs) {
        // keep the variable's numeric flavour: float slot stays float
        if (slot.k==Value::FLOAT) return Value::Flt(rhs.num());
        if (rhs.k==Value::FLOAT)  return Value::Int((long long)rhs.f);  // int slot truncates
        return rhs;
    }
    static void step(Value& s, int delta) {
        if (s.k==Value::FLOAT) s.f += delta; else s.i += delta;
    }
    static Value literal(const Token& t) {
        const std::string& x=t.text;
        if (!x.empty() && x[0]=='\'') { Value v; v.k=Value::CHAR; v.i = x.size()>=3 ? (unsigned char)x[1] : 0; if (x.size()>=4 && x[1]=='\\'){ char c=x[2]; v.i = c=='n'?'\n':c=='t'?'\t':c=='0'?'\0':c; } return v; }
        if (!x.empty() && x[0]=='"') { Value v; v.k=Value::STR; v.s = x.substr(1, x.size()>=2?x.size()-2:0); return v; }
        if (x=="true") return Value::Bool(true);
        if (x=="false") return Value::Bool(false);
        if (x.find('.')!=std::string::npos) return Value::Flt(std::stod(x));
        return Value::Int(std::stoll(x));
    }

    Value call(Node& n) {
        const std::string& fn=n.name;
        if (fn=="Done") throw StopSignal{};
        if (fn=="ListAllVariables") { listAll(); return Value{}; }
        if (fn=="ListVariable") {
            if (!n.kids.empty()) { Value a=eval(*n.kids[0]); listOne(a.k==Value::STR?a.s:fmt(a)); }
            return Value{};
        }
        if (fn=="ListFunction" || fn=="ListAllFunctions") { for (auto& f:mFuncs) std::cout << "函式 " << f.first << "\n"; return Value{}; }
        if (mFuncs.count(fn)) return callUser(*mFuncs[fn], n);
        throw SemError("使用未宣告的 '"+fn+"'", n.tok);
    }

    // P4:使用者函式呼叫。C 語意——函式框架只見全域 + 自身參數/區域,看不到呼叫端區域變數。
    Value callUser(Node& fn, Node& callNode) {
        std::vector<Value> av;                             // 先在呼叫端作用域算好引數
        for (auto& a : callNode.kids) av.push_back(eval(*a));
        if (av.size()!=fn.params.size())
            throw RunError("函式 '"+fn.name+"' 參數數量不符(需 "+std::to_string(fn.params.size())+" 個,給了 "+std::to_string(av.size())+" 個)", callNode.tok.line);
        // ponytail: 傳值呼叫;grammar 的 '&' by-reference 尚未實作(P4 最小版)
        std::vector<std::map<std::string,Var>> saved = std::move(mScopes);
        mScopes.clear();
        mScopes.push_back(std::move(saved[0]));            // 共享全域(變動保留)
        mScopes.emplace_back();                           // 參數作用域
        for (size_t k=0; k<fn.params.size(); ++k) { Var v; v.scalar=av[k]; mScopes.back()[fn.params[k]]=std::move(v); }
        Value ret;                                         // 無 return → VOID
        try { exec(*fn.kids[0]); }                         // 函式本體(BLOCK)
        catch (const ReturnSignal& r) { ret=r.v; }
        saved[0]=std::move(mScopes[0]);                    // 還原全域(保留本次呼叫對全域的變動)
        mScopes=std::move(saved);
        return ret;
    }
    void listAll() {
        for (auto it=mScopes.rbegin(); it!=mScopes.rend(); ++it)
            for (auto& kv : *it) {
                if (kv.second.isArray) std::cout << "變數 " << kv.first << "[" << kv.second.arr.size() << "]\n";
                else std::cout << "變數 " << kv.first << " = " << fmt(kv.second.scalar) << "\n";
            }
    }
    void listOne(const std::string& nm) {
        Var* v=findVar(nm);
        if (!v) std::cout << nm << " 未宣告\n";
        else if (v->isArray) std::cout << "變數 " << nm << "[" << v->arr.size() << "]\n";
        else std::cout << "變數 " << nm << " = " << fmt(v->scalar) << "\n";
    }
};

// ===========================================================================
// Driver
// ===========================================================================
int main() {
    std::string src((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    Lexer lx(src);
    Parser p(lx);
    Evaluator ev;

    int n=0, ok=0, lexErr=0, synErr=0, semErr=0, runErr=0;
    std::vector<NP> keep;                        // 保住所有 AST 節點:函式定義存進 mFuncs,需活到整批結束
    for (;;) {
        try { if (p.atEof()) break; }
        catch (const LexError& e) { n++; std::cout << "第 " << n << " 句: 詞法錯誤 (第 " << e.line << " 行 無法辨識的字元 '" << e.ch << "')\n"; lexErr++; continue; }
        n++;
        try {
            NP node = p.unit();                 // parse (lexical/syntax errors here)
            Node* np = node.get();
            keep.push_back(std::move(node));    // 保留擁有權(FUNCDEF 指標要一直有效)
            Value r = ev.execTop(*np);          // evaluate (semantic/runtime errors here)
            if (r.k==Value::VOID) std::cout << "第 " << n << " 句: 接受\n";
            else std::cout << "第 " << n << " 句: => " << fmt(r) << "\n";
            ok++;
        } catch (const StopSignal&) {
            std::cout << "第 " << n << " 句: Done() — 結束\n"; ok++; break;
        } catch (const ReturnSignal&) {          // 頂層 return:視為接受(靜默)
            std::cout << "第 " << n << " 句: 接受\n"; ok++;
        } catch (const LexError& e) {
            std::cout << "第 " << n << " 句: 詞法錯誤 (第 " << e.line << " 行 無法辨識的字元 '" << e.ch << "')\n"; lexErr++;
        } catch (const SyntaxError& e) {
            std::cout << "第 " << n << " 句: 語法錯誤 (第 " << e.tok.line << " 行 " << e.msg << ", 出現 '" << (e.tok.type==T_EOF?std::string("<EOF>"):e.tok.text) << "')\n"; synErr++; p.recover();
        } catch (const SemError& e) {
            // 語意錯誤發生在求值階段,parse 已消耗完整句子 → 不可再 recover(會吃掉下一句)
            std::cout << "第 " << n << " 句: 語意錯誤 (第 " << e.tok.line << " 行 " << e.msg << ")\n"; semErr++; ev.resetToGlobal();
        } catch (const RunError& e) {
            std::cout << "第 " << n << " 句: 執行期錯誤 (第 " << e.line << " 行 " << e.msg << ")\n"; runErr++; ev.resetToGlobal();
        }
    }
    std::cout << "----\n總計 " << n << " 句:成功 " << ok
              << "、詞法錯 " << lexErr << "、語法錯 " << synErr
              << "、語意錯 " << semErr << "、執行期錯 " << runErr << "\n";
    return 0;
}
