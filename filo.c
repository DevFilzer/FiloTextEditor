/*** includes ***/
#define  _DEFAULT_SOURCE
#define _BSD_SOURCE // change in case of conflict
#define _GNU_SOURCE

#include <ctype.h>  
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/*** defines ***/
#define FILO_VERSION "1.0.1"
#define FILO_TAB_STOP 8
#define FILO_QUIT_TIMES 3

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    KELL_NULL = 0,
    CTRL_C = 3,
    CTRL_D = 4,
    CTRL_F = 6,
    CTRL_H = 8,
    TAB = 9,
    CTRL_L = 12,
    ENTER = 13,
    CTRL_Q = 17,
    CTRL_S = 19,
    CTRL_U = 21,
    ESC = 27,
    BACKSPACE = 127,

    // Non-comadding keys
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

#define HL_NORMAL 0
#define HL_NONPRINT 1
#define HL_COMMENT 2 
#define HL_MLCOMMENT 3
#define HL_KEYWORD1 4
#define HL_KEYWORD2 5
#define HL_STRING 6
#define HL_NUMBER 7
#define HL_MATCH 8

#define HL_HIGHLIGHT_NUMBERS (1 << 0)
#define HL_HIGHLIGHT_STRINGS (1 << 1)

/*** data ***/
struct editorSyntax {
    char **filematch;
    char **keywords;
    char *singleline_comment_start[5]; // 4 chars + null -> cover most languages
    char *multiline_comment_start[5];
    char *multiline_comment_end[5];
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

typedef struct highlight_color {
    int r, g, b;
} highlight_color;

struct editorConfig 
{
    int cx, cy; // cursor x and y in chars
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    int rawmode; // is raw mode on? in the terminal
    erow *row;
    int dirty;
    int paste_mode; // disables autocomplete
    int last_key; // last key pressed by user
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;
    struct editorSyntax *syntax;
    struct timeval last_char_time; // time of last char for paste detection
};

struct editorConfig E;

void editorSetStatusMessage(const char *fmt, ...);

/*** filetypes ***/
char *C_HL_extensions[] = {".c", ".h", ".cpp", ".hpp", ".cc", ".cxx", ".c++", ".hxx", ".h++", NULL};
char *C_HL_keywords[] = {
    /* C Keywords */
    "auto", "break", "case", "continue", "default", "do", "else", "enum",
    "extern", "for", "goto", "if", "register", "return", "sizeof", "static",
    "struct", "switch", "typedef", "union", "volatile", "while", "NULL",

    /* C++ Keywords */
    "alignas", "alignof", "and", "and_eq", "asm", "bitand", "bitor", "class",
    "compl", "constexpr", "const_cast", "deltype", "delete", "dynamic_cast",
    "explicit", "export", "false", "friend", "inline", "mutable", "namespace",
    "new", "noexcept", "not", "not_eq", "nullptr", "operator", "or", "or_eq",
    "private", "protected", "public", "reinterpret_cast", "static_assert",
    "static_cast", "template", "this", "thread_local", "throw", "true", "try",
    "typeid", "typename", "virtual", "xor", "xor_eq",

    /* C types */
    "int|", "long|", "double|", "float|", "char|", "unsigned|", "signed|",
    "void|", "short|", "auto|", "const|", "bool|", NULL};

/* Python */
char *PYTHON_HL_extensions[] = {".py", ".pyw", ".pyi", ".pyx", NULL};
char *PYTHON_HL_keywords[] = {
    /* Python Keywords */
    "and", "as", "assert", "break", "class", "continue", "def", "del",
    "elif", "else", "except", "exec", "finally", "for", "from", "global",
    "if", "import", "in", "is", "lambda", "not", "or", "pass", "print",
    "raise", "return", "try", "while", "with", "yield", "async", "await",
    "nonlocal", "True", "False", "None",

    /* Python Built-ins */
    "abs|", "all|", "any|", "bin|", "bool|", "bytearray|", "bytes|", "callable|",
    "chr|", "classmethod|", "compile|", "complex|", "delattr|", "dict|", "dir|",
    "divmod|", "enumerate|", "eval|", "exec|", "filter|", "float|", "format|",
    "frozenset|", "getattr|", "globals|", "hasattr|", "hash|", "help|", "hex|",
    "id|", "input|", "int|", "isinstance|", "issubclass|", "iter|", "len|",
    "list|", "locals|", "map|", "max|", "memoryview|", "min|", "next|", "object|",
    "oct|", "open|", "ord|", "pow|", "property|", "range|", "repr|", "reversed|",
    "round|", "set|", "setattr|", "slice|", "sorted|", "staticmethod|", "str|",
    "sum|", "super|", "tuple|", "type|", "vars|", "zip|", "self|", "cls|", NULL};

/* Java */
char *JAVA_HL_extensions[] = {".java", ".class", NULL};
char *JAVA_HL_keywords[] = {
    /* Java Keywords */
    "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char",
    "class", "const", "continue", "default", "do", "double", "else", "enum",
    "extends", "final", "finally", "float", "for", "goto", "if", "implements",
    "import", "instanceof", "int", "interface", "long", "native", "new", "package",
    "private", "protected", "public", "return", "short", "static", "strictfp",
    "super", "switch", "synchronized", "this", "throw", "throws", "transient",
    "try", "void", "volatile", "while", "true", "false", "null",

    /* Java Types and Common Classes */
    "String|", "Object|", "Class|", "System|", "Thread|", "Runnable|",
    "Exception|", "RuntimeException|", "ArrayList|", "HashMap|", "List|",
    "Map|", "Set|", "Collection|", "Iterator|", "Comparable|", "Serializable|", NULL};

/* JavaScript */
char *JS_HL_extensions[] = {".js", ".jsx", ".mjs", ".cjs", NULL};
char *JS_HL_keywords[] = {
    /* JavaScript Keywords */
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "export", "extends", "finally", "for", "function",
    "if", "import", "in", "instanceof", "let", "new", "return", "super", "switch",
    "this", "throw", "try", "typeof", "var", "void", "while", "with", "yield",
    "async", "await", "of", "true", "false", "null", "undefined",

    /* JavaScript Built-ins */
    "Array|", "Object|", "String|", "Number|", "Boolean|", "Date|", "Math|",
    "RegExp|", "Error|", "JSON|", "console|", "window|", "document|", "setTimeout|",
    "setInterval|", "clearTimeout|", "clearInterval|", "parseInt|", "parseFloat|",
    "isNaN|", "isFinite|", "encodeURI|", "decodeURI|", "Promise|", "Map|", "Set|",
    "WeakMap|", "WeakSet|", "Symbol|", "Proxy|", "Reflect|", "Generator|", NULL};

/* TypeScript */
char *TS_HL_extensions[] = {".ts", ".tsx", ".d.ts", NULL};
char *TS_HL_keywords[] = {
    /* TypeScript Keywords (includes JavaScript) */
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "export", "extends", "finally", "for", "function",
    "if", "import", "in", "instanceof", "let", "new", "return", "super", "switch",
    "this", "throw", "try", "typeof", "var", "void", "while", "with", "yield",
    "async", "await", "of", "true", "false", "null", "undefined",

    /* TypeScript Specific */
    "interface", "type", "enum", "namespace", "module", "declare", "abstract",
    "implements", "private", "protected", "public", "readonly", "static",
    "get", "set", "as", "keyof", "infer", "is", "asserts",

    /* TypeScript Types */
    "string|", "number|", "boolean|", "object|", "any|", "unknown|", "never|",
    "void|", "bigint|", "symbol|", "Array|", "Promise|", "Record|", "Partial|",
    "Required|", "Pick|", "Omit|", "Exclude|", "Extract|", "NonNullable|", NULL};

/* C# */
char *CSHARP_HL_extensions[] = {".cs", ".csx", NULL};
char *CSHARP_HL_keywords[] = {
    /* C# Keywords */
    "abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char",
    "checked", "class", "const", "continue", "decimal", "default", "delegate",
    "do", "double", "else", "enum", "event", "explicit", "extern", "false",
    "finally", "fixed", "float", "for", "foreach", "goto", "if", "implicit",
    "in", "int", "interface", "internal", "is", "lock", "long", "namespace",
    "new", "null", "object", "operator", "out", "override", "params", "private",
    "protected", "public", "readonly", "ref", "return", "sbyte", "sealed",
    "short", "sizeof", "stackalloc", "static", "string", "struct", "switch",
    "this", "throw", "true", "try", "typeof", "uint", "ulong", "unchecked",
    "unsafe", "ushort", "using", "virtual", "void", "volatile", "while",
    "async", "await", "var", "dynamic", "yield", "where", "when", "nameof",

    /* C# Types */
    "String|", "Object|", "Int32|", "Boolean|", "Double|", "DateTime|", "List|",
    "Dictionary|", "Array|", "IEnumerable|", "ICollection|", "IList|", "Task|",
    "Exception|", "ArgumentException|", "NullReferenceException|", NULL};

/* PHP */
char *PHP_HL_extensions[] = {".php", ".phtml", ".php3", ".php4", ".php5", ".phps", NULL};
char *PHP_HL_keywords[] = {
    /* PHP Keywords */
    "abstract", "and", "array", "as", "break", "callable", "case", "catch",
    "class", "clone", "const", "continue", "declare", "default", "die", "do",
    "echo", "else", "elseif", "empty", "enddeclare", "endfor", "endforeach",
    "endif", "endswitch", "endwhile", "eval", "exit", "extends", "final",
    "finally", "for", "foreach", "function", "global", "goto", "if", "implements",
    "include", "include_once", "instanceof", "insteadof", "interface", "isset",
    "list", "namespace", "new", "or", "print", "private", "protected", "public",
    "require", "require_once", "return", "static", "switch", "throw", "trait",
    "try", "unset", "use", "var", "while", "xor", "yield", "true", "false", "null",

    /* PHP Built-ins */
    "$_GET|", "$_POST|", "$_SESSION|", "$_COOKIE|", "$_SERVER|", "$_FILES|",
    "$_ENV|", "$_REQUEST|", "$GLOBALS|", "strlen|", "substr|", "strpos|",
    "explode|", "implode|", "array_merge|", "array_push|", "array_pop|",
    "count|", "sizeof|", "is_array|", "is_string|", "is_numeric|", "empty|",
    "isset|", "unset|", "die|", "exit|", "echo|", "print|", "var_dump|", NULL};

/* Ruby */
char *RUBY_HL_extensions[] = {".rb", ".rbw", ".rake", ".gemspec", NULL};
char *RUBY_HL_keywords[] = {
    /* Ruby Keywords */
    "alias", "and", "begin", "break", "case", "class", "def", "defined", "do",
    "else", "elsif", "end", "ensure", "false", "for", "if", "in", "module",
    "next", "nil", "not", "or", "redo", "rescue", "retry", "return", "self",
    "super", "then", "true", "undef", "unless", "until", "when", "while", "yield",
    "require", "include", "extend", "attr_reader", "attr_writer", "attr_accessor",

    /* Ruby Built-ins */
    "puts|", "print|", "p|", "gets|", "chomp|", "strip|", "length|", "size|",
    "empty|", "nil|", "class|", "new|", "initialize|", "to_s|", "to_i|", "to_f|",
    "to_a|", "each|", "map|", "select|", "reject|", "find|", "inject|", "reduce|",
    "Array|", "Hash|", "String|", "Integer|", "Float|", "Symbol|", "Proc|",
    "Lambda|", "Method|", "Class|", "Module|", "Object|", "Kernel|", NULL};

/* Swift */
char *SWIFT_HL_extensions[] = {".swift", NULL};
char *SWIFT_HL_keywords[] = {
    /* Swift Keywords */
    "associatedtype", "class", "deinit", "enum", "extension", "fileprivate", "func",
    "import", "init", "inout", "internal", "let", "open", "operator", "private",
    "protocol", "public", "static", "struct", "subscript", "typealias", "var",
    "break", "case", "continue", "default", "defer", "do", "else", "fallthrough",
    "for", "guard", "if", "in", "repeat", "return", "switch", "where", "while",
    "as", "catch", "false", "is", "nil", "rethrows", "super", "self", "Self",
    "throw", "throws", "true", "try", "async", "await", "some", "any",

    /* Swift Types */
    "Int|", "Double|", "Float|", "Bool|", "String|", "Character|", "Array|",
    "Dictionary|", "Set|", "Optional|", "Result|", "Error|", "AnyObject|",
    "AnyClass|", "Protocol|", "Codable|", "Hashable|", "Equatable|",
    "Comparable|", "Collection|", "Sequence|", NULL};

/* SQL */
char *SQL_HL_extensions[] = {".sql", ".ddl", ".dml", NULL};
char *SQL_HL_keywords[] = {
    /* SQL Keywords */
    "SELECT", "FROM", "WHERE", "INSERT", "UPDATE", "DELETE", "CREATE", "DROP",
    "ALTER", "TABLE", "INDEX", "VIEW", "DATABASE", "SCHEMA", "COLUMN", "PRIMARY",
    "FOREIGN", "KEY", "REFERENCES", "CONSTRAINT", "UNIQUE", "NOT", "NULL", "DEFAULT",
    "AUTO_INCREMENT", "IDENTITY", "SERIAL", "BOOLEAN", "TINYINT", "SMALLINT",
    "MEDIUMINT", "INT", "INTEGER", "BIGINT", "DECIMAL", "NUMERIC", "FLOAT", "DOUBLE",
    "REAL", "BIT", "DATE", "TIME", "DATETIME", "TIMESTAMP", "YEAR", "CHAR", "VARCHAR",
    "BINARY", "VARBINARY", "TINYBLOB", "BLOB", "MEDIUMBLOB", "LONGBLOB", "TINYTEXT",
    "TEXT", "MEDIUMTEXT", "LONGTEXT", "ENUM", "SET", "JSON", "GEOMETRY", "POINT",
    "LINESTRING", "POLYGON", "MULTIPOINT", "MULTILINESTRING", "MULTIPOLYGON",
    "GEOMETRYCOLLECTION", "AND", "OR", "IN", "BETWEEN", "LIKE", "IS", "EXISTS",
    "ANY", "ALL", "SOME", "UNION", "INTERSECT", "EXCEPT", "INNER", "LEFT", "RIGHT",
    "FULL", "OUTER", "JOIN", "ON", "USING", "GROUP", "BY", "HAVING", "ORDER", "ASC",
    "DESC", "LIMIT", "OFFSET", "DISTINCT", "AS", "CASE", "WHEN", "THEN", "ELSE", "END",
    "IF", "IFNULL", "ISNULL", "COALESCE", "NULLIF", "CAST", "CONVERT", "SUBSTRING",
    "LENGTH", "UPPER", "LOWER", "TRIM", "LTRIM", "RTRIM", "REPLACE", "CONCAT",
    "CURRENT_DATE", "CURRENT_TIME", "CURRENT_TIMESTAMP", "NOW", "COUNT", "SUM",
    "AVG", "MIN", "MAX", "STDDEV", "VARIANCE", "BEGIN", "COMMIT", "ROLLBACK",
    "TRANSACTION", "SAVEPOINT", "GRANT", "REVOKE", "LOCK", "UNLOCK",

    /* SQL Functions and Operators */
    "TRUE|", "FALSE|", "UNKNOWN|", NULL};

/* Rust */
char *RUST_HL_extensions[] = {".rs", ".rlib", NULL};
char *RUST_HL_keywords[] = {
    /* Rust Keywords */
    "as", "async", "await", "break", "const", "continue", "crate", "dyn", "else",
    "enum", "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop",
    "match", "mod", "move", "mut", "pub", "ref", "return", "self", "Self", "static",
    "struct", "super", "trait", "true", "type", "unsafe", "use", "where", "while",
    "abstract", "become", "box", "do", "final", "macro", "override", "priv",
    "typeof", "unsized", "virtual", "yield", "try", "union", "catch", "default",

    /* Rust Types */
    "i8|", "i16|", "i32|", "i64|", "i128|", "isize|", "u8|", "u16|", "u32|", "u64|",
    "u128|", "usize|", "f32|", "f64|", "bool|", "char|", "str|", "String|", "Vec|",
    "HashMap|", "HashSet|", "BTreeMap|", "BTreeSet|", "Option|", "Result|", "Box|",
    "Rc|", "Arc|", "RefCell|", "Cell|", "Mutex|", "RwLock|", "thread|", "Clone|",
    "Copy|", "Send|", "Sync|", "Drop|", "Display|", "Debug|", "Default|", "PartialEq|",
    "Eq|", "PartialOrd|", "Ord|", "Hash|", "Iterator|", "IntoIterator|", NULL};

/* Dart */
char *DART_HL_extensions[] = {".dart", NULL};
char *DART_HL_keywords[] = {
    /* Dart Keywords */
    "abstract", "as", "assert", "async", "await", "break", "case", "catch", "class",
    "const", "continue", "covariant", "default", "deferred", "do", "dynamic", "else",
    "enum", "export", "extends", "extension", "external", "factory", "false", "final",
    "finally", "for", "Function", "get", "hide", "if", "implements", "import", "in",
    "interface", "is", "late", "library", "mixin", "new", "null", "on", "operator",
    "part", "required", "rethrow", "return", "set", "show", "static", "super", "switch",
    "sync", "this", "throw", "true", "try", "typedef", "var", "void", "while", "with",
    "yield",

    /* Dart Types */
    "int|", "double|", "num|", "String|", "bool|", "List|", "Map|", "Set|", "Object|",
    "dynamic|", "var|", "void|", "Future|", "Stream|", "Iterable|", "Iterator|",
    "Comparable|", "Duration|", "DateTime|", "Uri|", "RegExp|", "StringBuffer|",
    "Symbol|", "Type|", "Function|", "Null|", NULL};

/* Shell */
char *SHELL_HL_extensions[] = {
    ".sh", ".bash", ".zsh", ".ksh", ".csh", ".tcsh",
    ".profile", ".bashrc", ".bash_profile", ".bash_login",
    ".zshrc", ".zshenv", ".zlogin", ".zprofile",
    NULL};

char *SHELL_HL_keywords[] = {
    /* Shell Keywords */
    "if", "then", "else", "elif", "fi", "case", "esac", "for", "while",
    "until", "do", "done", "select", "function", "in", "time", "coproc",

    /* Common commands */
    "alias|", "bg|", "bind|", "break|", "builtin|", "caller|", "cd|",
    "command|", "compgen|", "complete|", "continue|", "declare|",
    "dirs|", "disown|", "echo|", "enable|", "eval|", "exec|", "exit|",
    "export|", "false|", "fc|", "fg|", "getopts|", "hash|", "help|",
    "history|", "jobs|", "kill|", "let|", "local|", "logout|", "mapfile|",
    "popd|", "printf|", "pushd|", "pwd|", "read|", "readarray|",
    "readonly|", "return|", "set|", "shift|", "shopt|", "source|",
    "suspend|", "test|", "times|", "trap|", "true|", "type|", "typeset|",
    "ulimit|", "umask|", "unalias|", "unset|", "wait|",

    /* System utilities */
    "awk|", "cat|", "chmod|", "chown|", "cp|", "curl|", "cut|", "date|",
    "df|", "diff|", "dig|", "du|", "find|", "grep|", "head|", "ln|", "ls|",
    "mkdir|", "mv|", "ping|", "ps|", "rm|", "rsync|", "scp|", "sed|",
    "ssh|", "sudo|", "tail|", "tar|", "top|", "touch|", "tr|", "uniq|",
    "wc|", "wget|", "which|", "xargs|",

    /* Special variables */
    "$BASH|", "$BASHOPTS|", "$BASHPID|", "$BASH_ALIASES|",
    "$BASH_ARGC|", "$BASH_ARGV|", "$BASH_CMDS|", "$BASH_COMMAND|",
    "$BASH_ENV|", "$BASH_LINENO|", "$BASH_SOURCE|", "$BASH_SUBSHELL|",
    "$BASH_VERSION|", "$DIRSTACK|", "$EUID|", "$FUNCNAME|",
    "$GROUPS|", "$HOME|", "$HOSTNAME|", "$HOSTTYPE|", "$IFS|",
    "$LINENO|", "$MACHTYPE|", "$OLDPWD|", "$OPTARG|", "$OPTIND|",
    "$OSTYPE|", "$PATH|", "$PIPESTATUS|", "$PPID|", "$PS1|",
    "$PS2|", "$PS3|", "$PS4|", "$PWD|", "$RANDOM|", "$REPLY|",
    "$SECONDS|", "$SHELL|", "$SHELLOPTS|", "$SHLVL|", "$UID|",
    NULL};

/* HTML */
char *HTML_HL_extensions[] = {".html", ".htm", ".xhtml", NULL};
char *HTML_HL_keywords[] = {
    /* Opening tags */
    "<a>", "<abbr>", "<address>", "<area>", "<article>", "<aside>", "<audio>",
    "<b>", "<base>", "<bdi>", "<bdo>", "<blockquote>", "<body>", "<br>", "<button>",
    "<canvas>", "<caption>", "<cite>", "<code>", "<col>", "<colgroup>",
    "<data>", "<datalist>", "<dd>", "<del>", "<details>", "<dfn>", "<dialog>",
    "<div>", "<dl>", "<dt>", "<em>", "<embed>",
    "<fieldset>", "<figcaption>", "<figure>", "<footer>", "<form>",
    "<h1>", "<h2>", "<h3>", "<h4>", "<h5>", "<h6>", "<head>", "<header>", "<hr>", "<html>",
    "<i>", "<iframe>", "<img>", "<input>", "<ins>",
    "<kbd>", "<label>", "<legend>", "<li>", "<link>",
    "<main>", "<map>", "<mark>", "<meta>", "<meter>",
    "<nav>", "<noscript>",
    "<object>", "<ol>", "<optgroup>", "<option>", "<output>",
    "<p>", "<param>", "<picture>", "<pre>", "<progress>",
    "<q>", "<rp>", "<rt>", "<ruby>",
    "<s>", "<samp>", "<script>", "<section>", "<select>", "<small>", "<source>",
    "<span>", "<strong>", "<style>", "<sub>", "<summary>", "<sup>", "<svg>",
    "<table>", "<tbody>", "<td>", "<template>", "<textarea>", "<tfoot>", "<th>", "<thead>",
    "<time>", "<title>", "<tr>", "<track>",
    "<u>", "<ul>",
    "<var>", "<video>",
    "<wbr>",

    /* Closing tags */
    "</a>", "</abbr>", "</address>", "</article>", "</aside>", "</audio>",
    "</b>", "</bdi>", "</bdo>", "</blockquote>", "</body>", "</button>",
    "</canvas>", "</caption>", "</cite>", "</code>", "</colgroup>",
    "</data>", "</datalist>", "</dd>", "</del>", "</details>", "</dfn>", "</dialog>",
    "</div>", "</dl>", "</dt>", "</em>",
    "</fieldset>", "</figcaption>", "</figure>", "</footer>", "</form>",
    "</h1>", "</h2>", "</h3>", "</h4>", "</h5>", "</h6>", "</head>", "</header>", "</html>",
    "</i>", "</iframe>", "</ins>",
    "</kbd>", "</label>", "</legend>", "</li>",
    "</main>", "</map>", "</mark>", "</meter>",
    "</nav>", "</noscript>",
    "</object>", "</ol>", "</optgroup>", "</option>", "</output>",
    "</p>", "</picture>", "</pre>", "</progress>",
    "</q>", "</rp>", "</rt>", "</ruby>",
    "</s>", "</samp>", "</script>", "</section>", "</select>", "</small>",
    "</span>", "</strong>", "</style>", "</sub>", "</summary>", "</sup>", "</svg>",
    "</table>", "</tbody>", "</td>", "</template>", "</textarea>", "</tfoot>", "</th>", "</thead>",
    "</time>", "</title>", "</tr>",
    "</u>", "</ul>",
    "</var>", "</video>",

    /* Self-closing/void elements */
    "<area/>", "<base/>", "<br/>", "<col/>", "<embed/>", "<hr/>", "<img/>", "<input/>",
    "<link/>", "<meta/>", "<param/>", "<source/>", "<track/>", "<wbr/>",

    /* Doctype */
    "<!DOCTYPE>",
    NULL};

/* React (JSX) */
char *REACT_HL_extensions[] = {".jsx", ".tsx", NULL};
char *REACT_HL_keywords[] = {
    /* React Components */
    "Component|", "PureComponent|", "memo|", "Fragment|", "StrictMode|", "Suspense|",
    "lazy|", "Profiler|", "Children|", "createRef|", "forwardRef|",

    /* React Hooks */
    "useState|", "useEffect|", "useContext|", "useReducer|", "useCallback|",
    "useMemo|", "useRef|", "useImperativeHandle|", "useLayoutEffect|",
    "useDebugValue|",

    /* React APIs */
    "createContext|", "createElement|", "cloneElement|", "createFactory|",
    "isValidElement|", "ReactDOM|", "ReactDOMServer|",

    /* React Events */
    "onClick|", "onContextMenu|", "onDoubleClick|", "onDrag|", "onDragEnd|",
    "onDragEnter|", "onDragExit|", "onDragLeave|", "onDragOver|", "onDragStart|",
    "onDrop|", "onMouseDown|", "onMouseEnter|", "onMouseLeave|", "onMouseMove|",
    "onMouseOut|", "onMouseOver|", "onMouseUp|", "onSelect|", "onTouchCancel|",
    "onTouchEnd|", "onTouchMove|", "onTouchStart|", "onScroll|", "onWheel|",
    "onCopy|", "onCut|", "onPaste|", "onKeyDown|", "onKeyPress|", "onKeyUp|",
    "onFocus|", "onBlur|", "onChange|", "onInput|", "onInvalid|", "onSubmit|",

    /* JSX-specific */
    "className|", "htmlFor|", "dangerouslySetInnerHTML|",
    NULL};

/* Vue.js */
char *VUE_HL_extensions[] = {".vue", NULL};
char *VUE_HL_keywords[] = {
    /* Vue Directives */
    "v-text|", "v-html|", "v-show|", "v-if|", "v-else|", "v-else-if|", "v-for|",
    "v-on|", "v-bind|", "v-model|", "v-slot|", "v-pre|", "v-cloak|", "v-once|",

    /* Vue Events */
    "@click|", "@input|", "@change|", "@submit|", "@keydown|", "@keyup|", "@mouseover|",
    "@mousemove|", "@mouseleave|", "@focus|", "@blur|", "@scroll|", "@load|",

    /* Vue Special Attributes */
    ":key|", ":class|", ":style|", ":ref|", ":is|", ":slot|", ":props|",

    /* Vue Options */
    "data|", "props|", "computed|", "methods|", "watch|", "components|", "filters|",
    "directives|", "mixins|", "extends|", "provide|", "inject|", "template|",
    "render|", "el|", "name|", "functional|", "model|", "propsData|",

    /* Vue Lifecycle Hooks */
    "beforeCreate|", "created|", "beforeMount|", "mounted|", "beforeUpdate|",
    "updated|", "activated|", "deactivated|", "beforeDestroy|", "destroyed|",
    "errorCaptured|", "serverPrefetch|",

    /* Vue API */
    "Vue.directive|", "Vue.filter|", "Vue.component|", "Vue.mixin|",
    "Vue.use|", "Vue.extend|", "Vue.set|", "Vue.delete|", "Vue.nextTick|",
    "Vue.observable|", "Vue.version|",

    /* Vue Router */
    "router-link|", "router-view|", "$route|", "$router|",
    NULL};

/* Angular */
char *ANGULAR_HL_extensions[] = {".component.ts", NULL};
char *ANGULAR_HL_keywords[] = {
    /* Angular Decorators */
    "@Component|", "@Directive|", "@Pipe|", "@Injectable|", "@NgModule|",
    "@Input|", "@Output|", "@HostBinding|", "@HostListener|", "@ViewChild|",
    "@ViewChildren|", "@ContentChild|", "@ContentChildren|",

    /* Angular Directives */
    "*ngIf|", "*ngFor|", "*ngSwitch|", "*ngSwitchCase|", "*ngSwitchDefault|",
    "[ngClass]|", "[ngStyle]|", "[ngModel]|", "[ngTemplateOutlet]|",
    "ng-template|", "ng-container|", "ng-content|",

    /* Angular Lifecycle Hooks */
    "ngOnChanges|", "ngOnInit|", "ngDoCheck|", "ngAfterContentInit|",
    "ngAfterContentChecked|", "ngAfterViewInit|", "ngAfterViewChecked|",
    "ngOnDestroy|",

    /* Angular Services */
    "HttpClient|", "HttpHandler|", "HttpInterceptor|", "HttpClientModule|",
    "Router|", "ActivatedRoute|", "RouterModule|", "Location|",

    /* Angular Pipes */
    "async|", "date|", "currency|", "decimal|", "percent|", "json|", "lowercase|",
    "uppercase|", "titlecase|", "slice|", "i18nSelect|", "i18nPlural|",

    /* Angular Forms */
    "FormGroup|", "FormControl|", "FormArray|", "FormBuilder|", "Validators|",
    "ReactiveFormsModule|", "FormsModule|",

    /* Angular Testing */
    "TestBed|", "ComponentFixture|", "fakeAsync|", "tick|", "async|", "inject|",
    NULL};

/* Svelte */
char *SVELTE_HL_extensions[] = {".svelte", NULL};
char *SVELTE_HL_keywords[] = {
    /* Svelte Directives */
    "on:|", "bind:|", "class:|", "use:|", "transition:|", "in:|", "out:|",
    "animate:|", "let:|", "export|", "as|", "in|", "out|", "animate|",

    /* Svelte Events */
    "onclick|", "oninput|", "onchange|", "onsubmit|", "onkeydown|", "onkeyup|",
    "onmouseover|", "onmousemove|", "onmouseleave|", "onfocus|", "onblur|",
    "onscroll|", "onload|",

    /* Svelte Actions */
    "use:action|", "use:enhance|",

    /* Svelte Transitions */
    "transition:fade|", "transition:slide|", "transition:blur|", "transition:fly|",
    "transition:scale|", "transition:draw|", "transition:crossfade|",

    /* Svelte Stores */
    "writable|", "readable|", "derived|", "get|", "subscribe|", "set|", "update|",

    /* Svelte Special Elements */
    "svelte:self|", "svelte:component|", "svelte:window|", "svelte:body|",
    "svelte:head|", "svelte:options|",

    /* Svelte Runtime */
    "beforeUpdate|", "afterUpdate|", "onMount|", "onDestroy|", "tick|",
    "createEventDispatcher|", "getContext|", "setContext|", "hasContext|",
    "all|", "setTimeout|", "setInterval|", "requestAnimationFrame|",
    NULL};

/* Here we define an array of syntax highlights by extensions, keywords,
 * comments delimiters and flags. */
struct editorSyntax HLDB[] = {
    {/* C / C++ */
     C_HL_extensions,
     C_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Python */
     PYTHON_HL_extensions,
     PYTHON_HL_keywords,
     "#", "", "",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Java */
     JAVA_HL_extensions,
     JAVA_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* JavaScript */
     JS_HL_extensions,
     JS_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* TypeScript */
     TS_HL_extensions,
     TS_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* C# */
     CSHARP_HL_extensions,
     CSHARP_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* PHP */
     PHP_HL_extensions,
     PHP_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Ruby */
     RUBY_HL_extensions,
     RUBY_HL_keywords,
     "#", "", "",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Swift */
     SWIFT_HL_extensions,
     SWIFT_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* SQL */
     SQL_HL_extensions,
     SQL_HL_keywords,
     "--", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Rust */
     RUST_HL_extensions,
     RUST_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Dart */
     DART_HL_extensions,
     DART_HL_keywords,
     "//", "/*", "*/",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},
    {/* Shell */
     SHELL_HL_extensions,
     SHELL_HL_keywords,
     "#", "", "",
     HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},

