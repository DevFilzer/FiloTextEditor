/*** includes ***/
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/
#define FILO_VERSION "1.0.1"
#define FILO_TAB_STOP 8
#define FILO_QUIT_TIMES 3
#define FILO_QUERY_LEN 256

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    BACKSPACE   = 127,
    ARROW_LEFT  = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

enum editorHighlight {
    HL_NORMAL    = 0,
    HL_NONPRINT,
    HL_COMMENT,
    HL_MLCOMMENT,
    HL_KEYWORD1,
    HL_KEYWORD2,
    HL_STRING,
    HL_NUMBER,
    HL_MATCH
};

#define HL_HIGHLIGHT_NUMBERS (1<<0)
#define HL_HIGHLIGHT_STRINGS (1<<1)

/*** data ***/
struct editorSyntax {
    char *filetype;
    char **filematch;
    char **keywords;
    char *singleline_comment_start;
    char *multiline_comment_start;
    char *multiline_comment_end;
    int flags;
};

typedef struct erow {
    int idx;
    int size;
    int rsize;
    char *chars;
    char *render;
    unsigned char *hl;
    int hl_open_comment;
} erow;

struct editorConfig {
    int cx, cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty;
    int rawmode;
    int paste_mode;
    int last_key;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct termios orig_termios;
    struct timeval last_char_time;
};

struct editorConfig E;

/*** filetypes ***/

char *C_HL_extensions[] = { ".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", NULL };
char *C_HL_keywords[] = {
    "auto","break","case","continue","default","do","else","enum",
    "extern","for","goto","if","register","return","sizeof","static",
    "struct","switch","typedef","union","volatile","while","NULL",
    "alignas","alignof","and","and_eq","asm","bitand","bitor","class",
    "compl","constexpr","const_cast","delete","dynamic_cast",
    "explicit","export","false","friend","inline","mutable","namespace",
    "new","noexcept","not","not_eq","nullptr","operator","or","or_eq",
    "private","protected","public","reinterpret_cast","static_assert",
    "static_cast","template","this","thread_local","throw","true","try",
    "typeid","typename","virtual","xor","xor_eq",
    "int|","long|","double|","float|","char|","unsigned|","signed|",
    "void|","short|","const|","bool|", NULL
};

char *PYTHON_HL_extensions[] = { ".py", ".pyw", ".pyi", ".pyx", NULL };
char *PYTHON_HL_keywords[] = {
    "and","as","assert","break","class","continue","def","del",
    "elif","else","except","exec","finally","for","from","global",
    "if","import","in","is","lambda","not","or","pass","print",
    "raise","return","try","while","with","yield","async","await",
    "nonlocal","True","False","None",
    "abs|","all|","any|","bin|","bool|","bytearray|","bytes|","callable|",
    "chr|","classmethod|","compile|","complex|","delattr|","dict|","dir|",
    "divmod|","enumerate|","eval|","exec|","filter|","float|","format|",
    "frozenset|","getattr|","globals|","hasattr|","hash|","help|","hex|",
    "id|","input|","int|","isinstance|","issubclass|","iter|","len|",
    "list|","locals|","map|","max|","memoryview|","min|","next|","object|",
    "oct|","open|","ord|","pow|","property|","range|","repr|","reversed|",
    "round|","set|","setattr|","slice|","sorted|","staticmethod|","str|",
    "sum|","super|","tuple|","type|","vars|","zip|","self|","cls|", NULL
};

char *JAVA_HL_extensions[] = { ".java", NULL };
char *JAVA_HL_keywords[] = {
    "abstract","assert","break","case","catch","class","const","continue",
    "default","do","else","enum","extends","final","finally","for","goto",
    "if","implements","import","instanceof","interface","native","new","package",
    "private","protected","public","return","static","strictfp","super",
    "switch","synchronized","this","throw","throws","transient","try",
    "volatile","while","true","false","null",
    "boolean|","byte|","char|","double|","float|","int|","long|","short|","void|",
    "String|","Object|","Class|","System|","Thread|","Exception|",
    "RuntimeException|","ArrayList|","HashMap|","List|","Map|","Set|",
    "Collection|","Iterator|", NULL
};

char *JS_HL_extensions[] = { ".js", ".mjs", ".cjs", NULL };
char *JS_HL_keywords[] = {
    "break","case","catch","class","const","continue","debugger","default",
    "delete","do","else","export","extends","finally","for","function",
    "if","import","in","instanceof","let","new","return","super","switch",
    "this","throw","try","typeof","var","void","while","with","yield",
    "async","await","of","true","false","null","undefined",
    "Array|","Object|","String|","Number|","Boolean|","Date|","Math|",
    "RegExp|","Error|","JSON|","console|","Promise|","Map|","Set|",
    "parseInt|","parseFloat|","isNaN|","isFinite|","Symbol|", NULL
};

