# Sbix

<div align="center">
  
[![Static Badge](https://img.shields.io/badge/sponsor-gray?logo=githubsponsors&style=for-the-badge&link=https%3A%2F%2Fapp.lava.top%2Fproducts%2F7ea4f127-2f35-4ee6-8ac9-5acf6d126ea2%2F82ae2840-1223-44a5-8794-15756255d60b%3Fcurrency%3DUSD%26amp%3BdomainId%3D215e25f7-4e79-4264-9633-5ad0512e8e37%26amp%3BdomainName%3Dgithub.com)](https://app.lava.top/products/7ea4f127-2f35-4ee6-8ac9-5acf6d126ea2/82ae2840-1223-44a5-8794-15756255d60b?currency=USD&domainId=215e25f7-4e79-4264-9633-5ad0512e8e37&amp;domainName=github.com)
![Visitors](https://api.visitorbadge.io/api/visitors?path=https%3A%2F%2Fgithub.com%2Ftezevsk%2FSbix&countColor=%23263759)
![GitHub Repo stars](https://img.shields.io/github/stars/tezevsk/Sbix?style=for-the-badge)
![License](https://img.shields.io/badge/LICENSE-APACHE_2.0-blue?style=for-the-badge)
![Install](https://img.shields.io/badge/INSTALL-green?style=for-the-badge&link=https%3A%2F%2Fgithub.com%2Ftezevsk%2FSbix%2Freleases%2Flatest)

</div>

---

Compiler for the **Sbix** programming language

**Sbix** -- is a fast compiled programming language designed by Stanislav Maryin (also known under personal brand **tezevsk**)

Writen in C++ using LLVM

## What's that
Let's give it a **quick breakdown**:
#### Hello world
```sbix
bring commandline -> cm

function helloWorld() {
    cm.print("Hello world")
}

@entry

helloWorld()
```
#### Statements, Loops
while
```sbix
while (true) {
    // Loop body
}
```
if
```sbix
if (green) {
    // Green light
} else if (Yellow) {
    // Yellow light
} else {
    // Red light
}
```
for
```sbix
// Second "->" (step) is optional
for (i -> 0:10 -> 1) {
    // To be honest, I'm so proud of this syntax
    // Loop
}
```

#### Need more? [Check the Docs!](docs/)
Do WHAT you **want**, HOW you **want**.
Hope you'll like it

## Requirements
Before building, insure you have following dependencies installed

* **C++20** Compatable Compiler (Clang *16*+, GCC *13*+, MSVC *17*+)
* **CMake *3.20*** or newer
* **LLVM *22* (devel)** Binnaries and Headers

## Quick Start
### Building from the source
```bash
git clone https://github.com/tezevsk/Sbix.git
cd ./Sbix

mkdir build
cmake -B build -DCMAKE_BUILD_TYPE=Release .
cmake --build build
```

### Using Installer
alternatively you can use

```bash
curl -fsSL https://raw.githubusercontent.com/tezevsk/Sbix/refs/heads/main/scripts/fastinstall.sh | bash
```

⚠️ &nbsp; Might **require** `sudo` to acces system-wide **directores**

You can *ensure* script *safety* [Here](scripts/fastinstall.sh)

#### Or instal &emsp; [prebuilt binnaries](github.com/tezevsk/Sbix/releases)</a>

### Compiling Programs
To get executable you need:
* Sbix Compiler
* Linker (ex. clang)
  ![Result of compiled program](.data/image.png)
  ```bash
  sbix examples/Hello42.sbix
  clang output.o -o Hello42
  ./Hello42
  ```

## Tested on

* **OS:** Fedora 44 Linux
* **Kernel** 7.0.11-200.fc44.x86_64

## Contributing

Contributions, Issues and Pull requests are **welcome**! Every small step lead to larger results. Help it grow and prosper

## License

Code licensed under *Apache License 2.0* Distributed "AS IS" Absolute accuracy and (absolute) safety of work are NOT guaranteed. For further [info](LICENSE)