    /* HTML */
    {
        HTML_HL_extensions,
        HTML_HL_keywords,
        "<!--",
        "",
        "-->",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},

    /* React */
    {
        REACT_HL_extensions,
        REACT_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},

    /* Vue */
    {
        VUE_HL_extensions,
        VUE_HL_keywords,
        "<!--", "", "-->",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},

    /* Angular */
    {
        ANGULAR_HL_extensions,
        ANGULAR_HL_keywords,
        "//", "/*", "*/",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS},

    /* Svelte */
    {
        SVELTE_HL_extensions,
        SVELTE_HL_keywords,
        "<!--", "", "-->",
        HL_HIGHLIGHT_STRINGS | HL_HIGHLIGHT_NUMBERS}

};

#define HLDB_ENTRIES (sizeof(HLDB) / sizeof(HLDB[0]))

/*** prototypes ***/

void editorRefreshScreen();
char *editorPrompt(char *prompt, void (*callback)(char *, int));

/*** terminal ***/
void die(const char *s) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(s);
    exit(1);
}

void disableRawMode() {
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcgetattr");
}
 
void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcgetattr");

    atexit(disableRawMode);

    struct termios raw = E.orig_termios;

    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP| IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcgetattr");
}

int editorReadKey() {
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
                        case '1' : return HOME_KEY;
                        case '3' : return DEL_KEY;                        
                        case '4' : return END_KEY;
                        case '5' : return PAGE_UP;
                        case '6' : return PAGE_DOWN;
                        case '7' : return HOME_KEY;
                        case '8' : return END_KEY;
                    }
                }
        } else {
            switch (seq[1]) {
            case 'A': return ARROW_UP;
            case 'B': return ARROW_DOWN;
            case 'C': return ARROW_RIGHT;
            case 'D': return ARROW_LEFT;
            case 'H' : return HOME_KEY;
            case 'F' : return END_KEY;
            }
        }
    } else if (seq[0] == 'O') {
        switch (seq[1]) {
            case 'H' : return HOME_KEY;
            case 'F' : return END_KEY;
        }
    }

        return '\x1b';
    }
    else {
        return c;
    }
}