char *TS_HL_extensions[] = { ".ts", NULL };
char *TS_HL_keywords[] = {
    "break","case","catch","class","const","continue","debugger","default",
    "delete","do","else","export","extends","finally","for","function",
    "if","import","in","instanceof","let","new","return","super","switch",
    "this","throw","try","typeof","var","void","while","with","yield",
    "async","await","of","true","false","null","undefined",
    "interface","type","enum","namespace","module","declare","abstract",
    "implements","private","protected","public","readonly","static",
    "get","set","as","keyof","infer","is","asserts",
    "string|","number|","boolean|","object|","any|","unknown|","never|",
    "void|","bigint|","symbol|","Array|","Promise|","Record|","Partial|",
    "Required|","Pick|","Omit|", NULL
};

char *CSHARP_HL_extensions[] = { ".cs", NULL };
char *CSHARP_HL_keywords[] = {
    "abstract","as","base","break","case","catch","checked","class","const",
    "continue","default","delegate","do","else","enum","event","explicit",
    "extern","false","finally","fixed","for","foreach","goto","if","implicit",
    "in","interface","internal","is","lock","namespace","new","null",
    "operator","out","override","params","private","protected","public",
    "readonly","ref","return","sealed","sizeof","stackalloc","static",
    "struct","switch","this","throw","true","try","typeof","unchecked",
    "unsafe","using","virtual","volatile","while","async","await","var",
    "dynamic","yield","where","when","nameof",
    "bool|","byte|","char|","decimal|","double|","float|","int|","long|",
    "object|","sbyte|","short|","string|","uint|","ulong|","ushort|","void|",
    "String|","Object|","List|","Dictionary|","Array|","Task|","Exception|", NULL
};

char *PHP_HL_extensions[] = { ".php", ".phtml", NULL };
char *PHP_HL_keywords[] = {
    "abstract","and","array","as","break","callable","case","catch","class",
    "clone","const","continue","declare","default","die","do","echo","else",
    "elseif","empty","enddeclare","endfor","endforeach","endif","endswitch",
    "endwhile","eval","exit","extends","final","finally","for","foreach",
    "function","global","goto","if","implements","include","include_once",
    "instanceof","insteadof","interface","isset","list","namespace","new",
    "or","print","private","protected","public","require","require_once",
    "return","static","switch","throw","trait","try","unset","use","var",
    "while","xor","yield","true","false","null",
    "$_GET|","$_POST|","$_SESSION|","$_COOKIE|","$_SERVER|","strlen|",
    "substr|","strpos|","explode|","implode|","count|","isset|","empty|",
    "echo|","print|","var_dump|", NULL
};

char *RUBY_HL_extensions[] = { ".rb", ".rake", NULL };
char *RUBY_HL_keywords[] = {
    "alias","and","begin","break","case","class","def","defined","do",
    "else","elsif","end","ensure","false","for","if","in","module",
    "next","nil","not","or","redo","rescue","retry","return","self",
    "super","then","true","undef","unless","until","when","while","yield",
    "require","include","extend","attr_reader","attr_writer","attr_accessor",
    "puts|","print|","p|","gets|","chomp|","length|","size|",
    "each|","map|","select|","reject|","find|","inject|","reduce|",
    "Array|","Hash|","String|","Integer|","Float|","Symbol|", NULL
};

char *SWIFT_HL_extensions[] = { ".swift", NULL };
char *SWIFT_HL_keywords[] = {
    "associatedtype","class","deinit","enum","extension","fileprivate","func",
    "import","init","inout","internal","let","open","operator","private",
    "protocol","public","static","struct","subscript","typealias","var",
    "break","case","continue","default","defer","do","else","fallthrough",
    "for","guard","if","in","repeat","return","switch","where","while",
    "as","catch","false","is","nil","rethrows","super","self","Self",
    "throw","throws","true","try","async","await","some","any",
    "Int|","Double|","Float|","Bool|","String|","Character|","Array|",
    "Dictionary|","Set|","Optional|","Result|","Error|", NULL
};

char *RUST_HL_extensions[] = { ".rs", NULL };
char *RUST_HL_keywords[] = {
    "as","async","await","break","const","continue","crate","dyn","else",
    "enum","extern","false","fn","for","if","impl","in","let","loop",
    "match","mod","move","mut","pub","ref","return","self","Self","static",
    "struct","super","trait","true","type","unsafe","use","where","while",
    "i8|","i16|","i32|","i64|","i128|","isize|","u8|","u16|","u32|","u64|",
    "u128|","usize|","f32|","f64|","bool|","char|","str|","String|","Vec|",
    "HashMap|","HashSet|","Option|","Result|","Box|","Rc|","Arc|", NULL
};

