# HuiwenMCP for Harmony

[简体中文](README.md) · [English](README.en.md)

A document reading, Markdown editing and format conversion project for **OpenHarmony / HarmonyOS PC**, built with **Qt, ArkTS, CEF, PDFium and Python HNP**.

Read source documents, compare PDF-to-Markdown results side by side, edit Markdown, and resume work from recent tasks.

## Screenshots

### HarmonyOS PC home

![HuiwenMCP home with reading, editing, conversion and recent workspaces](docs/images/home-harmonyos-pc.png)

### PDF-to-Markdown workspace

![PDF and Markdown side-by-side workspace with file tree, outline and assistant panel](docs/images/pdf-to-markdown-workspace.png)

These screenshots show a running version supplied by the maintainer. The assistant panel currently provides a conversation UI and model configuration; a live model backend is not connected yet.

## Features and scope

| Area | Current scope |
| --- | --- |
| Document reading | PDF, Markdown and DOCX preview; legacy DOC is converted to a temporary DOCX for preview. |
| PDF to Markdown | Text-based PDF conversion, with source and output displayed side by side. |
| Synchronized scrolling | PDF and Markdown follow page position or overall progress; independent scrolling is also available. |
| Markdown editing | Editing, saving and unsaved-change confirmation. |
| Workspaces | Input/output files, document outlines and recent task restoration. |
| Home appearance | Light, dark and system modes; Qt workspace colors are managed separately. |
| Document assistant | UI and configuration only; live model calls and attachments are still in development. |

Word editing, PDF editing, OCR and broader batch conversion are planned. Compatibility depends on document content and device environment; screenshots do not validate every format or workflow.

## Development

Clone the repository and initialize its pinned source dependencies:

```sh
git clone --recurse-submodules https://github.com/duduhel/HuiwenMCP.git
cd HuiwenMCP
```

For an existing checkout:

```sh
git submodule sync --recursive
git submodule update --init --recursive
```

See the [build instructions](README.md#本地构建) for DevEco Studio, SDK, Python runtime and local signing setup, and [dependency maintenance](third_party/README.md) for source ownership. Desktop Windows/macOS builds and installation packages are not claimed to be validated.

## Contributing

Feedback, documentation, sanitized sample files and focused pull requests are welcome. Read [CONTRIBUTING.md](CONTRIBUTING.md) before changing application code or source submodules.

## License

Project-owned code is licensed under [PolyForm Noncommercial 1.0.0](LICENSE). This is source-available software with noncommercial license restrictions. Third-party code, dependencies and assets retain their own licenses. Refer to the license text for permitted uses.