int getCursorPosition(int *rows, int *cols) {
    char buf[32];
    unsigned int i = 0;
    
    if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;

    while (i < sizeof(buf) - 1)
    {
        if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
        if (buf[i] == 'R') break;
        i++;
    }
    buf[i] = '\0';

    if (buf[0] != '\x1b' || buf[1] != '[') return -1;
    if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

    editorReadKey();
    return -1;
}

int getWindowSize(int *rows, int *cols) {
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
        return getCursorPosition(rows, cols);
    }
    else
    {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
        return 0;
    }
}

/*** syntax highlighting ***/
int is_separator(int c) {
    return isspace(c) || c == '\0' || strchr(",.()+-/*=~%<>[];", c) != NULL;
}

void editorUpdateSyntax(erow *row) {
    row->hl = realloc(row->hl, row->rsize);
    if (row->rsize > 0) {
        memset(row->hl, HL_NORMAL, row->rsize);
    }

    if (E.syntax == NULL) return;

    char **keywords = E.syntax->keywords;
    
    char *scs = E.syntax->singleline_comment_start;
    char *mcs = E.syntax->multiline_comment_start;
    char *mce = E.syntax->multiline_comment_end;

    int scs_len = scs ? strlen(scs) : 0;
    int mcs_len = mcs ? strlen(mcs) : 0;
    int mce_len = mce ? strlen(mce) : 0;

    int prev_sep = 1;
    int in_string = 0;
    int in_comment = (row->idx > 0 && E.row[row->idx - 1].hl_open_comment);

    int i = 0;
    while (i < row->rsize) {
        char c = row->render[i];
        unsigned char prev_hl = (i > 0) ? row->hl[i - 1] : HL_NORMAL;

        if (scs_len && !in_string && !in_comment) {
            if (!strncmp(&row->render[i], scs, scs_len)) {
                memset(&row->hl[i], HL_COMMENT, row->rsize - i);
                break;
            }
        }

        if (mcs_len && mce_len && !in_string) {
            if (in_comment) {
                row->hl[i] = HL_MLCOMMENT;
                if (!strncmp(&row->render[i], mce, mce_len)) {
                    memset(&row->hl[i], HL_MLCOMMENT, mce_len);
                    i += mce_len;
                    in_comment = 0;
                    prev_sep = 1;
                    continue;
                }
                else {
                    i++;
                }
            }
            else if (!strncmp(&row->render[i], mcs, mcs_len)) {
                memset(&row->hl[i], HL_MLCOMMENT, mcs_len);
                i += mcs_len;
                in_comment = 1;
                continue;
            }
        }

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
            }
            else {
                if (c == '"' || c == '\'') {
                    in_string = c;
                    row->hl[i] = HL_STRING;
                    i++;
                    continue;
                }
            }
        }

        if (E.syntax->flags & HL_HIGHLIGHT_NUMBERS) {
            if ((isdigit(c) && (prev_sep || prev_hl == HL_NUMBER)) ||
                (c == '.' && prev_hl == HL_NUMBER)) {
                row->hl[i] = HL_NUMBER;
                i++;
                prev_sep = 0;
                continue;
            }
        }

        if (prev_sep) {
            int j;
            for (j = 0; keywords[j]; j++)
            {
                int klen = strlen(keywords[j]);
                int kw2 = keywords[j][klen - 1] == '|';
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
        case HL_KEYWORD1 : return 33;
        case HL_KEYWORD2 : return 32;
        case HL_STRING: return 35;
        case HL_NUMBER: return 31;
        case HL_MATCH: return 34;
        default: return 37;
    }
}