char *SQL_HL_extensions[] = { ".sql", ".ddl", ".dml", NULL };
char *SQL_HL_keywords[] = {
    "SELECT","FROM","WHERE","INSERT","UPDATE","DELETE","CREATE","DROP","ALTER",
    "TABLE","INDEX","VIEW","DATABASE","SCHEMA","PRIMARY","FOREIGN","KEY",
    "REFERENCES","CONSTRAINT","UNIQUE","NOT","NULL","DEFAULT","AND","OR","IN",
    "BETWEEN","LIKE","IS","EXISTS","ANY","ALL","UNION","INNER","LEFT","RIGHT",
    "FULL","OUTER","JOIN","ON","GROUP","BY","HAVING","ORDER","ASC","DESC",
    "LIMIT","OFFSET","DISTINCT","AS","CASE","WHEN","THEN","ELSE","END",
    "BEGIN","COMMIT","ROLLBACK","TRANSACTION","COUNT","SUM","AVG","MIN","MAX",
    "INT|","INTEGER|","BIGINT|","VARCHAR|","TEXT|","BOOLEAN|","FLOAT|",
    "DOUBLE|","DECIMAL|","DATE|","TIMESTAMP|","TRUE|","FALSE|", NULL
};

char *DART_HL_extensions[] = { ".dart", NULL };
char *DART_HL_keywords[] = {
    "abstract","as","assert","async","await","break","case","catch","class",
    "const","continue","covariant","default","deferred","do","dynamic","else",
    "enum","export","extends","extension","external","factory","false","final",
    "finally","for","get","hide","if","implements","import","in","interface",
    "is","late","library","mixin","new","null","on","operator","part",
    "required","rethrow","return","set","show","static","super","switch",
    "sync","this","throw","true","try","typedef","var","void","while",
    "with","yield",
    "int|","double|","num|","String|","bool|","List|","Map|","Set|",
    "Future|","Stream|","Object|","dynamic|", NULL
};

char *SHELL_HL_extensions[] = {
    ".sh", ".bash", ".zsh", ".ksh",
    ".bashrc", ".bash_profile", ".zshrc", ".zshenv", NULL
};
char *SHELL_HL_keywords[] = {
    "if","then","else","elif","fi","case","esac","for","while",
    "until","do","done","select","function","in","time",
    "alias|","bg|","break|","builtin|","cd|","command|","continue|","declare|",
    "echo|","eval|","exec|","exit|","export|","fg|","getopts|","hash|",
    "history|","jobs|","kill|","let|","local|","printf|","pwd|","read|",
    "readonly|","return|","set|","shift|","source|","test|","trap|",
    "true|","type|","typeset|","ulimit|","umask|","unset|","wait|",
    "awk|","cat|","chmod|","chown|","cp|","curl|","cut|","date|",
    "df|","diff|","du|","find|","grep|","head|","ln|","ls|",
    "mkdir|","mv|","ps|","rm|","sed|","ssh|","sudo|","tail|",
    "tar|","touch|","tr|","uniq|","wc|","wget|","which|","xargs|",
    "$HOME|","$PATH|","$PWD|","$USER|","$SHELL|","$IFS|",
    "$OLDPWD|","$PPID|","$RANDOM|","$SECONDS|","$LINENO|", NULL
};

char *HTML_HL_extensions[] = { ".html", ".htm", ".xhtml", NULL };
char *HTML_HL_keywords[] = {
    "<a>","<abbr>","<address>","<article>","<aside>","<audio>",
    "<b>","<blockquote>","<body>","<br>","<button>",
    "<canvas>","<caption>","<cite>","<code>","<colgroup>",
    "<data>","<datalist>","<dd>","<del>","<details>","<dfn>","<dialog>",
    "<div>","<dl>","<dt>","<em>","<embed>",
    "<fieldset>","<figcaption>","<figure>","<footer>","<form>",
    "<h1>","<h2>","<h3>","<h4>","<h5>","<h6>",
    "<head>","<header>","<hr>","<html>",
    "<i>","<iframe>","<img>","<input>","<ins>",
    "<label>","<legend>","<li>","<link>",
    "<main>","<map>","<mark>","<meta>","<meter>",
    "<nav>","<noscript>","<object>","<ol>","<option>","<output>",
    "<p>","<picture>","<pre>","<progress>","<q>",
    "<s>","<script>","<section>","<select>","<small>","<source>",
    "<span>","<strong>","<style>","<sub>","<summary>","<sup>","<svg>",
    "<table>","<tbody>","<td>","<template>","<textarea>",
    "<tfoot>","<th>","<thead>","<time>","<title>","<tr>",
    "<u>","<ul>","<var>","<video>","<wbr>",
    "</a>","</article>","</aside>","</audio>","</b>","</blockquote>",
    "</body>","</button>","</canvas>","</caption>","</cite>","</code>",
    "</div>","</em>","</fieldset>","</figcaption>","</figure>",
    "</footer>","</form>","</h1>","</h2>","</h3>","</h4>","</h5>","</h6>",
    "</head>","</header>","</html>","</i>","</iframe>","</ins>",
    "</label>","</legend>","</li>","</main>","</mark>","</meter>",
    "</nav>","</noscript>","</object>","</ol>","</option>","</output>",
    "</p>","</picture>","</pre>","</progress>","</q>","</s>",
    "</script>","</section>","</select>","</small>","</span>","</strong>",
    "</style>","</sub>","</summary>","</sup>","</svg>","</table>",
    "</tbody>","</td>","</template>","</textarea>","</tfoot>",
    "</th>","</thead>","</time>","</title>","</tr>","</u>","</ul>",
    "</var>","</video>",
    "<!DOCTYPE>", NULL
};

