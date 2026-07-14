# Markdown Renderer

A lightweight tool that parses Markdown files into HTML and serves them locally using a built-in HTTP server.
I made this tool to help me read through documentation of projects written in markdown format, when I working 
on remote servers via ssh tty access only. Its actually irittating copy pasting into browser based parsers.
This can be used when using Vi, like run in the background while editing. I made it to solve my problem, if it 
does yours too, feel free to contribute to it.

## Features
- **Markdown Parsing:** Powered by [cmark](https://github.com/common-mark/cmark) for reliable CommonMark compliance.
- **Local Rendering:** Hosts the rendered HTML directly through a built-in HTTP server so you can view files in your browser.

## Getting Started

### Prerequisites
- GCC or any standard C compiler
- Make build system
- [cmark](https://github.com/common-mark/cmark) development libraries installed on your system.

### Installation & Compilation

1. **Clone the repository:**
   ```bash
   git clone https://github.com/r1shavd/md-renderer.git
   cd md-renderer
   ```

2. **Build the project:**
   ```bash
   make

    # Or, use this to compile with mongoose version
   make mongoose
   ```

## Usage

To start the built-in HTTP server and render your Markdown file, run the following command:

```bash
./bin/md-renderer <markdown-file.md> --ip-address <host-address> --port <port>
```

**Example:**
```bash
./bin/md-renderer README.md --port 8000
```

Once running, open your web browser and navigate to `http://127.0.0.1:8000` to view your rendered document.

Use the `--help` flag for additional configuration options:
```bash
./bin/md-renderer --help
```

## Dependencies
* [common-mark/cmark](https://github.com/common-mark/cmark) — Required for parsing Markdown files to HTML.
* [cesanta/mongoose](https://github.com/cesanta/mongoose) - Required for hosting the parsed markdown files. Use the _make mongoose_ version to compile this version. Other versions, reduce this dependencies sacrifising stability to use native SOCKET HTTP server.
