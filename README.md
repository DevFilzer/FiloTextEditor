# Filó

A lightweight terminal text editor written in C.

Filó is a fast, minimal, terminal-based code editor inspired by classic text editors like Kilo, with modern quality-of-life features such as syntax highlighting, search, auto-pair completion, and support for multiple programming languages.

---

## Features

* Lightweight and fast
* Syntax highlighting
* Multi-language support
* Incremental search
* Auto-closing brackets and quotes
* Paste mode detection
* Window resize support
* Status bar and live messages
* File creation and editing
* Line deletion shortcut

---

## Supported Languages

Filó includes syntax highlighting for:

* C / C++
* Python
* Java
* JavaScript
* TypeScript
* C#
* PHP
* Ruby
* Swift
* Rust
* SQL
* Dart
* Shell scripts
* HTML
* JSX / TSX
* Vue
* Svelte

---

## Usage

Open or create a file:

```bash
./filo filename.c
```

Example:

```bash
./filo main.c
```

If the file does not exist, Filó will create it when saved.

---

## Keyboard Shortcuts

| Shortcut       | Action              |
| -------------- | ------------------- |
| Ctrl-S         | Save file           |
| Ctrl-Q         | Quit                |
| Ctrl-F         | Search              |
| Ctrl-H         | Delete current line |
| Backspace      | Delete character    |
| Enter          | New line            |
| Arrow Keys     | Move cursor         |
| Page Up / Down | Scroll              |
| Home / End     | Jump to line edges  |

---

## Inspiration

Filó is inspired by the excellent tutorial:

**Build Your Own Text Editor**

by Salvatore Sanfilippo

---

## License

MIT License

---