char *JSX_HL_extensions[] = { ".jsx", ".tsx", NULL };
char *JSX_HL_keywords[] = {
    "break","case","catch","class","const","continue","default","delete",
    "do","else","export","extends","finally","for","function","if","import",
    "in","instanceof","let","new","return","super","switch","this","throw",
    "try","typeof","var","void","while","yield","async","await","of",
    "true","false","null","undefined",
    "interface","type","enum","declare","abstract","implements",
    "private","protected","public","readonly","static","as","keyof",
    "Component|","PureComponent|","memo|","Fragment|","Suspense|","lazy|",
    "useState|","useEffect|","useContext|","useReducer|","useCallback|",
    "useMemo|","useRef|","useLayoutEffect|",
    "createContext|","createElement|","ReactDOM|",
    "onClick|","onChange|","onSubmit|","onKeyDown|","onKeyUp|",
    "onFocus|","onBlur|","onMouseOver|","onMouseLeave|",
    "className|","htmlFor|",
    "string|","number|","boolean|","any|","void|","never|","Array|","Promise|", NULL
};

char *VUE_HL_extensions[] = { ".vue", NULL };
char *VUE_HL_keywords[] = {
    "v-text|","v-html|","v-show|","v-if|","v-else|","v-else-if|","v-for|",
    "v-on|","v-bind|","v-model|","v-slot|","v-pre|","v-cloak|","v-once|",
    "@click|","@input|","@change|","@submit|","@keydown|","@keyup|",
    "@focus|","@blur|","@scroll|","@load|",
    ":key|",":class|",":style|",":ref|",":is|",
    "data|","props|","computed|","methods|","watch|","components|",
    "beforeCreate|","created|","beforeMount|","mounted|",
    "beforeUpdate|","updated|","beforeDestroy|","destroyed|",
    "template|","render|","name|","provide|","inject|",
    "$route|","$router|","router-link|","router-view|", NULL
};

char *SVELTE_HL_extensions[] = { ".svelte", NULL };
char *SVELTE_HL_keywords[] = {
    "if","else","each","await","then","catch","as","in",
    "on:|","bind:|","class:|","use:|","transition:|","in:|","out:|","animate:|",
    "writable|","readable|","derived|","get|","subscribe|","set|","update|",
    "onMount|","onDestroy|","beforeUpdate|","afterUpdate|","tick|",
    "createEventDispatcher|","getContext|","setContext|",
    "svelte:self|","svelte:component|","svelte:window|",
    "svelte:body|","svelte:head|","svelte:options|", NULL
};

struct editorSyntax HLDB[] = {
    { "c",      C_HL_extensions,    C_HL_keywords,    "//", "/*", "*/",  HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "python", PYTHON_HL_extensions, PYTHON_HL_keywords, "#", "", "",   HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "java",   JAVA_HL_extensions,  JAVA_HL_keywords,  "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "js",     JS_HL_extensions,    JS_HL_keywords,    "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "ts",     TS_HL_extensions,    TS_HL_keywords,    "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "csharp", CSHARP_HL_extensions, CSHARP_HL_keywords,"//","/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "php",    PHP_HL_extensions,   PHP_HL_keywords,   "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "ruby",   RUBY_HL_extensions,  RUBY_HL_keywords,  "#",  "", "",    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "swift",  SWIFT_HL_extensions, SWIFT_HL_keywords, "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "rust",   RUST_HL_extensions,  RUST_HL_keywords,  "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "sql",    SQL_HL_extensions,   SQL_HL_keywords,   "--", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "dart",   DART_HL_extensions,  DART_HL_keywords,  "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "shell",  SHELL_HL_extensions, SHELL_HL_keywords, "#",  "", "",    HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "html",   HTML_HL_extensions,  HTML_HL_keywords,  "", "", "",      HL_HIGHLIGHT_STRINGS },
    { "jsx",    JSX_HL_extensions,   JSX_HL_keywords,   "//", "/*", "*/", HL_HIGHLIGHT_NUMBERS | HL_HIGHLIGHT_STRINGS },
    { "vue",    VUE_HL_extensions,   VUE_HL_keywords,   "", "", "",      HL_HIGHLIGHT_STRINGS },
    { "svelte", SVELTE_HL_extensions, SVELTE_HL_keywords, "", "", "",    HL_HIGHLIGHT_STRINGS },
};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/
void editorSetStatusMessage(const char *fmt, ...);
void editorRefreshScreen(void);
char *editorPrompt(char *prompt, void (*callback)(char *, int));
void editorMoveCursor(int key);
void editorInsertChar(int c);

/*** terminal ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(s);
    exit(1);
}

void disableRawMode(void) {
    if (E.rawmode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios);
        E.rawmode = 0;
    }
}

void editorAtExit(void) {
    disableRawMode();
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
}

void enableRawMode(void) {
    if (E.rawmode) return;
    if (!isatty(STDIN_FILENO)) die("not a tty");
    atexit(editorAtExit);

    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");

    struct termios raw = E.orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
    E.rawmode = 1;
}

int editorReadKey(void) {
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }

    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            } else {
                switch (seq[1]) {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
    }
    return c;
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;

    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
    while (i < sizeof(buf) - 1) {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
    return 0;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

/*** syntax highlighting ***/
int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    if (row->rsize > 0)
        memset(row->hl, HL_NORMAL, row->rsize);