void editorSelectSyntaxHighlight() {
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

                int filerow;
                for (filerow = 0; filerow < E.numrows; filerow++) {
                    editorUpdateSyntax(&E.row[filerow]);
                }

                return;
            }
            i++;
    }
  }
}

/*** row operation ***/
int editorRowCXToRx(erow *row, int cx) {
    int rx = 0;
    int j;
    for (j = 0; j < cx; j++) {
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
    int j;
    for (j = 0; j < row->size; j++)
        if (row->chars[j] == '\t') tabs++;

    free(row->render);
    row->render = malloc(row->size + tabs*(FILO_TAB_STOP - 1) + 1);

    int idx = 0;
    for (j = 0; j < row->size; j++) {
        if (row->chars[j] == '\t') {
            row->render[idx++] = ' ';
            while (idx % FILO_TAB_STOP != 0) row->render[idx++] = ' ';
        }
        else {
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
    for (int j = at + 1; j <=E.numrows; j++) E.row[j].idx++;
    
    E.row[at].idx = at;

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
    memmove(&row->chars[at], &row->chars[at + 1],  row->size - at);
    row->size--;
    editorUpdateRow(row);
    E.dirty++;
}

/*** editor operations ***/
void editorInsertChar(int c) {
    if (E.cy == E.numrows) {
        editorInsertRow(E.numrows, "", 0);
    }
    editorRowInsertChar(&E.row[E.cy], E.cx, c);
    E.cx++;
}

void editorInsertNewline() {
    if (E.cx == 0) {
        editorInsertRow(E.cy, "", 0);
    }
    else {
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

void editorDelChar() {
    if (E.cy == E.numrows) return;
    if (E.cx == 0 && E.cy == 0) return;

    erow *row = &E.row[E.cy];
    if (E.cx > 0) {
        editorRowDelChar(row, E.cx - 1);
        E.cx--;
    }
    else {
        E.cx = E.row[E.cy - 1].size;
        editorRowAppendString(&E.row[E.cy - 1], row->chars, row->size);
        editorDelRow(E.cy); //Fix the Phantom lines bug
        E.cy--;
    }
}

/*** file i/o ***/
char *editorRowsToString(int *buflen) {
    int totlen = 0;
    int j;
    for (j =0; j < E.numrows; j++)
        totlen += E.row[j].size + 1;
    *buflen = totlen;

    char *buf = malloc(totlen);
    char *p = buf;
    for (j = 0; j < E.numrows; j++) {
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
    if (!fp) die("fopen");

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

void editorSave() {
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

    int fd = open(E.filename, O_RDWR | O_CREAT, 0664);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                close(fd);
                free(buf);
                E.dirty = 0;
                editorSetStatusMessage("%d bytes written to disk", len);
                return;
            }
        }
        close(fd);
    }

    free(buf);
    editorSetStatusMessage("Can't save! I/O error: %s", strerror(errno));
}

/*** find function ***/
void editorFindCallback(char *query, int key) {
    static int last_match = -1;
    static int direction = 1;

    static int saved_hl_line;
    static char *saved_hl = NULL;

    if (saved_hl) {
        memcpy(E.row[saved_hl_line].hl, saved_hl, E.row[saved_hl_line].rsize);
        free(saved_hl);
        saved_hl = NULL;
    }

    if (key == '\r' || key == '\x1b') {
        last_match = -1;
        direction = 1;
        return;
    }
    else if (key == ARROW_RIGHT || key == ARROW_DOWN) {
        direction = 1;
    }
    else if (key == ARROW_LEFT || key == ARROW_UP) {
        direction = -1;
    }
    else {
        last_match = -1;
        direction = 1;
    }

    if (last_match == -1) direction = 1;
    int current = last_match;
    int i;
    for (i = 0; i < E.numrows; i++) {
        current += direction;
        if (current == -1) current = E.numrows -1;
        else if (current == E.numrows) current = 0;

        erow *row = &E.row[current];
        char *match = strstr(row->render, query);
        if (match) {
            last_match = current;
            E.cy = current;
            E.cx = editorRowRxToCx(row, match - row->render);
            E.rowoff = E.numrows;

            saved_hl_line = current;
            saved_hl = malloc(row->rsize);
            memcpy(saved_hl, row->hl, row->rsize);
            memset(&row->hl[match - row->render], HL_MATCH, strlen(query));
            break;
        }
    }
}

void editorFind() {
    int saved_cx = E.cx;
    int saved_cy = E.cy;
    int saved_coloff = E.coloff;
    int saved_rowoff = E.rowoff;

    char *query = editorPrompt("Search %s (Use ESC/Arrows/Enter)", editorFindCallback);
    if (query) {
        free(query);
    }
    else {
        E.cx = saved_cx;
        E.cy = saved_cy;
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
    ab-> len += len;
}

void abFree(struct abuf *ab) {
    free(ab->b);
}

/*** output ***/
void editorScroll() {
    E.rx = 0;
    if (E.cy < E.numrows) {
        E.rx = editorRowCXToRx(&E.row[E.cy], E.cx);
    }

    if (E.cy < E.rowoff) {
        E.rowoff = E.cy;
    }
    if (E.cy >= E.rowoff + E.screenrows) {
        E.rowoff = E.cy - E.screenrows + 1;
    }
    if (E.rx < E.coloff) {
        E.coloff = E.rx;
    }
    if (E.rx >= E.coloff +  E.screencols) {
        E.coloff = E.rx - E.screencols + 1;
    }
}

void editorDrawRows(struct abuf *ab) {
    int y;
    for (y = 0; y < E.screenrows; y++) {
        int filerow = y + E.rowoff;
        if (filerow >= E.numrows) {
            if (E.numrows == 0 && y == E.screenrows / 3) {
                char welcome[80];
                int welcomelen = snprintf(welcome, sizeof(welcome),
                    "Filo editor -- version %s", FILO_VERSION);
                if (welcomelen > E.screencols) welcomelen = E.screencols;
                int padding = (E.screencols - welcomelen) / 2;
                if (padding) {
                    abAppend(ab, "~", 1);
                    padding--;
                }
                while (padding--) abAppend(ab, " ", 1);
                abAppend(ab, welcome, welcomelen);
            }
            else {
            abAppend(ab, "~", 1);
            }
        }
        else {
            int len = E.row[filerow].rsize - E.coloff;
            if (len < 0) len = 0;
            if (len > E.screencols) len = E.screencols;
            char *c = &E.row[filerow].render[E.coloff];
            unsigned char *hl = &E.row[filerow].hl[E.coloff];
            int current_color = -1;
            int j;
            for (j = 0; j < len; j++) {
                if (iscntrl(c[j])) {
                    char sym = (c[j] <= 26) ? '@' + c[j] : '?';
                    abAppend(ab, "\x1b[7m", 4);
                    abAppend(ab, &sym, 1);
                    abAppend(ab, "\x1b[m", 3);
                    if (current_color != -1) {
                        char buf[16];
                        int clen = snprintf(buf, sizeof(buf), "\x1b[%dm", current_color);
                        abAppend(ab, buf, clen);
                    }
                }
                else if (hl[j] == HL_NORMAL) { 
                    if (current_color != -1) {
                        abAppend(ab, "\x1b[39m", 5);
                        current_color = -1;
                    }
                    abAppend(ab, &c[j], 1);
                }
                else {
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

        abAppend(ab,"\x1b[K", 3);
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
    if (len > E.screencols) len =  E.screencols;
    abAppend(ab, status, len);
    while (len < E.screencols) {
        if (E.screencols - len == rlen) {
            abAppend(ab, rstatus, rlen);
            break;
        }
        else {
            abAppend(ab, " ", 1);
            len++;
        }
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

void editorRefreshScreen() {
    editorScroll();

    struct abuf ab = ABUF_INIT;

    abAppend(&ab, "\x1b[?25l", 6);
    abAppend(&ab, "\x1b[H", 3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMessageBar(&ab);

    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", (E.cy - E.rowoff) + 1,
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
    char *buf = malloc(bufsize);

    size_t buflen = 0;
    buf[0] = '\0';

    while (1) {
        editorSetStatusMessage(prompt, buf);
        editorRefreshScreen();

        int c = editorReadKey();
        if (c == DEL_KEY ||  c == CTRL_KEY('h') || c == BACKSPACE) {
            if (buflen != 0) buf[--buflen] = '\0';
        }
        else if (c == '\x1b') {
            editorSetStatusMessage("");
            if (callback) callback(buf, c);
            free(buf);
            return NULL;
        }
        else if (c == '\r') {
            if (buflen != 0) {
                editorSetStatusMessage("");
                if (callback) callback(buf, c);
                return buf;
            }
        }
        else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
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
                }
            else if (E.cy > 0) {
                E.cy--;
                E.cx = E.row[E.cy].size;
            }
            break;
        case ARROW_RIGHT:
            if (row && E.cx < row->size) {
                E.cx++;
            }
            else if (row && E.cx == row->size) {
                E.cy++;
                E.cx = 0;
            }
            break;
         case ARROW_UP:
            if (E.cy != 0) {
                E.cy--;
                }
            break;
        case ARROW_DOWN:
            if (E.cy < E.numrows) {
                E.cy++;
            }
            break;
    }

    row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
    int rowlen = row ? row->size : 0;
    if (E.cx > rowlen) {
        E.cx = rowlen;
    }
}

void editorProcessKeypress() {
    static int quit_times = FILO_QUIT_TIMES;

    int c = editorReadKey();

    switch (c) {
        case '\r':
            editorInsertNewline();
            break;

        case CTRL_KEY('q'):
        if (E.dirty && quit_times > 0) {
            editorSetStatusMessage("WARNING!!! File has unsaved changes. "
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
            if (c == PAGE_UP) {
                E.cy = E.rowoff;
            }
            else if (c == PAGE_DOWN) {
                E.cy = E.rowoff + E.screenrows - 1;
                if  (E.cy > E.numrows) E.cy = E.numrows;
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

        case CTRL_KEY('l'):
        case '\x1b':
            break;

        default:
            editorInsertChar(c);
            break;
    }

    quit_times = FILO_QUIT_TIMES;
}

/*** init ***/
void initEditor() {
    E.cx = 0;
    E.cy = 0;
    E.rx = 0;
    E.rowoff = 0;
    E.coloff = 0;
    E.numrows = 0;
    E.row = NULL;
    E.dirty = 0;
    E.filename = NULL;
    E.statusmsg[0] = '\0';
    E.statusmsg_time = 0;
    E.syntax = NULL;

    if (getWindowSize(&E.screenrows, &E.screencols) == -1) die ("getWindowSize");
    E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
    enableRawMode();
    initEditor();
    if (argc >= 2) {
        editorOpen(argv[1]);
    }

    editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find");

    while (1)
    {
        editorRefreshScreen();
        editorProcessKeypress();
    }
    return 0;
}