    if (E.syntax == NULL) return;

    char **keywords = E.syntax->keywords;
    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? (int)strlen(scs) : 0;
    int mcs_len = mcs ? (int)strlen(mcs) : 0;
    int mce_len = mce ? (int)strlen(mce) : 0;

    int prev_sep   = 1;
    int in_string  = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        /* Single-line comment */
        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        /* Multi-line comment */
        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                } else {
                    i++;
                    continue;
                }
            } else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

        /* Strings */
        if (E.syntax->flags & HL_HIGHLIGHT_STRINGS) {
            if (in_string) {
                row->hl[i] = HL_STRING;
                if (c == '\\' && i + 1 < row->rsize) {
                    row->hl[i + 1] = HL_STRING;
                    i += 2;
                    continue;
                }
                if (c == in_string) in_string = 0;
                i++;
                prev_sep = 1;
                continue;
            } else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        /* Numbers */
        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        /* Non-printable characters */
        if (!isprint((unsigned char)c)) {
            row->hl[i] = HL_NONPRINT;
            i++;
            prev_sep = 0;
            continue;
        }

        /* Keywords */
        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++) {
                int klen = (int)strlen(keywords[j]);
                int kw2  = keywords[j][klen - 1] == '|';
                if (kw2) klen--;

                if (!strncmp(&row->render[i], keywords[j], klen) &&
                        is_separator(row->render[i + klen])) {
                    memset(&row->hl[i], kw2 ? HL_KEYWORD2 : HL_KEYWORD1, klen);
                    i += klen;
                    break;
                }
            }
            if (keywords[j] != NULL) {
                prev_sep = 0;
                continue;
            }
        }

        prev_sep = is_separator(c);
        i++;
    }

    int changed = (row->hl_open_comment != in_comment);
    row->hl_open_comment = in_comment;
    if (changed && row->idx + 1 < E.numrows)
        editorUpdateSyntax(&E.row[row->idx + 1]);
}

int editorSyntaxToColor(int hl) {
    switch (hl) {
        case HL_COMMENT:
        case HL_MLCOMMENT: return 36;
        case HL_KEYWORD1:  return 33;
        case HL_KEYWORD2:  return 32;
        case HL_STRING:    return 35;
        case HL_NUMBER:    return 31;
        case HL_MATCH:     return 34;
        default:           return 37;
    }
}

void editorSelectSyntaxHighlight(void) {
    E.syntax = NULL;
    if (E.filename == NULL) return;

    char *ext = strrchr(E.filename, '.');

    for (unsigned int j = 0; j < HLDB_ENTRIES; j++) {
        struct editorSyntax *s = &HLDB[j];
        unsigned int i = 0;
        while (s->filematch[i]) {
            int is_ext = (s->filematch[i][0] == '.');
            if ((is_ext && ext && !strcmp(ext, s->filematch[i])) ||
                (!is_ext && strstr(E.filename, s->filematch[i]))) {
                E.syntax = s;
                for (int filerow = 0; filerow < E.numrows; filerow++)
                    editorUpdateSyntax(&E.row[filerow]);
                return;
            }
            i++;
        }
    }
}

/*** row operations ***/
int editorRowCxToRx(erow *row, int cx) {
    int rx = 0;
    for (int j = 0; j < cx; j++) {
        if (row->chars[j] == '\t')
            rx += (FILO_TAB_STOP - 1) - (rx % FILO_TAB_STOP);
        rx++;
    }
    return rx;
}

int editorRowRxToCx(erow *row, int rx) {
    int cur_rx = 0;
    int cx;
    for (cx = 0; cx < row->size; cx++) {
        if (row->chars[cx] == '\t')
            cur_rx += (FILO_TAB_STOP - 1) - (cur_rx % FILO_TAB_STOP);
        cur_rx++;
        if (cur_rx > rx) return cx;
    }
    return cx;
}

void editorUpdateRow(erow *row) {
    int tabs = 0;
    for (int j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs * (FILO_TAB_STOP - 1) + 1);

    int idx = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % FILO_TAB_STOP != 0) row->render[idx++] = ' ';
        } else {
            row->render[idx++] = row->chars[j];
        }
    }
    row->render[idx] = '\0';
    row->rsize = idx;

    editorUpdateSyntax(row);
}

void editorInsertRow(int at, char *s, size_t len) {
    if (at < 0 || at > E.numrows) return;

    E.row = realloc(E.row, sizeof(erow) * (E.numrows + 1));
    memmove(&E.row[at + 1], &E.row[at], sizeof(erow) * (E.numrows - at));
    for (int j = at + 1; j <= E.numrows; j++) E.row[j].idx++;

    E.row[at].idx  = at;
    E.row[at].size = len;
    E.row[at].chars = malloc(len + 1);
    memcpy(E.row[at].chars, s, len);
    E.row[at].chars[len] = '\0';
    E.row[at].rsize = 0;
    E.row[at].render = NULL;
    E.row[at].hl = NULL;
    E.row[at].hl_open_comment = 0;
    editorUpdateRow(&E.row[at]);

    E.numrows++;
    E.dirty++;
}

void editorFreeRow(erow *row) {
    free(row->render);
    free(row->chars);
    free(row->hl);
}

void editorDelRow(int at) {
    if (at < 0 || at >= E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at], &E.row[at + 1], sizeof(erow) * (E.numrows - at - 1));
    for (int j = at; j < E.numrows - 1; j++) E.row[j].idx--;
    E.numrows--;
    E.dirty++;
}

void editorRowInsertChar(erow *row, int at, int c) {
    if (at < 0 || at > row->size) at = row->size;
    row->chars = realloc(row->chars, row->size + 2);
    memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
    row->size++;
    row->chars[at] = c;
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowAppendString(erow *row, char *s, size_t len) {
    row->chars = realloc(row->chars, row->size + len + 1);
    memcpy(&row->chars[row->size], s, len);
    row->size += len;
    row->chars[row->size] = '\0';
    editorUpdateRow(row);
    E.dirty++;
}

void editorRowDelChar(erow *row, int at) {
    if (at < 0 || at >= row->size) return;
    memmove(&row->chars[at], &row->chars[at + 1], row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/

/* Autocompletion pairs */
struct autopair {
    int open_char;
    int close_char;
};

struct autopair autopairs[] = {
    { '{',  '}' },
    { '[',  ']' },
    { '(',  ')' },
    { '"',  '"' },
    { '\'', '\'' },
    { '`',  '`'  },
    { '<',  '>'  },
};
#define AUTOPAIRS_COUNT (sizeof(autopairs) / sizeof(autopairs[0]))

int editorFindCloseChar(int open_char) {
    for (size_t i = 0; i < AUTOPAIRS_COUNT; i++) {
        if (autopairs[i].open_char == open_char)
            return autopairs[i].close_char;
    }
    return 0;
}

void editorInsertChar(int c) {
    if (E.cy == E.numrows)
        editorInsertRow(E.numrows, "", 0);
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertCharAutoComplete(int c) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int at_end          = (!row || E.cx >= row->size);
    int next_is_special = at_end
        || isspace((unsigned char)row->chars[E.cx])
        || strchr(",.()+-/*=~%[];{}", row->chars[E.cx]) != NULL;

    int close_char = editorFindCloseChar(c);
    editorInsertChar(c);

    if (!E.paste_mode && close_char && next_is_special) {
        editorInsertChar(close_char);
        editorMoveCursor(ARROW_LEFT);
    }
}

void editorInsertNewline(void) {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    } else {
        erow *row = &E.row[E.cy];
        editorInsertRow(E.cy + 1, &row->chars[E.cx], row->size - E.cx);
        row = &E.row[E.cy];
        row->size = E.cx;
        row->chars[row->size] = '\0';
        editorUpdateRow(row);
    }
    E.cy++;
    E.cx = 0;
}

void editorDelChar(void) {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];

    /* Ctrl-H: delete the entire current line */
    if (E.last_key == CTRL_KEY('h')) {
        editorDelRow(E.cy);
        if (E.cy > 0 && E.cy >= E.numrows) E.cy--;
        E.cx = 0;
        return;
    }

    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    } else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy);
        E.cy--;
    }
}

/*** file i/o ***/
char *editorRowsToString(int *buflen) {
    int totlen = 0;
    for (int j = 0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for (int j = 0; j < E.numrows; j++) {
        memcpy(p, E.row[j].chars, E.row[j].size);
        p += E.row[j].size;
        *p = '\n';
        p++;
    }
    return buf;
}

void editorOpen(char *filename) {
    free(E.filename);
    E.filename = strdup(filename);
    editorSelectSyntaxHighlight();

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        /* New file is not an error */
        if (errno != ENOENT) die("fopen");
        return;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    while ((linelen = getline(&line, &linecap, fp)) != -1) {
        while (linelen > 0 && (line[linelen - 1] == '\n' ||
                               line[linelen - 1] == '\r'))
            linelen--;
        editorInsertRow(E.numrows, line, linelen);
    }
    free(line);
    fclose(fp);
    E.dirty = 0;
}

void editorSave(void) {
    if (E.filename == NULL) {
        E.filename = editorPrompt("Save as: %s (ESC to cancel)", NULL);
        if (E.filename == NULL) {
            editorSetStatusMessage("Save aborted");
            return;
        }
        editorSelectSyntaxHighlight();
    }

    int len;
    char *buf = editorRowsToString(&len);

    char tmpname[1024];
    snprintf(tmpname, sizeof(tmpname), "%s.tmp", E.filename);

    int fd = open(tmpname, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) goto fail;

    if (write(fd, buf, len) != len) goto fail;
    if (fsync(fd) == -1) goto fail;

    close(fd);

    if (rename(tmpname, E.filename) == -1) goto fail;

    free(buf);
    E.dirty = 0;
    editorSetStatusMessage("%d bytes written safely", len);
    return;

fail:
    close(fd);
    unlink(tmpname);
    free(buf);
    editorSetStatusMessage("Save failed: %s", strerror(errno));
}

/*** find ***/
void editorFindCallback(char *query, int key) {
    static int last_match = -1;
    static int direction  = 1;
    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction  = 1;
        return;
    } else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    } else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    } else {
        last_match = -1;
        direction  = 1;
    }

    if (last_match == -1) direction = 1;
    int current = last_match;

    for (int i = 0; i < E.numrows; i++) {
        current += direction;
        if (current == -1)          current = E.numrows - 1;
        else if (current == E.numrows) current = 0;

        erow *row   = &E.row[current];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E.cy     = current;
            E.cx     = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind(void) {
    int saved_cx     = E.cx;
    int saved_cy     = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search: %s (ESC/Arrows/Enter)", editorFindCallback);
    if (query) {
        free(query);
    } else {
        E.cx     = saved_cx;
        E.cy     = saved_cy;
        E.coloff = saved_coloff;
        E.rowoff = saved_rowoff;
    }
}

/*** append buffer ***/
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL, 0}

void abAppend(struct abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/
void editorScroll(void) {
    E.rx = 0;
    if (E.cy < E.numrows)
        E.rx = editorRowCxToRx(&E.row[E.cy], E.cx);

    if (E.cy < E.rowoff)
        E.rowoff = E.cy;
    if (E.cy >= E.rowoff + E.screenrows)
        E.rowoff = E.cy - E.screenrows + 1;
    if (E.rx < E.coloff)
        E.coloff = E.rx;
    if (E.rx >= E.coloff + E.screencols)
        E.coloff = E.rx - E.screencols + 1;
}

void editorDrawRows(struct abuf *ab) {
    for (int y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Filó editor -- version %s", FILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            } else {
                abAppend(ab, "~", 1);
            }
        } else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;

            char *c          = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;

            for (int j = 0; j < len; j++) {
                if (hl[j] == HL_NONPRINT) {
                    char sym = ((unsigned char)c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                } else if (hl[j] == HL_NORMAL) {
                    if (current_color != -1) {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                } else {
                    int color = editorSyntaxToColor(hl[j]);
                    if (color != current_color) {
                        current_color = color;
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", color);
                        abAppend(ab, buf, clen);
                    }
                    abAppend(ab, &c[j], 1);
                }
            }
            abAppend(ab, "\x1b[39m", 5);
        }
        abAppend(ab, "\x1b[K", 3);
        abAppend(ab, "\r\n", 2);
    }
}

void editorDrawStatusBar(struct abuf *ab) {
    abAppend(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    int len = snprintf(status, sizeof(status), "%.20s - %d lines %s",
        E.filename ? E.filename : "[No Name]", E.numrows,
        E.dirty ? "(modified)" : "");
    int rlen = snprintf(rstatus, sizeof(rstatus), "%s | %d/%d",
        E.syntax ? E.syntax->filetype : "no ft", E.cy + 1, E.numrows);
    if (len > E.screencols) len = E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }
        abAppend(ab, " ", 1);
        len++;
    }
    abAppend(ab, "\x1b[m", 3);
    abAppend(ab, "\r\n", 2);
}

void editorDrawMessageBar(struct abuf *ab) {
    abAppend(ab, "\x1b[K", 3);
    int msglen = strlen(E.statusmsg);
    if (msglen > E.screencols) msglen = E.screencols;
    if (msglen && time(NULL) - E.statusmsg_time < 5)
        abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen(void) {
    editorScroll();

    struct abuf ab = ABUF_INIT;
    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH",
        (E.cy - E.rowoff) + 1,
        (E.rx - E.coloff) + 1);
    abAppend(&ab, buf, strlen(buf));
    abAppend(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
    va_end(ap);
    E.statusmsg_time = time(NULL);
}

/*** input ***/
char *editorPrompt(char *prompt, void (*callback)(char *, int)) {
    size_t bufsize = 128;
    char  *buf = malloc(bufsize);
    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY || c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        } else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage("");
                if (callback) callback(buf, c);
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen]   = '\0';
        }
        if (callback) callback(buf, c);
    }
}

void editorMoveCursor(int key) {
    erow *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];

    switch (key) {
        case ARROW_LEFT:
            if (E.cx != 0) {
                E.cx--;
            } else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            } else if (row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
        case ARROW_UP:
            if (E.cy != 0) E.cy--;
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) E.cy++;
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) E.cx = rowlen;
}

void editorProcessKeypress(void) {
    static int quit_times = FILO_QUIT_TIMES;

    int c = editorReadKey();
    E.last_key = c;

    /* Paste mode detection: rapid successive chars = paste */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    if (tv.tv_sec == E.last_char_time.tv_sec &&
        (tv.tv_usec - E.last_char_time.tv_usec) < 30000) {
        E.paste_mode = 1;
    } else {
        E.paste_mode = 0;
    }
    E.last_char_time = tv;

    switch (c) {
        case '\r':
            editorInsertNewline();
            break;

        case CTRL_KEY('q'):
            if (E.dirty && quit_times > 0) {
                editorSetStatusMessage(
                    "WARNING!!! File has unsaved changes. "
                    "Press Ctrl-Q %d more times to quit.", quit_times);
                quit_times--;
                return;
            }
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            break;

        case CTRL_KEY('s'):
            editorSave();
            break;

        case HOME_KEY:
            E.cx = 0;
            break;
        case END_KEY:
            if (E.cy < E.numrows)
                E.cx = E.row[E.cy].size;
            break;

        case CTRL_KEY('f'):
            editorFind();
            break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
            if (c == DEL_KEY) editorMoveCursor(ARROW_RIGHT);
            editorDelChar();
            break;

        case PAGE_UP:
        case PAGE_DOWN:
        {
            if (c == PAGE_UP)
                E.cy = E.rowoff;
            else {
                E.cy = E.rowoff + E.screenrows - 1;
                if (E.cy > E.numrows) E.cy = E.numrows;
            }
            int times = E.screenrows;
            while (times--)
                editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
        break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;

        case CTRL_KEY('c'):
            /* Ignore Ctrl-C to prevent accidental loss of changes */
            break;

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertCharAutoComplete(c);
            break;
    }

    quit_times = FILO_QUIT_TIMES;
}

/*** window resize ***/
void updateWindowSize(void) {
    int rows, cols;
    int attempts = 0;
    while (attempts < 3) {
        if (getWindowSize(&rows, &cols) == 0) {
            E.screenrows = rows - 2;
            E.screencols = cols;
            return;
        }
        attempts++;
        if (attempts < 3) usleep(10000);
    }
    editorSetStatusMessage("Warning: Could not update window size");
}

void handleSigWinCh(int unused __attribute__((unused))) {
    updateWindowSize();
    if (E.cy >= E.screenrows) E.cy = E.screenrows - 1;
    if (E.cx >= E.screencols) E.cx = E.screencols - 1;
    if (E.rowoff + E.cy >= E.numrows) {
        E.rowoff = E.numrows - E.cy - 1;
        if (E.rowoff < 0) E.rowoff = 0;
    }
    editorRefreshScreen();
}

/*** init ***/
void initEditor(void) {
    E.cx    = 0;
    E.cy    = 0;
    E.rx    = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row   = NULL;
    E.dirty = 0;
    E.rawmode   = 0;
    E.paste_mode = 0;
    E.last_key  = 0;
    E.filename  = NULL;
    E.statusmsg[0]   = '\0';
    E.statusmsg_time = 0;
    E.syntax    = NULL;
    E.last_char_time.tv_sec  = 0;
    E.last_char_time.tv_usec = 0;

    updateWindowSize();
    signal(SIGWINCH, handleSigWinCh);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: filo <filename>\n");
        exit(1);
    }

    initEditor();
    editorOpen(argv[1]);
    enableRawMode();
    editorSetStatusMessage(
        "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find | Ctrl-H = delete line");

    while (1) {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}